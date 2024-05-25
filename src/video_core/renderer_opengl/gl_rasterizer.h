// SPDX-FileCopyrightText: 2015 Citra Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <cstddef>
#include <optional>

#include <boost/container/static_vector.hpp>

#include <glad/glad.h>

#include "common/common_types.h"
#include "video_core/control/channel_state_cache.h"
#include "video_core/engines/maxwell_dma.h"
#include "video_core/rasterizer_interface.h"
#include "video_core/renderer_opengl/blit_image.h"
#include "video_core/renderer_opengl/gl_blit_screen.h"
#include "video_core/renderer_opengl/gl_buffer_cache.h"
#include "video_core/renderer_opengl/gl_device.h"
#include "video_core/renderer_opengl/gl_fence_manager.h"
#include "video_core/renderer_opengl/gl_query_cache.h"
#include "video_core/renderer_opengl/gl_shader_cache.h"
#include "video_core/renderer_opengl/gl_texture_cache.h"

namespace Core::Memory {
class Memory;
}

namespace Core::Frontend {
class EmuWindow;
}

namespace Tegra {
class MemoryManager;
}

namespace OpenGL {

struct FramebufferTextureInfo;
struct ShaderEntries;

struct BindlessSSBO {
    GLuint64EXT address;
    GLsizei length;
    GLsizei padding;
};
static_assert(sizeof(BindlessSSBO) * CHAR_BIT == 128);

class AccelerateDMA : public Tegra::Engines::AccelerateDMAInterface {
public:
    explicit AccelerateDMA(BufferCache& buffer_cache, TextureCache& texture_cache);

    bool BufferCopy(GPUVAddr src_address, GPUVAddr dest_address, u64 amount) override;

    bool BufferClear(GPUVAddr src_address, u64 amount, u32 value) override;

    bool ImageToBuffer(const Tegra::DMA::ImageCopy& copy_info, const Tegra::DMA::ImageOperand& src,
                       const Tegra::DMA::BufferOperand& dst) override;

    bool BufferToImage(const Tegra::DMA::ImageCopy& copy_info, const Tegra::DMA::BufferOperand& src,
                       const Tegra::DMA::ImageOperand& dst) override;

private:
    template <bool IS_IMAGE_UPLOAD>
    bool DmaBufferImageCopy(const Tegra::DMA::ImageCopy& copy_info,
                            const Tegra::DMA::BufferOperand& src,
                            const Tegra::DMA::ImageOperand& dst);

