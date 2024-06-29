// SPDX-FileCopyrightText: Copyright 2020 sudachi Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "video_core/engines/fermi_2d.h"
#include "video_core/renderer_vulkan/vk_descriptor_pool.h"
#include "video_core/texture_cache/types.h"
#include "video_core/vulkan_common/vulkan_wrapper.h"

namespace Vulkan {

using VideoCommon::Extent3D;
using VideoCommon::Offset2D;
using VideoCommon::Region2D;

class Device;
class Framebuffer;
class ImageView;
class StateTracker;
class Scheduler;

struct BlitImagePipelineKey {
    constexpr auto operator<=>(const BlitImagePipelineKey&) const noexcept = default;

    VkRenderPass renderpass;
    Tegra::Engines::Fermi2D::Operation operation;
};

struct BlitDepthStencilPipelineKey {
    constexpr auto operator<=>(const BlitDepthStencilPipelineKey&) const noexcept = default;

    VkRenderPass renderpass;
    bool depth_clear;
    u8 stencil_mask;
    u32 stencil_compare_mask;
    u32 stencil_ref;
};

class BlitImageHelper {
public:
    explicit BlitImageHelper(const Device& device, Scheduler& scheduler,
                             StateTracker& state_tracker, DescriptorPool& descriptor_pool);
    ~BlitImageHelper();

    void BlitColor(const Framebuffer* dst_framebuffer, VkImageView src_image_view,
                   const Region2D& dst_region, const Region2D& src_region,
                   Tegra::Engines::Fermi2D::Filter filter,
                   Tegra::Engines::Fermi2D::Operation operation);

    void BlitColor(const Framebuffer* dst_framebuffer, VkImageView src_image_view,
                   VkImage src_image, VkSampler src_sampler, const Region2D& dst_region,
                   const Region2D& src_region, const Extent3D& src_size);

    void BlitDepthStencil(const Framebuffer* dst_framebuffer, VkImageView src_depth_view,
                          VkImageView src_stencil_view, const Region2D& dst_region,
                          const Region2D& src_region, Tegra::Engines::Fermi2D::Filter filter,
                          Tegra::Engines::Fermi2D::Operation operation);

    void ConvertD32ToR32(const Framebuffer* dst_framebuffer, const ImageView& src_image_view);

    void ConvertR32ToD32(const Framebuffer* dst_framebuffer, const ImageView& src_image_view);

    void ConvertD16ToR16(const Framebuffer* dst_framebuffer, const ImageView& src_image_view);

    void ConvertR16ToD16(const Framebuffer* dst_framebuffer, const ImageView& src_image_view);

    void ConvertABGR8ToD24S8(const Framebuffer* dst_framebuffer, const ImageView& src_image_view);

    void ConvertABGR8SRGBToD24S8(const Framebuffer* dst_framebuffer,
                                 const ImageView& src_image_view);

    void ConvertABGR8ToD32F(const Framebuffer* dst_framebuffer, const ImageView& src_image_view);

    void ConvertD32FToABGR8(const Framebuffer* dst_framebuffer, ImageView& src_image_view);

    void ConvertD24S8ToABGR8(const Framebuffer* dst_framebuffer, ImageView& src_image_view);

    void ConvertS8D24ToABGR8(const Framebuffer* dst_framebuffer, ImageView& src_image_view);

    void ClearColor(const Framebuffer* dst_framebuffer, u8 color_mask,
                    const std::array<f32, 4>& clear_color, const Region2D& dst_region);

    void ClearDepthStencil(const Framebuffer* dst_framebuffer, bool depth_clear, f32 clear_depth,
                           u8 stencil_mask, u32 stencil_ref, u32 stencil_compare_mask,
                           const Region2D& dst_region);

private:
    void Convert(VkPipeline pipeline, const Framebuffer* dst_framebuffer,
                 const ImageView& src_image_view);

    void ConvertDepthStencil(VkPipeline pipeline, const Framebuffer* dst_framebuffer,
                             ImageView& src_image_view);

    [[nodiscard]] VkPipeline FindOrEmplaceColorPipeline(const BlitImagePipelineKey& key);

