// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <memory>

#include "video_core/query_cache/query_cache_base.h"
#include "video_core/renderer_vulkan/vk_buffer_cache.h"

namespace VideoCore {
class RasterizerInterface;
}

namespace VideoCommon {
class StreamerInterface;
}

namespace Vulkan {

class Device;
class Scheduler;
class StagingBufferPool;

struct QueryCacheRuntimeImpl;

class QueryCacheRuntime {
public:
    explicit QueryCacheRuntime(VideoCore::RasterizerInterface* rasterizer,
                               Tegra::MaxwellDeviceMemoryManager& device_memory_,
                               Vulkan::BufferCache& buffer_cache_, const Device& device_,
                               const MemoryAllocator& memory_allocator_, Scheduler& scheduler_,
                               StagingBufferPool& staging_pool_,
                               ComputePassDescriptorQueue& compute_pass_descriptor_queue,
                               DescriptorPool& descriptor_pool);
    ~QueryCacheRuntime();

    template <typename SyncValuesType>
    void SyncValues(std::span<SyncValuesType> values, VkBuffer base_src_buffer = nullptr);

    void Barriers(bool is_prebarrier);

    void EndHostConditionalRendering();

    void PauseHostConditionalRendering();

    void ResumeHostConditionalRendering();

    bool HostConditionalRenderingCompareValue(VideoCommon::LookupData object_1, bool qc_dirty);

    bool HostConditionalRenderingCompareValues(VideoCommon::LookupData object_1,
                                               VideoCommon::LookupData object_2, bool qc_dirty,
                                               bool equal_check);

    VideoCommon::StreamerInterface* GetStreamerInterface(VideoCommon::QueryType query_type);

    void Bind3DEngine(Tegra::Engines::Maxwell3D* maxwell3d);

    template <typename Func>
    void View3DRegs(Func&& func);

private:
    void HostConditionalRenderingCompareValueImpl(VideoCommon::LookupData object, bool is_equal);
    void HostConditionalRenderingCompareBCImpl(DAddr address, bool is_equal);
    friend struct QueryCacheRuntimeImpl;
    std::unique_ptr<QueryCacheRuntimeImpl> impl;
};

struct QueryCacheParams {
    using RuntimeType = typename Vulkan::QueryCacheRuntime;
};

using QueryCache = VideoCommon::QueryCacheBase<QueryCacheParams>;

} // namespace Vulkan