    BufferCache& buffer_cache;
    TextureCache& texture_cache;
};

class RasterizerOpenGL : public VideoCore::RasterizerInterface,
                         protected VideoCommon::ChannelSetupCaches<VideoCommon::ChannelInfo> {
public:
    explicit RasterizerOpenGL(Core::Frontend::EmuWindow& emu_window_, Tegra::GPU& gpu_,
                              Tegra::MaxwellDeviceMemoryManager& device_memory_,
                              const Device& device_, ProgramManager& program_manager_,
                              StateTracker& state_tracker_);
    ~RasterizerOpenGL() override;

    void Draw(bool is_indexed, u32 instance_count) override;
    void DrawIndirect() override;
    void DrawTexture() override;
    void Clear(u32 layer_count) override;
    void DispatchCompute() override;
    void ResetCounter(VideoCommon::QueryType type) override;
    void Query(GPUVAddr gpu_addr, VideoCommon::QueryType type,
               VideoCommon::QueryPropertiesFlags flags, u32 payload, u32 subreport) override;
    void BindGraphicsUniformBuffer(size_t stage, u32 index, GPUVAddr gpu_addr, u32 size) override;
    void DisableGraphicsUniformBuffer(size_t stage, u32 index) override;
    void FlushAll() override;
    void FlushRegion(DAddr addr, u64 size,
                     VideoCommon::CacheType which = VideoCommon::CacheType::All) override;
    bool MustFlushRegion(DAddr addr, u64 size,
                         VideoCommon::CacheType which = VideoCommon::CacheType::All) override;
    VideoCore::RasterizerDownloadArea GetFlushArea(PAddr addr, u64 size) override;
    void InvalidateRegion(DAddr addr, u64 size,
                          VideoCommon::CacheType which = VideoCommon::CacheType::All) override;
    void OnCacheInvalidation(PAddr addr, u64 size) override;
    bool OnCPUWrite(PAddr addr, u64 size) override;
    void InvalidateGPUCache() override;
    void UnmapMemory(DAddr addr, u64 size) override;
    void ModifyGPUMemory(size_t as_id, GPUVAddr addr, u64 size) override;
    void SignalFence(std::function<void()>&& func) override;
    void SyncOperation(std::function<void()>&& func) override;
    void SignalSyncPoint(u32 value) override;
    void SignalReference() override;
    void ReleaseFences(bool force = true) override;
    void FlushAndInvalidateRegion(
        DAddr addr, u64 size, VideoCommon::CacheType which = VideoCommon::CacheType::All) override;
    void WaitForIdle() override;
    void FragmentBarrier() override;
    void TiledCacheBarrier() override;
    void FlushCommands() override;
    void TickFrame() override;
    bool AccelerateConditionalRendering() override;
    bool AccelerateSurfaceCopy(const Tegra::Engines::Fermi2D::Surface& src,
                               const Tegra::Engines::Fermi2D::Surface& dst,
                               const Tegra::Engines::Fermi2D::Config& copy_config) override;
    Tegra::Engines::AccelerateDMAInterface& AccessAccelerateDMA() override;
    void AccelerateInlineToMemory(GPUVAddr address, size_t copy_size,
                                  std::span<const u8> memory) override;
    void LoadDiskResources(u64 title_id, std::stop_token stop_loading,
                           const VideoCore::DiskResourceLoadCallback& callback) override;

    /// Returns true when there are commands queued to the OpenGL server.
    bool AnyCommandQueued() const {
        return num_queued_commands > 0;
    }

    void InitializeChannel(Tegra::Control::ChannelState& channel) override;

    void BindChannel(Tegra::Control::ChannelState& channel) override;

    void ReleaseChannel(s32 channel_id) override;

    void RegisterTransformFeedback(GPUVAddr tfb_object_addr) override;

    bool HasDrawTransformFeedback() override {
        return true;
    }

    std::optional<FramebufferTextureInfo> AccelerateDisplay(const Tegra::FramebufferConfig& config,
                                                            VAddr framebuffer_addr,
                                                            u32 pixel_stride);

private:
    static constexpr size_t MAX_TEXTURES = 192;
    static constexpr size_t MAX_IMAGES = 48;
    static constexpr size_t MAX_IMAGE_VIEWS = MAX_TEXTURES + MAX_IMAGES;

    template <typename Func>
    void PrepareDraw(bool is_indexed, Func&&);

    /// Syncs state to match guest's
    void SyncState();

    /// Syncs the viewport and depth range to match the guest state
    void SyncViewport();

    /// Syncs the depth clamp state
    void SyncDepthClamp();

    /// Syncs the clip enabled status to match the guest state
    void SyncClipEnabled(u32 clip_mask);

    /// Syncs the clip coefficients to match the guest state
    void SyncClipCoef();

    /// Syncs the cull mode to match the guest state
    void SyncCullMode();

    /// Syncs the primitive restart to match the guest state
    void SyncPrimitiveRestart();

    /// Syncs the depth test state to match the guest state
    void SyncDepthTestState();

    /// Syncs the stencil test state to match the guest state
    void SyncStencilTestState();

    /// Syncs the blend state to match the guest state
    void SyncBlendState();

    /// Syncs the LogicOp state to match the guest state
    void SyncLogicOpState();

    /// Syncs the the color clamp state
    void SyncFragmentColorClampState();

    /// Syncs the alpha coverage and alpha to one
    void SyncMultiSampleState();

    /// Syncs the scissor test state to match the guest state
    void SyncScissorTest();

    /// Syncs the point state to match the guest state
    void SyncPointState();

    /// Syncs the line state to match the guest state
    void SyncLineState();

    /// Syncs the rasterizer enable state to match the guest state
    void SyncRasterizeEnable();

    /// Syncs polygon modes to match the guest state
    void SyncPolygonModes();

    /// Syncs Color Mask
    void SyncColorMask();

    /// Syncs the polygon offsets
    void SyncPolygonOffset();

    /// Syncs the alpha test state to match the guest state
    void SyncAlphaTest();

    /// Syncs the framebuffer sRGB state to match the guest state
    void SyncFramebufferSRGB();

    /// Syncs vertex formats to match the guest state
    void SyncVertexFormats();

    /// Syncs vertex instances to match the guest state
    void SyncVertexInstances();

    /// Begin a transform feedback
    void BeginTransformFeedback(GraphicsPipeline* pipeline, GLenum primitive_mode);

    /// End a transform feedback
    void EndTransformFeedback();

    void QueryFallback(GPUVAddr gpu_addr, VideoCommon::QueryType type,
                       VideoCommon::QueryPropertiesFlags flags, u32 payload, u32 subreport);

    bool IsAnyFloat(Tegra::Engines::Maxwell3D::Regs::VertexAttribute attribute);

    Tegra::GPU& gpu;
    Tegra::MaxwellDeviceMemoryManager& device_memory;

    const Device& device;
    ProgramManager& program_manager;
    StateTracker& state_tracker;

    StagingBufferPool staging_buffer_pool;
    TextureCacheRuntime texture_cache_runtime;
    TextureCache texture_cache;
    BufferCacheRuntime buffer_cache_runtime;
    BufferCache buffer_cache;
    ShaderCache shader_cache;
    QueryCache query_cache;
    AccelerateDMA accelerate_dma;
    FenceManagerOpenGL fence_manager;

    BlitImageHelper blit_image;

    boost::container::static_vector<u32, MAX_IMAGE_VIEWS> image_view_indices;
    std::array<ImageViewId, MAX_IMAGE_VIEWS> image_view_ids;
    boost::container::static_vector<GLuint, MAX_TEXTURES> sampler_handles;
    std::array<GLuint, MAX_TEXTURES> texture_handles{};
    std::array<GLuint, MAX_IMAGES> image_handles{};

    /// Number of commands queued to the OpenGL driver. Reset on flush.
    size_t num_queued_commands = 0;
    bool has_written_global_memory = false;

    u32 last_clip_distance_mask = 0;
};

} // namespace OpenGL