    [[nodiscard]] VkPipeline FindOrEmplaceDepthStencilPipeline(const BlitImagePipelineKey& key);

    [[nodiscard]] VkPipeline FindOrEmplaceClearColorPipeline(const BlitImagePipelineKey& key);
    [[nodiscard]] VkPipeline FindOrEmplaceClearStencilPipeline(
        const BlitDepthStencilPipelineKey& key);

    void ConvertPipeline(vk::Pipeline& pipeline, VkRenderPass renderpass, bool is_target_depth);

    void ConvertDepthToColorPipeline(vk::Pipeline& pipeline, VkRenderPass renderpass);

    void ConvertColorToDepthPipeline(vk::Pipeline& pipeline, VkRenderPass renderpass);

    void ConvertPipelineEx(vk::Pipeline& pipeline, VkRenderPass renderpass,
                           vk::ShaderModule& module, bool single_texture, bool is_target_depth);

    void ConvertPipelineColorTargetEx(vk::Pipeline& pipeline, VkRenderPass renderpass,
                                      vk::ShaderModule& module);

    void ConvertPipelineDepthTargetEx(vk::Pipeline& pipeline, VkRenderPass renderpass,
                                      vk::ShaderModule& module);

    const Device& device;
    Scheduler& scheduler;
    StateTracker& state_tracker;

    vk::DescriptorSetLayout one_texture_set_layout;
    vk::DescriptorSetLayout two_textures_set_layout;
    DescriptorAllocator one_texture_descriptor_allocator;
    DescriptorAllocator two_textures_descriptor_allocator;
    vk::PipelineLayout one_texture_pipeline_layout;
    vk::PipelineLayout two_textures_pipeline_layout;
    vk::PipelineLayout clear_color_pipeline_layout;
    vk::ShaderModule full_screen_vert;
    vk::ShaderModule blit_color_to_color_frag;
    vk::ShaderModule blit_depth_stencil_frag;
    vk::ShaderModule clear_color_vert;
    vk::ShaderModule clear_color_frag;
    vk::ShaderModule clear_stencil_frag;
    vk::ShaderModule convert_depth_to_float_frag;
    vk::ShaderModule convert_float_to_depth_frag;
    vk::ShaderModule convert_abgr8_srgb_to_d24s8_frag, convert_abgr8_to_d24s8_frag;
    vk::ShaderModule convert_abgr8_to_d32f_frag;
    vk::ShaderModule convert_d32f_to_abgr8_frag;
    vk::ShaderModule convert_d24s8_to_abgr8_frag;
    vk::ShaderModule convert_s8d24_to_abgr8_frag;
    vk::Sampler linear_sampler;
    vk::Sampler nearest_sampler;

    std::vector<BlitImagePipelineKey> blit_color_keys;
    std::vector<vk::Pipeline> blit_color_pipelines;
    std::vector<BlitImagePipelineKey> blit_depth_stencil_keys;
    std::vector<vk::Pipeline> blit_depth_stencil_pipelines;
    std::vector<BlitImagePipelineKey> clear_color_keys;
    std::vector<vk::Pipeline> clear_color_pipelines;
    std::vector<BlitDepthStencilPipelineKey> clear_stencil_keys;
    std::vector<vk::Pipeline> clear_stencil_pipelines;
    vk::Pipeline convert_d32_to_r32_pipeline;
    vk::Pipeline convert_r32_to_d32_pipeline;
    vk::Pipeline convert_d16_to_r16_pipeline;
    vk::Pipeline convert_r16_to_d16_pipeline;
    vk::Pipeline convert_abgr8_srgb_to_d24s8_pipeline, convert_abgr8_to_d24s8_pipeline;
    vk::Pipeline convert_abgr8_to_d32f_pipeline;
    vk::Pipeline convert_d32f_to_abgr8_pipeline;
    vk::Pipeline convert_d24s8_to_abgr8_pipeline;
    vk::Pipeline convert_s8d24_to_abgr8_pipeline;
};

} // namespace Vulkan
