// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <cstddef>
#include <limits>
#include <map>
#include <memory>
#include <span>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include "common/bit_util.h"
#include "common/common_types.h"
#include "video_core/engines/draw_manager.h"
#include "video_core/host1x/gpu_device_memory_manager.h"
#include "video_core/query_cache/query_cache.h"
#include "video_core/rasterizer_interface.h"
#include "video_core/renderer_vulkan/vk_buffer_cache.h"
#include "video_core/renderer_vulkan/vk_compute_pass.h"
#include "video_core/renderer_vulkan/vk_query_cache.h"
#include "video_core/renderer_vulkan/vk_resource_pool.h"
#include "video_core/renderer_vulkan/vk_scheduler.h"
#include "video_core/renderer_vulkan/vk_staging_buffer_pool.h"
#include "video_core/renderer_vulkan/vk_update_descriptor.h"
#include "video_core/vulkan_common/vulkan_device.h"
#include "video_core/vulkan_common/vulkan_memory_allocator.h"
#include "video_core/vulkan_common/vulkan_wrapper.h"

namespace Vulkan {

using Tegra::Engines::Maxwell3D;
using VideoCommon::QueryType;

namespace {
class SamplesQueryBank : public VideoCommon::BankBase {
public:
    static constexpr size_t BANK_SIZE = 256;
    static constexpr size_t QUERY_SIZE = 8;
    explicit SamplesQueryBank(const Device& device_, size_t index_)
        : BankBase(BANK_SIZE), device{device_}, index{index_} {
        const auto& dev = device.GetLogical();
        query_pool = dev.CreateQueryPool({
            .sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .queryType = VK_QUERY_TYPE_OCCLUSION,
            .queryCount = BANK_SIZE,
            .pipelineStatistics = 0,
        });
        Reset();
    }

    ~SamplesQueryBank() = default;

    void Reset() override {
        ASSERT(references == 0);
        VideoCommon::BankBase::Reset();
        const auto& dev = device.GetLogical();
        dev.ResetQueryPool(*query_pool, 0, BANK_SIZE);
        host_results.fill(0ULL);
        next_bank = 0;
    }

    void Sync(size_t start, size_t size) {
        const auto& dev = device.GetLogical();
        const VkResult query_result = dev.GetQueryResults(
            *query_pool, static_cast<u32>(start), static_cast<u32>(size), sizeof(u64) * size,
            &host_results[start], sizeof(u64), VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);
        switch (query_result) {
        case VK_SUCCESS:
            return;
        case VK_ERROR_DEVICE_LOST:
            device.ReportLoss();
            [[fallthrough]];
        default:
            throw vk::Exception(query_result);
        }
    }

    VkQueryPool GetInnerPool() {
        return *query_pool;
    }

    size_t GetIndex() const {
        return index;
    }

    const std::array<u64, BANK_SIZE>& GetResults() const {
        return host_results;
    }

    size_t next_bank;

private:
    const Device& device;
    const size_t index;
    vk::QueryPool query_pool;
    std::array<u64, BANK_SIZE> host_results;
};

using BaseStreamer = VideoCommon::SimpleStreamer<VideoCommon::HostQueryBase>;

struct HostSyncValues {
    DAddr address;
    size_t size;
    size_t offset;

    static constexpr bool GeneratesBaseBuffer = false;
};

class SamplesStreamer : public BaseStreamer {
public:
    explicit SamplesStreamer(size_t id_, QueryCacheRuntime& runtime_,
                             VideoCore::RasterizerInterface* rasterizer_, const Device& device_,
                             Scheduler& scheduler_, const MemoryAllocator& memory_allocator_,
                             ComputePassDescriptorQueue& compute_pass_descriptor_queue,
                             DescriptorPool& descriptor_pool)
        : BaseStreamer(id_), runtime{runtime_}, rasterizer{rasterizer_}, device{device_},
          scheduler{scheduler_}, memory_allocator{memory_allocator_} {
        current_bank = nullptr;
        current_query = nullptr;
        amend_value = 0;
        accumulation_value = 0;
        queries_prefix_scan_pass = std::make_unique<QueriesPrefixScanPass>(
            device, scheduler, descriptor_pool, compute_pass_descriptor_queue);

        const VkBufferCreateInfo buffer_ci = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .size = 8,
            .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                     VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .queueFamilyIndexCount = 0,
            .pQueueFamilyIndices = nullptr,
        };
        accumulation_buffer = memory_allocator.CreateBuffer(buffer_ci, MemoryUsage::DeviceLocal);
        scheduler.RequestOutsideRenderPassOperationContext();
        scheduler.Record([buffer = *accumulation_buffer](vk::CommandBuffer cmdbuf) {
            cmdbuf.FillBuffer(buffer, 0, 8, 0);
        });
    }

    ~SamplesStreamer() = default;

    void StartCounter() override {
        if (has_started) {
            return;
        }
        ReserveHostQuery();
        scheduler.Record([query_pool = current_query_pool,
                          query_index = current_bank_slot](vk::CommandBuffer cmdbuf) {
            const bool use_precise = Settings::IsGPULevelHigh();
            cmdbuf.BeginQuery(query_pool, static_cast<u32>(query_index),
                              use_precise ? VK_QUERY_CONTROL_PRECISE_BIT : 0);
        });
        has_started = true;
    }

    void PauseCounter() override {
        if (!has_started) {
            return;
        }
        scheduler.Record([query_pool = current_query_pool,
                          query_index = current_bank_slot](vk::CommandBuffer cmdbuf) {
            cmdbuf.EndQuery(query_pool, static_cast<u32>(query_index));
        });
        has_started = false;
    }

    void ResetCounter() override {
        if (has_started) {
            PauseCounter();
        }
        AbandonCurrentQuery();
        std::function<void()> func([this, counts = pending_flush_queries.size()] {
            amend_value = 0;
            accumulation_value = 0;
        });
        rasterizer->SyncOperation(std::move(func));
        accumulation_since_last_sync = false;
        first_accumulation_checkpoint = std::min(first_accumulation_checkpoint, num_slots_used);
        last_accumulation_checkpoint = std::max(last_accumulation_checkpoint, num_slots_used);
    }

    void CloseCounter() override {
        PauseCounter();
    }

    bool HasPendingSync() const override {
        return !pending_sync.empty();
    }

    void SyncWrites() override {
        if (sync_values_stash.empty()) {
            return;
        }

        for (size_t i = 0; i < sync_values_stash.size(); i++) {
            runtime.template SyncValues<HostSyncValues>(sync_values_stash[i],
                                                        *buffers[resolve_buffers[i]]);
        }

        sync_values_stash.clear();
    }

    void PresyncWrites() override {
        if (pending_sync.empty()) {
            return;
        }
        PauseCounter();
        const auto driver_id = device.GetDriverID();
        if (driver_id == VK_DRIVER_ID_QUALCOMM_PROPRIETARY ||
            driver_id == VK_DRIVER_ID_ARM_PROPRIETARY || driver_id == VK_DRIVER_ID_MESA_TURNIP) {
            pending_sync.clear();
            sync_values_stash.clear();
            return;
        }
        sync_values_stash.clear();
        sync_values_stash.emplace_back();
        std::vector<HostSyncValues>* sync_values = &sync_values_stash.back();
        sync_values->reserve(num_slots_used);
        std::unordered_map<size_t, std::pair<size_t, size_t>> offsets;
        resolve_buffers.clear();
        size_t resolve_buffer_index = ObtainBuffer<true>(num_slots_used);
        resolve_buffers.push_back(resolve_buffer_index);
        size_t base_offset = 0;

        ApplyBanksWideOp<true>(pending_sync, [&](SamplesQueryBank* bank, size_t start,
                                                 size_t amount) {
            size_t bank_id = bank->GetIndex();
            auto& resolve_buffer = buffers[resolve_buffer_index];
            VkQueryPool query_pool = bank->GetInnerPool();
            scheduler.RequestOutsideRenderPassOperationContext();
            scheduler.Record([start, amount, base_offset, query_pool,
                              buffer = *resolve_buffer](vk::CommandBuffer cmdbuf) {
                const VkBufferMemoryBarrier copy_query_pool_barrier{
                    .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
                    .pNext = nullptr,
                    .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
                    .dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
                    .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                    .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                    .buffer = buffer,
                    .offset = base_offset,
                    .size = amount * SamplesQueryBank::QUERY_SIZE,
                };

                cmdbuf.CopyQueryPoolResults(
                    query_pool, static_cast<u32>(start), static_cast<u32>(amount), buffer,
                    static_cast<u32>(base_offset), SamplesQueryBank::QUERY_SIZE,
                    VK_QUERY_RESULT_WAIT_BIT | VK_QUERY_RESULT_64_BIT);
                cmdbuf.PipelineBarrier(VK_PIPELINE_STAGE_TRANSFER_BIT,
                                       VK_PIPELINE_STAGE_TRANSFER_BIT, 0, copy_query_pool_barrier);
            });
            offsets[bank_id] = {start, base_offset};
            base_offset += amount * SamplesQueryBank::QUERY_SIZE;
        });

        // Convert queries
        bool has_multi_queries = false;
        for (auto q : pending_sync) {
            auto* query = GetQuery(q);
            size_t sync_value_slot = 0;
            if (True(query->flags & VideoCommon::QueryFlagBits::IsRewritten)) {
                continue;
            }
            if (True(query->flags & VideoCommon::QueryFlagBits::IsInvalidated)) {
                continue;
            }
            if (accumulation_since_last_sync || query->size_slots > 1) {
                if (!has_multi_queries) {
                    has_multi_queries = true;
                    sync_values_stash.emplace_back();
                }
                sync_value_slot = 1;
            }
            query->flags |= VideoCommon::QueryFlagBits::IsHostSynced;
            auto loc_data = offsets[query->start_bank_id];
            sync_values_stash[sync_value_slot].emplace_back(HostSyncValues{
                .address = query->guest_address,
                .size = SamplesQueryBank::QUERY_SIZE,
                .offset =
                    loc_data.second + (query->start_slot - loc_data.first + query->size_slots - 1) *
                                          SamplesQueryBank::QUERY_SIZE,
            });
        }

        if (has_multi_queries) {
            const size_t min_accumulation_limit =
                std::min(first_accumulation_checkpoint, num_slots_used);
            const size_t max_accumulation_limit =
                std::max(last_accumulation_checkpoint, num_slots_used);
            const size_t intermediary_buffer_index = ObtainBuffer<false>(num_slots_used);
            resolve_buffers.push_back(intermediary_buffer_index);
            queries_prefix_scan_pass->Run(*accumulation_buffer, *buffers[intermediary_buffer_index],
                                          *buffers[resolve_buffer_index], num_slots_used,
                                          min_accumulation_limit, max_accumulation_limit);

        } else {
            scheduler.RequestOutsideRenderPassOperationContext();
            scheduler.Record([buffer = *accumulation_buffer](vk::CommandBuffer cmdbuf) {
                cmdbuf.FillBuffer(buffer, 0, 8, 0);
            });
        }

        ReplicateCurrentQueryIfNeeded();
        std::function<void()> func([this] { amend_value = accumulation_value; });
        rasterizer->SyncOperation(std::move(func));
        AbandonCurrentQuery();
        num_slots_used = 0;
        first_accumulation_checkpoint = std::numeric_limits<size_t>::max();
        last_accumulation_checkpoint = 0;
        accumulation_since_last_sync = has_multi_queries;
        pending_sync.clear();
    }

    size_t WriteCounter(DAddr address, bool has_timestamp, u32 value,
                        [[maybe_unused]] std::optional<u32> subreport) override {
        PauseCounter();
        auto index = BuildQuery();
        auto* new_query = GetQuery(index);
        new_query->guest_address = address;
        new_query->value = 0;
        new_query->flags &= ~VideoCommon::QueryFlagBits::IsOrphan;
        if (has_timestamp) {
            new_query->flags |= VideoCommon::QueryFlagBits::HasTimestamp;
        }
        if (!current_query) {
            new_query->flags |= VideoCommon::QueryFlagBits::IsFinalValueSynced;
            return index;
        }
        new_query->start_bank_id = current_query->start_bank_id;
        new_query->size_banks = current_query->size_banks;
        new_query->start_slot = current_query->start_slot;
        new_query->size_slots = current_query->size_slots;
        ApplyBankOp(new_query, [](SamplesQueryBank* bank, size_t start, size_t amount) {
            bank->AddReference(amount);
        });
        pending_sync.push_back(index);
        pending_flush_queries.push_back(index);
        return index;
    }

    bool HasUnsyncedQueries() const override {
        return !pending_flush_queries.empty();
    }

    void PushUnsyncedQueries() override {
        PauseCounter();
        current_bank->Close();
        {
            std::scoped_lock lk(flush_guard);
            pending_flush_sets.emplace_back(std::move(pending_flush_queries));
        }
    }

    void PopUnsyncedQueries() override {
        std::vector<size_t> current_flush_queries;
        {
            std::scoped_lock lk(flush_guard);
            current_flush_queries = std::move(pending_flush_sets.front());
            pending_flush_sets.pop_front();
        }
        ApplyBanksWideOp<false>(
            current_flush_queries,
            [](SamplesQueryBank* bank, size_t start, size_t amount) { bank->Sync(start, amount); });
        for (auto q : current_flush_queries) {
            auto* query = GetQuery(q);
            u64 total = 0;
            ApplyBankOp(query, [&total](SamplesQueryBank* bank, size_t start, size_t amount) {
                const auto& results = bank->GetResults();
                for (size_t i = 0; i < amount; i++) {
                    total += results[start + i];
                }
            });
            query->value = total;
            query->flags |= VideoCommon::QueryFlagBits::IsFinalValueSynced;
        }
    }

private:
    template <typename Func>
    void ApplyBankOp(VideoCommon::HostQueryBase* query, Func&& func) {
        size_t size_slots = query->size_slots;
        if (size_slots == 0) {
            return;
        }
        size_t bank_id = query->start_bank_id;
        size_t banks_set = query->size_banks;
        size_t start_slot = query->start_slot;
        for (size_t i = 0; i < banks_set; i++) {
            auto& the_bank = bank_pool.GetBank(bank_id);
            size_t amount = std::min(the_bank.Size() - start_slot, size_slots);
            func(&the_bank, start_slot, amount);
            bank_id = the_bank.next_bank - 1;
            start_slot = 0;
            size_slots -= amount;
        }
    }

    template <bool is_ordered, typename Func>
    void ApplyBanksWideOp(std::vector<size_t>& queries, Func&& func) {
        std::conditional_t<is_ordered, std::map<size_t, std::pair<size_t, size_t>>,
                           std::unordered_map<size_t, std::pair<size_t, size_t>>>
            indexer;
        for (auto q : queries) {
            auto* query = GetQuery(q);
            ApplyBankOp(query, [&indexer](SamplesQueryBank* bank, size_t start, size_t amount) {
                auto id_ = bank->GetIndex();
                auto pair = indexer.try_emplace(id_, std::numeric_limits<size_t>::max(),
                                                std::numeric_limits<size_t>::min());
                auto& current_pair = pair.first->second;
                current_pair.first = std::min(current_pair.first, start);
                current_pair.second = std::max(current_pair.second, amount + start);
            });
        }
        for (auto& cont : indexer) {
            func(&bank_pool.GetBank(cont.first), cont.second.first,
                 cont.second.second - cont.second.first);
        }
    }

    void ReserveBank() {
        current_bank_id =
            bank_pool.ReserveBank([this](std::deque<SamplesQueryBank>& queue, size_t index) {
                queue.emplace_back(device, index);
            });
        if (current_bank) {
            current_bank->next_bank = current_bank_id + 1;
        }
        current_bank = &bank_pool.GetBank(current_bank_id);
        current_query_pool = current_bank->GetInnerPool();
    }

    size_t ReserveBankSlot() {
        if (!current_bank || current_bank->IsClosed()) {
            ReserveBank();
        }
        auto [built, index] = current_bank->Reserve();
        current_bank_slot = index;
        return index;
    }

    void ReserveHostQuery() {
        size_t new_slot = ReserveBankSlot();
        current_bank->AddReference(1);
        num_slots_used++;
        if (current_query) {
            size_t bank_id = current_query->start_bank_id;
            size_t banks_set = current_query->size_banks - 1;
            bool found = bank_id == current_bank_id;
            while (!found && banks_set > 0) {
                SamplesQueryBank& some_bank = bank_pool.GetBank(bank_id);
                bank_id = some_bank.next_bank - 1;
                found = bank_id == current_bank_id;
                banks_set--;
            }
            if (!found) {
                current_query->size_banks++;
            }
            current_query->size_slots++;
        } else {
            current_query_id = BuildQuery();
            current_query = GetQuery(current_query_id);
            current_query->start_bank_id = static_cast<u32>(current_bank_id);
            current_query->size_banks = 1;
            current_query->start_slot = new_slot;
            current_query->size_slots = 1;
        }
    }

    void Free(size_t query_id) override {
        std::scoped_lock lk(guard);
        auto* query = GetQuery(query_id);
        ApplyBankOp(query, [](SamplesQueryBank* bank, size_t start, size_t amount) {
            bank->CloseReference(amount);
        });
        ReleaseQuery(query_id);
    }

    void AbandonCurrentQuery() {
        if (!current_query) {
            return;
        }
        Free(current_query_id);
        current_query = nullptr;
        current_query_id = 0;
    }

    void ReplicateCurrentQueryIfNeeded() {
        if (pending_sync.empty()) {
            return;
        }
        if (!current_query) {
            return;
        }
        auto index = BuildQuery();
        auto* new_query = GetQuery(index);
        new_query->guest_address = 0;
        new_query->value = 0;
        new_query->flags &= ~VideoCommon::QueryFlagBits::IsOrphan;
        new_query->start_bank_id = current_query->start_bank_id;
        new_query->size_banks = current_query->size_banks;
        new_query->start_slot = current_query->start_slot;
        new_query->size_slots = current_query->size_slots;
        ApplyBankOp(new_query, [](SamplesQueryBank* bank, size_t start, size_t amount) {
            bank->AddReference(amount);
        });
        pending_flush_queries.push_back(index);
        std::function<void()> func([this, index] {
            auto* query = GetQuery(index);
            query->value += GetAmendValue();
            SetAccumulationValue(query->value);
            Free(index);
        });
        rasterizer->SyncOperation(std::move(func));
    }

    template <bool is_resolve>
    size_t ObtainBuffer(size_t num_needed) {
        const size_t log_2 = std::max<size_t>(11U, Common::Log2Ceil64(num_needed));
        if constexpr (is_resolve) {
            if (resolve_table[log_2] != 0) {
                return resolve_table[log_2] - 1;
            }
        } else {
            if (intermediary_table[log_2] != 0) {
                return intermediary_table[log_2] - 1;
            }
        }
        const VkBufferCreateInfo buffer_ci = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .size = SamplesQueryBank::QUERY_SIZE * (1ULL << log_2),
            .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                     VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .queueFamilyIndexCount = 0,
            .pQueueFamilyIndices = nullptr,
        };
        buffers.emplace_back(memory_allocator.CreateBuffer(buffer_ci, MemoryUsage::DeviceLocal));
        if constexpr (is_resolve) {
            resolve_table[log_2] = buffers.size();
        } else {
            intermediary_table[log_2] = buffers.size();
        }
        return buffers.size() - 1;
    }

    QueryCacheRuntime& runtime;
    VideoCore::RasterizerInterface* rasterizer;
    const Device& device;
    Scheduler& scheduler;
    const MemoryAllocator& memory_allocator;
    VideoCommon::BankPool<SamplesQueryBank> bank_pool;
    std::deque<vk::Buffer> buffers;
    std::array<size_t, 32> resolve_table{};
    std::array<size_t, 32> intermediary_table{};
    vk::Buffer accumulation_buffer;
    std::deque<std::vector<HostSyncValues>> sync_values_stash;
    std::vector<size_t> resolve_buffers;

    // syncing queue
    std::vector<size_t> pending_sync;

    // flush levels
    std::vector<size_t> pending_flush_queries;
    std::deque<std::vector<size_t>> pending_flush_sets;

    // State Machine
    size_t current_bank_slot;
    size_t current_bank_id;
    SamplesQueryBank* current_bank;
    VkQueryPool current_query_pool;
    size_t current_query_id;
    size_t num_slots_used{};
    size_t first_accumulation_checkpoint{};
    size_t last_accumulation_checkpoint{};
    bool accumulation_since_last_sync{};
    VideoCommon::HostQueryBase* current_query;
    bool has_started{};
    std::mutex flush_guard;

    std::unique_ptr<QueriesPrefixScanPass> queries_prefix_scan_pass;
};

// Transform feedback queries
class TFBQueryBank : public VideoCommon::BankBase {
public:
    static constexpr size_t BANK_SIZE = 1024;
    static constexpr size_t QUERY_SIZE = 4;
    explicit TFBQueryBank(Scheduler& scheduler_, const MemoryAllocator& memory_allocator,
                          size_t index_)
        : BankBase(BANK_SIZE), scheduler{scheduler_}, index{index_} {
        const VkBufferCreateInfo buffer_ci = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .size = QUERY_SIZE * BANK_SIZE,
            .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .queueFamilyIndexCount = 0,
            .pQueueFamilyIndices = nullptr,
        };
        buffer = memory_allocator.CreateBuffer(buffer_ci, MemoryUsage::DeviceLocal);
    }

    ~TFBQueryBank() = default;

    void Reset() override {
        ASSERT(references == 0);
        VideoCommon::BankBase::Reset();
    }

    void Sync(StagingBufferRef& stagging_buffer, size_t extra_offset, size_t start, size_t size) {
        scheduler.RequestOutsideRenderPassOperationContext();
        scheduler.Record([this, dst_buffer = stagging_buffer.buffer, extra_offset, start,
                          size](vk::CommandBuffer cmdbuf) {
            std::array<VkBufferCopy, 1> copy{VkBufferCopy{
                .srcOffset = start * QUERY_SIZE,
                .dstOffset = extra_offset,
                .size = size * QUERY_SIZE,
            }};
            cmdbuf.CopyBuffer(*buffer, dst_buffer, copy);
        });
    }

    size_t GetIndex() const {
        return index;
    }

    VkBuffer GetBuffer() const {
        return *buffer;
    }

private:
    Scheduler& scheduler;
    const size_t index;
    vk::Buffer buffer;
};

class PrimitivesSucceededStreamer;

class TFBCounterStreamer : public BaseStreamer {
public:
    explicit TFBCounterStreamer(size_t id_, QueryCacheRuntime& runtime_, const Device& device_,
                                Scheduler& scheduler_, const MemoryAllocator& memory_allocator_,
                                StagingBufferPool& staging_pool_)
        : BaseStreamer(id_), runtime{runtime_}, device{device_}, scheduler{scheduler_},
          memory_allocator{memory_allocator_}, staging_pool{staging_pool_} {
        buffers_count = 0;
        current_bank = nullptr;
        counter_buffers.fill(VK_NULL_HANDLE);
        offsets.fill(0);
        last_queries.fill(0);
        last_queries_stride.fill(1);
        const VkBufferCreateInfo buffer_ci = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .size = TFBQueryBank::QUERY_SIZE * NUM_STREAMS,
            .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                     VK_BUFFER_USAGE_TRANSFORM_FEEDBACK_COUNTER_BUFFER_BIT_EXT,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .queueFamilyIndexCount = 0,
            .pQueueFamilyIndices = nullptr,
        };

        counters_buffer = memory_allocator.CreateBuffer(buffer_ci, MemoryUsage::DeviceLocal);
        for (auto& c : counter_buffers) {
            c = *counters_buffer;
        }
        size_t base_offset = 0;
        for (auto& o : offsets) {
            o = base_offset;
            base_offset += TFBQueryBank::QUERY_SIZE;
        }
    }

    ~TFBCounterStreamer() = default;

    void StartCounter() override {
        FlushBeginTFB();
        has_started = true;
    }

    void PauseCounter() override {
        CloseCounter();
    }

    void ResetCounter() override {
        CloseCounter();
    }

    void CloseCounter() override {
        if (has_flushed_end_pending) {
            FlushEndTFB();
        }
        runtime.View3DRegs([this](Maxwell3D& maxwell3d) {
            if (maxwell3d.regs.transform_feedback_enabled == 0) {
                streams_mask = 0;
                has_started = false;
            }
        });
    }

    bool HasPendingSync() const override {
        return !pending_sync.empty();
    }

    void SyncWrites() override {
        CloseCounter();
        std::unordered_map<size_t, std::vector<HostSyncValues>> sync_values_stash;
        for (auto q : pending_sync) {
            auto* query = GetQuery(q);
            if (True(query->flags & VideoCommon::QueryFlagBits::IsRewritten)) {
                continue;
            }
            if (True(query->flags & VideoCommon::QueryFlagBits::IsInvalidated)) {
                continue;
            }
            query->flags |= VideoCommon::QueryFlagBits::IsHostSynced;
            sync_values_stash.try_emplace(query->start_bank_id);
            sync_values_stash[query->start_bank_id].emplace_back(HostSyncValues{
                .address = query->guest_address,
                .size = TFBQueryBank::QUERY_SIZE,
                .offset = query->start_slot * TFBQueryBank::QUERY_SIZE,
            });
        }
        for (auto& p : sync_values_stash) {
            auto& bank = bank_pool.GetBank(p.first);
            runtime.template SyncValues<HostSyncValues>(p.second, bank.GetBuffer());
        }
        pending_sync.clear();
    }

    size_t WriteCounter(DAddr address, bool has_timestamp, u32 value,
                        std::optional<u32> subreport_) override {
        auto index = BuildQuery();
        auto* new_query = GetQuery(index);
        new_query->guest_address = address;
        new_query->value = 0;
        new_query->flags &= ~VideoCommon::QueryFlagBits::IsOrphan;
        if (has_timestamp) {
            new_query->flags |= VideoCommon::QueryFlagBits::HasTimestamp;
        }
        if (!subreport_) {
            new_query->flags |= VideoCommon::QueryFlagBits::IsFinalValueSynced;
            return index;
        }
        const size_t subreport = static_cast<size_t>(*subreport_);
        last_queries[subreport] = address;
        if ((streams_mask & (1ULL << subreport)) == 0) {
            new_query->flags |= VideoCommon::QueryFlagBits::IsFinalValueSynced;
            return index;
        }
        CloseCounter();
        auto [bank_slot, data_slot] = ProduceCounterBuffer(subreport);
        new_query->start_bank_id = static_cast<u32>(bank_slot);
        new_query->size_banks = 1;
        new_query->start_slot = static_cast<u32>(data_slot);
        new_query->size_slots = 1;
        pending_sync.push_back(index);
        pending_flush_queries.push_back(index);
        return index;
    }

    std::optional<std::pair<DAddr, size_t>> GetLastQueryStream(size_t stream) {
        if (last_queries[stream] != 0) {
            std::pair<DAddr, size_t> result(last_queries[stream], last_queries_stride[stream]);
            return result;
        }
        return std::nullopt;
    }

    Maxwell3D::Regs::PrimitiveTopology GetOutputTopology() const {
        return out_topology;
    }

    bool HasUnsyncedQueries() const override {
        return !pending_flush_queries.empty();
    }

    void PushUnsyncedQueries() override {
        CloseCounter();
        auto staging_ref = staging_pool.Request(
            pending_flush_queries.size() * TFBQueryBank::QUERY_SIZE, MemoryUsage::Download, true);
        size_t offset_base = staging_ref.offset;
        for (auto q : pending_flush_queries) {
            auto* query = GetQuery(q);
            auto& bank = bank_pool.GetBank(query->start_bank_id);
            bank.Sync(staging_ref, offset_base, query->start_slot, 1);
            offset_base += TFBQueryBank::QUERY_SIZE;
            bank.CloseReference();
        }
        static constexpr VkMemoryBarrier WRITE_BARRIER{
            .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
            .pNext = nullptr,
            .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
            .dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
        };
        scheduler.RequestOutsideRenderPassOperationContext();
        scheduler.Record([](vk::CommandBuffer cmdbuf) {
            cmdbuf.PipelineBarrier(VK_PIPELINE_STAGE_TRANSFER_BIT,
                                   VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, WRITE_BARRIER);
        });

        std::scoped_lock lk(flush_guard);
        for (auto& str : free_queue) {
            staging_pool.FreeDeferred(str);
        }
        free_queue.clear();
        download_buffers.emplace_back(staging_ref);
        pending_flush_sets.emplace_back(std::move(pending_flush_queries));
    }

    void PopUnsyncedQueries() override {
        StagingBufferRef staging_ref;
        std::vector<size_t> flushed_queries;
        {
            std::scoped_lock lk(flush_guard);
            staging_ref = download_buffers.front();
            flushed_queries = std::move(pending_flush_sets.front());
            download_buffers.pop_front();
            pending_flush_sets.pop_front();
        }

        size_t offset_base = staging_ref.offset;
        for (auto q : flushed_queries) {
            auto* query = GetQuery(q);
            u32 result = 0;
            std::memcpy(&result, staging_ref.mapped_span.data() + offset_base, sizeof(u32));
            query->value = static_cast<u64>(result);
            query->flags |= VideoCommon::QueryFlagBits::IsFinalValueSynced;
            offset_base += TFBQueryBank::QUERY_SIZE;
        }

        {
            std::scoped_lock lk(flush_guard);
            free_queue.emplace_back(staging_ref);
        }
    }

private:
    void FlushBeginTFB() {
        if (has_flushed_end_pending) [[unlikely]] {
            return;
        }
        has_flushed_end_pending = true;
        if (!has_started || buffers_count == 0) {
            scheduler.Record([](vk::CommandBuffer cmdbuf) {
                cmdbuf.BeginTransformFeedbackEXT(0, 0, nullptr, nullptr);
            });
            UpdateBuffers();
            return;
        }
        scheduler.Record([this, total = static_cast<u32>(buffers_count)](vk::CommandBuffer cmdbuf) {
            cmdbuf.BeginTransformFeedbackEXT(0, total, counter_buffers.data(), offsets.data());
        });
        UpdateBuffers();
    }

    void FlushEndTFB() {
        if (!has_flushed_end_pending) [[unlikely]] {
            UNREACHABLE();
            return;
        }
        has_flushed_end_pending = false;

        if (buffers_count == 0) {
            scheduler.Record([](vk::CommandBuffer cmdbuf) {
                cmdbuf.EndTransformFeedbackEXT(0, 0, nullptr, nullptr);
            });
        } else {
            scheduler.Record([this,
                              total = static_cast<u32>(buffers_count)](vk::CommandBuffer cmdbuf) {
                cmdbuf.EndTransformFeedbackEXT(0, total, counter_buffers.data(), offsets.data());
            });
        }
    }

    void UpdateBuffers() {
        last_queries.fill(0);
        last_queries_stride.fill(1);
        runtime.View3DRegs([this](Maxwell3D& maxwell3d) {
            buffers_count = 0;
            out_topology = maxwell3d.draw_manager->GetDrawState().topology;
            for (size_t i = 0; i < Maxwell3D::Regs::NumTransformFeedbackBuffers; i++) {
                const auto& tf = maxwell3d.regs.transform_feedback;
                if (tf.buffers[i].enable == 0) {
                    continue;
                }
                const size_t stream = tf.controls[i].stream;
                last_queries_stride[stream] = tf.controls[i].stride;
                streams_mask |= 1ULL << stream;
                buffers_count = std::max<size_t>(buffers_count, stream + 1);
            }
        });
    }

    std::pair<size_t, size_t> ProduceCounterBuffer(size_t stream) {
        if (current_bank == nullptr || current_bank->IsClosed()) {
            current_bank_id =
                bank_pool.ReserveBank([this](std::deque<TFBQueryBank>& queue, size_t index) {
                    queue.emplace_back(scheduler, memory_allocator, index);
                });
            current_bank = &bank_pool.GetBank(current_bank_id);
        }
        auto [dont_care, other] = current_bank->Reserve();
        const size_t slot = other; // workaround to compile bug.
        current_bank->AddReference();

        static constexpr VkMemoryBarrier READ_BARRIER{
            .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
            .pNext = nullptr,
            .srcAccessMask = VK_ACCESS_TRANSFORM_FEEDBACK_COUNTER_WRITE_BIT_EXT,
            .dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
        };
        static constexpr VkMemoryBarrier WRITE_BARRIER{
            .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
            .pNext = nullptr,
            .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
            .dstAccessMask = VK_ACCESS_MEMORY_READ_BIT,
        };
        scheduler.RequestOutsideRenderPassOperationContext();
        scheduler.Record([dst_buffer = current_bank->GetBuffer(),
                          src_buffer = counter_buffers[stream], src_offset = offsets[stream],
                          slot](vk::CommandBuffer cmdbuf) {
            cmdbuf.PipelineBarrier(VK_PIPELINE_STAGE_TRANSFORM_FEEDBACK_BIT_EXT,
                                   VK_PIPELINE_STAGE_TRANSFER_BIT, 0, READ_BARRIER);
            std::array<VkBufferCopy, 1> copy{VkBufferCopy{
                .srcOffset = src_offset,
                .dstOffset = slot * TFBQueryBank::QUERY_SIZE,
                .size = TFBQueryBank::QUERY_SIZE,
            }};
            cmdbuf.CopyBuffer(src_buffer, dst_buffer, copy);
            cmdbuf.PipelineBarrier(VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                   0, WRITE_BARRIER);
        });
        return {current_bank_id, slot};
    }

    friend class PrimitivesSucceededStreamer;

    static constexpr size_t NUM_STREAMS = 4;

    QueryCacheRuntime& runtime;
    const Device& device;
    Scheduler& scheduler;
    const MemoryAllocator& memory_allocator;
    StagingBufferPool& staging_pool;
    VideoCommon::BankPool<TFBQueryBank> bank_pool;
    size_t current_bank_id;
    TFBQueryBank* current_bank;
    vk::Buffer counters_buffer;

    // syncing queue
    std::vector<size_t> pending_sync;

    // flush levels
    std::vector<size_t> pending_flush_queries;
    std::deque<StagingBufferRef> download_buffers;
    std::deque<std::vector<size_t>> pending_flush_sets;
    std::vector<StagingBufferRef> free_queue;
    std::mutex flush_guard;

    // state machine
    bool has_started{};
    bool has_flushed_end_pending{};
    size_t buffers_count{};
    std::array<VkBuffer, NUM_STREAMS> counter_buffers{};
    std::array<VkDeviceSize, NUM_STREAMS> offsets{};
    std::array<DAddr, NUM_STREAMS> last_queries;
    std::array<size_t, NUM_STREAMS> last_queries_stride;
    Maxwell3D::Regs::PrimitiveTopology out_topology;
    u64 streams_mask;
};

class PrimitivesQueryBase : public VideoCommon::QueryBase {
public:
    // Default constructor
    PrimitivesQueryBase()
        : VideoCommon::QueryBase(0, VideoCommon::QueryFlagBits::IsHostManaged, 0) {}

    // Parameterized constructor
    PrimitivesQueryBase(bool has_timestamp, DAddr address)
        : VideoCommon::QueryBase(address, VideoCommon::QueryFlagBits::IsHostManaged, 0) {
        if (has_timestamp) {
            flags |= VideoCommon::QueryFlagBits::HasTimestamp;
        }
    }

    u64 stride{};
    DAddr dependant_address{};
    Maxwell3D::Regs::PrimitiveTopology topology{Maxwell3D::Regs::PrimitiveTopology::Points};
    size_t dependant_index{};
    bool dependant_manage{};
};

class PrimitivesSucceededStreamer : public VideoCommon::SimpleStreamer<PrimitivesQueryBase> {
public:
    explicit PrimitivesSucceededStreamer(size_t id_, QueryCacheRuntime& runtime_,
                                         TFBCounterStreamer& tfb_streamer_,
                                         Tegra::MaxwellDeviceMemoryManager& device_memory_)
        : VideoCommon::SimpleStreamer<PrimitivesQueryBase>(id_), runtime{runtime_},
          tfb_streamer{tfb_streamer_}, device_memory{device_memory_} {
        MakeDependent(&tfb_streamer);
    }

    ~PrimitivesSucceededStreamer() = default;

    size_t WriteCounter(DAddr address, bool has_timestamp, u32 value,
                        std::optional<u32> subreport_) override {
        auto index = BuildQuery();
        auto* new_query = GetQuery(index);
        new_query->guest_address = address;
        new_query->value = 0;
        if (has_timestamp) {
            new_query->flags |= VideoCommon::QueryFlagBits::HasTimestamp;
        }
        if (!subreport_) {
            new_query->flags |= VideoCommon::QueryFlagBits::IsFinalValueSynced;
            return index;
        }
        const size_t subreport = static_cast<size_t>(*subreport_);
        auto dependant_address_opt = tfb_streamer.GetLastQueryStream(subreport);
        bool must_manage_dependance = false;
        new_query->topology = tfb_streamer.GetOutputTopology();
        if (dependant_address_opt) {
            auto [dep_address, stride] = *dependant_address_opt;
            new_query->dependant_address = dep_address;
            new_query->stride = stride;
        } else {
            new_query->dependant_index =
                tfb_streamer.WriteCounter(address, has_timestamp, value, subreport_);
            auto* dependant_query = tfb_streamer.GetQuery(new_query->dependant_index);
            dependant_query->flags |= VideoCommon::QueryFlagBits::IsInvalidated;
            must_manage_dependance = true;
            if (True(dependant_query->flags & VideoCommon::QueryFlagBits::IsFinalValueSynced)) {
                new_query->value = 0;
                new_query->flags |= VideoCommon::QueryFlagBits::IsFinalValueSynced;
                if (must_manage_dependance) {
                    tfb_streamer.Free(new_query->dependant_index);
                }
                return index;
            }
            new_query->stride = 1;
            runtime.View3DRegs([new_query, subreport](Maxwell3D& maxwell3d) {
                for (size_t i = 0; i < Maxwell3D::Regs::NumTransformFeedbackBuffers; i++) {
                    const auto& tf = maxwell3d.regs.transform_feedback;
                    if (tf.buffers[i].enable == 0) {
                        continue;
                    }
                    if (tf.controls[i].stream != subreport) {
                        continue;
                    }
                    new_query->stride = tf.controls[i].stride;
                    break;
                }
            });
        }

        new_query->dependant_manage = must_manage_dependance;
        pending_flush_queries.push_back(index);
        return index;
    }

    bool HasUnsyncedQueries() const override {
        return !pending_flush_queries.empty();
    }

    void PushUnsyncedQueries() override {
        std::scoped_lock lk(flush_guard);
        pending_flush_sets.emplace_back(std::move(pending_flush_queries));
        pending_flush_queries.clear();
    }

    void PopUnsyncedQueries() override {
        std::vector<size_t> flushed_queries;
        {
            std::scoped_lock lk(flush_guard);
            flushed_queries = std::move(pending_flush_sets.front());
            pending_flush_sets.pop_front();
        }

        for (auto q : flushed_queries) {
            auto* query = GetQuery(q);
            if (True(query->flags & VideoCommon::QueryFlagBits::IsFinalValueSynced)) {
                continue;
            }

            query->flags |= VideoCommon::QueryFlagBits::IsFinalValueSynced;
            u64 num_vertices = 0;
            if (query->dependant_manage) {
                auto* dependant_query = tfb_streamer.GetQuery(query->dependant_index);
                num_vertices = dependant_query->value / query->stride;
                tfb_streamer.Free(query->dependant_index);
            } else {
                u8* pointer = device_memory.GetPointer<u8>(query->dependant_address);
                if (pointer != nullptr) {
                    u32 result;
                    std::memcpy(&result, pointer, sizeof(u32));
                    num_vertices = static_cast<u64>(result) / query->stride;
                }
            }
            query->value = [&]() -> u64 {
                switch (query->topology) {
                case Maxwell3D::Regs::PrimitiveTopology::Points:
                    return num_vertices;
                case Maxwell3D::Regs::PrimitiveTopology::Lines:
                    return num_vertices / 2;
                case Maxwell3D::Regs::PrimitiveTopology::LineLoop:
                    return (num_vertices / 2) + 1;
                case Maxwell3D::Regs::PrimitiveTopology::LineStrip:
                    return num_vertices - 1;
                case Maxwell3D::Regs::PrimitiveTopology::Patches:
                case Maxwell3D::Regs::PrimitiveTopology::Triangles:
                case Maxwell3D::Regs::PrimitiveTopology::TrianglesAdjacency:
                    return num_vertices / 3;
                case Maxwell3D::Regs::PrimitiveTopology::TriangleFan:
                case Maxwell3D::Regs::PrimitiveTopology::TriangleStrip:
                case Maxwell3D::Regs::PrimitiveTopology::TriangleStripAdjacency:
                    return num_vertices - 2;
                case Maxwell3D::Regs::PrimitiveTopology::Quads:
                    return num_vertices / 4;
                case Maxwell3D::Regs::PrimitiveTopology::Polygon:
                    return 1U;
                default:
                    return num_vertices;
                }
            }();
        }
    }

private:
    QueryCacheRuntime& runtime;
    TFBCounterStreamer& tfb_streamer;
    Tegra::MaxwellDeviceMemoryManager& device_memory;

    // syncing queue
    std::vector<size_t> pending_sync;

    // flush levels
    std::vector<size_t> pending_flush_queries;
    std::deque<std::vector<size_t>> pending_flush_sets;
    std::mutex flush_guard;
};

} // namespace

struct QueryCacheRuntimeImpl {
    QueryCacheRuntimeImpl(QueryCacheRuntime& runtime, VideoCore::RasterizerInterface* rasterizer_,
                          Tegra::MaxwellDeviceMemoryManager& device_memory_,
                          Vulkan::BufferCache& buffer_cache_, const Device& device_,
                          const MemoryAllocator& memory_allocator_, Scheduler& scheduler_,
                          StagingBufferPool& staging_pool_,
                          ComputePassDescriptorQueue& compute_pass_descriptor_queue,
                          DescriptorPool& descriptor_pool)
        : rasterizer{rasterizer_}, device_memory{device_memory_}, buffer_cache{buffer_cache_},
          device{device_}, memory_allocator{memory_allocator_}, scheduler{scheduler_},
          staging_pool{staging_pool_}, guest_streamer(0, runtime),
          sample_streamer(static_cast<size_t>(QueryType::ZPassPixelCount64), runtime, rasterizer,
                          device, scheduler, memory_allocator, compute_pass_descriptor_queue,
                          descriptor_pool),
          tfb_streamer(static_cast<size_t>(QueryType::StreamingByteCount), runtime, device,
                       scheduler, memory_allocator, staging_pool),
          primitives_succeeded_streamer(
              static_cast<size_t>(QueryType::StreamingPrimitivesSucceeded), runtime, tfb_streamer,
              device_memory_),
          primitives_needed_minus_succeeded_streamer(
              static_cast<size_t>(QueryType::StreamingPrimitivesNeededMinusSucceeded), runtime, 0u),
          hcr_setup{}, hcr_is_set{}, is_hcr_running{}, maxwell3d{} {

        hcr_setup.sType = VK_STRUCTURE_TYPE_CONDITIONAL_RENDERING_BEGIN_INFO_EXT;
        hcr_setup.pNext = nullptr;
        hcr_setup.flags = 0;

        conditional_resolve_pass = std::make_unique<ConditionalRenderingResolvePass>(
            device, scheduler, descriptor_pool, compute_pass_descriptor_queue);

        const VkBufferCreateInfo buffer_ci = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .size = sizeof(u32),
            .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                     VK_BUFFER_USAGE_CONDITIONAL_RENDERING_BIT_EXT,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .queueFamilyIndexCount = 0,
            .pQueueFamilyIndices = nullptr,
        };
        hcr_resolve_buffer = memory_allocator.CreateBuffer(buffer_ci, MemoryUsage::DeviceLocal);
    }

    VideoCore::RasterizerInterface* rasterizer;
    Tegra::MaxwellDeviceMemoryManager& device_memory;
    Vulkan::BufferCache& buffer_cache;

    const Device& device;
    const MemoryAllocator& memory_allocator;
    Scheduler& scheduler;
    StagingBufferPool& staging_pool;

    // Streamers
    VideoCommon::GuestStreamer<QueryCacheParams> guest_streamer;
    SamplesStreamer sample_streamer;
    TFBCounterStreamer tfb_streamer;
    PrimitivesSucceededStreamer primitives_succeeded_streamer;
    VideoCommon::StubStreamer<QueryCacheParams> primitives_needed_minus_succeeded_streamer;

    std::vector<std::pair<DAddr, DAddr>> little_cache;
    std::vector<std::pair<VkBuffer, VkDeviceSize>> buffers_to_upload_to;
    std::vector<size_t> redirect_cache;
    std::vector<std::vector<VkBufferCopy>> copies_setup;

    // Host conditional rendering data
    std::unique_ptr<ConditionalRenderingResolvePass> conditional_resolve_pass;
    vk::Buffer hcr_resolve_buffer;
    VkConditionalRenderingBeginInfoEXT hcr_setup;
    VkBuffer hcr_buffer;
    size_t hcr_offset;
    bool hcr_is_set;
    bool is_hcr_running;

    // maxwell3d
    Maxwell3D* maxwell3d;
};

QueryCacheRuntime::QueryCacheRuntime(VideoCore::RasterizerInterface* rasterizer,
                                     Tegra::MaxwellDeviceMemoryManager& device_memory_,
                                     Vulkan::BufferCache& buffer_cache_, const Device& device_,
                                     const MemoryAllocator& memory_allocator_,
                                     Scheduler& scheduler_, StagingBufferPool& staging_pool_,
                                     ComputePassDescriptorQueue& compute_pass_descriptor_queue,
                                     DescriptorPool& descriptor_pool) {
    impl = std::make_unique<QueryCacheRuntimeImpl>(
        *this, rasterizer, device_memory_, buffer_cache_, device_, memory_allocator_, scheduler_,
        staging_pool_, compute_pass_descriptor_queue, descriptor_pool);
}

void QueryCacheRuntime::Bind3DEngine(Maxwell3D* maxwell3d) {
    impl->maxwell3d = maxwell3d;
}

template <typename Func>
void QueryCacheRuntime::View3DRegs(Func&& func) {
    if (impl->maxwell3d) {
        func(*impl->maxwell3d);
    }
}

void QueryCacheRuntime::EndHostConditionalRendering() {
    PauseHostConditionalRendering();
    impl->hcr_is_set = false;
    impl->is_hcr_running = false;
    impl->hcr_buffer = nullptr;
    impl->hcr_offset = 0;
}

void QueryCacheRuntime::PauseHostConditionalRendering() {
    if (!impl->hcr_is_set) {
        return;
    }
    if (impl->is_hcr_running) {
        impl->scheduler.Record(
            [](vk::CommandBuffer cmdbuf) { cmdbuf.EndConditionalRenderingEXT(); });
    }
    impl->is_hcr_running = false;
}

void QueryCacheRuntime::ResumeHostConditionalRendering() {
    if (!impl->hcr_is_set) {
        return;
    }
    if (!impl->is_hcr_running) {
        impl->scheduler.Record([hcr_setup = impl->hcr_setup](vk::CommandBuffer cmdbuf) {
            cmdbuf.BeginConditionalRenderingEXT(hcr_setup);
        });
    }
    impl->is_hcr_running = true;
}

void QueryCacheRuntime::HostConditionalRenderingCompareValueImpl(VideoCommon::LookupData object,
                                                                 bool is_equal) {
    {
        std::scoped_lock lk(impl->buffer_cache.mutex);
        static constexpr auto sync_info = VideoCommon::ObtainBufferSynchronize::FullSynchronize;
        const auto post_op = VideoCommon::ObtainBufferOperation::DoNothing;
        const auto [buffer, offset] =
            impl->buffer_cache.ObtainCPUBuffer(object.address, 8, sync_info, post_op);
        impl->hcr_buffer = buffer->Handle();
        impl->hcr_offset = offset;
    }
    if (impl->hcr_is_set) {
        if (impl->hcr_setup.buffer == impl->hcr_buffer &&
            impl->hcr_setup.offset == impl->hcr_offset) {
            ResumeHostConditionalRendering();
            return;
        }
        PauseHostConditionalRendering();
    }
    impl->hcr_setup.buffer = impl->hcr_buffer;
    impl->hcr_setup.offset = impl->hcr_offset;
    impl->hcr_setup.flags = is_equal ? VK_CONDITIONAL_RENDERING_INVERTED_BIT_EXT : 0;
    impl->hcr_is_set = true;
    impl->is_hcr_running = false;
    ResumeHostConditionalRendering();
}

void QueryCacheRuntime::HostConditionalRenderingCompareBCImpl(DAddr address, bool is_equal) {
    VkBuffer to_resolve;
    u32 to_resolve_offset;
    {
        std::scoped_lock lk(impl->buffer_cache.mutex);
        static constexpr auto sync_info = VideoCommon::ObtainBufferSynchronize::NoSynchronize;
        const auto post_op = VideoCommon::ObtainBufferOperation::DoNothing;
        const auto [buffer, offset] =
            impl->buffer_cache.ObtainCPUBuffer(address, 24, sync_info, post_op);
        to_resolve = buffer->Handle();
        to_resolve_offset = static_cast<u32>(offset);
    }
    if (impl->is_hcr_running) {
        PauseHostConditionalRendering();
    }
    impl->conditional_resolve_pass->Resolve(*impl->hcr_resolve_buffer, to_resolve,
                                            to_resolve_offset, false);
    impl->hcr_setup.buffer = *impl->hcr_resolve_buffer;
    impl->hcr_setup.offset = 0;
    impl->hcr_setup.flags = is_equal ? 0 : VK_CONDITIONAL_RENDERING_INVERTED_BIT_EXT;
    impl->hcr_is_set = true;
    impl->is_hcr_running = false;
    ResumeHostConditionalRendering();
}

bool QueryCacheRuntime::HostConditionalRenderingCompareValue(VideoCommon::LookupData object_1,
                                                             [[maybe_unused]] bool qc_dirty) {
    if (!impl->device.IsExtConditionalRendering()) {
        return false;
    }
    HostConditionalRenderingCompareValueImpl(object_1, false);
    return true;
}

bool QueryCacheRuntime::HostConditionalRenderingCompareValues(VideoCommon::LookupData object_1,
                                                              VideoCommon::LookupData object_2,
                                                              bool qc_dirty, bool equal_check) {
    if (!impl->device.IsExtConditionalRendering()) {
        return false;
    }

    const auto check_in_bc = [&](DAddr address) {
        return impl->buffer_cache.IsRegionGpuModified(address, 8);
    };
    const auto check_value = [&](DAddr address) {
        u8* ptr = impl->device_memory.GetPointer<u8>(address);
        u64 value{};
        if (ptr != nullptr) {
            std::memcpy(&value, ptr, sizeof(value));
        }
        return value == 0;
    };
    std::array<VideoCommon::LookupData*, 2> objects{&object_1, &object_2};
    std::array<bool, 2> is_in_bc{};
    std::array<bool, 2> is_in_qc{};
    std::array<bool, 2> is_in_ac{};
    std::array<bool, 2> is_null{};
    {
        std::scoped_lock lk(impl->buffer_cache.mutex);
        for (size_t i = 0; i < 2; i++) {
            is_in_qc[i] = objects[i]->found_query != nullptr;
            is_in_bc[i] = !is_in_qc[i] && check_in_bc(objects[i]->address);
            is_in_ac[i] = is_in_qc[i] || is_in_bc[i];
        }
    }

    if (!is_in_ac[0] && !is_in_ac[1]) {
        EndHostConditionalRendering();
        return false;
    }

    if (!qc_dirty && !is_in_bc[0] && !is_in_bc[1]) {
        EndHostConditionalRendering();
        return false;
    }

    const bool is_gpu_high = Settings::IsGPULevelHigh();
    if (!is_gpu_high && impl->device.GetDriverID() == VK_DRIVER_ID_INTEL_PROPRIETARY_WINDOWS) {
        return true;
    }

    auto driver_id = impl->device.GetDriverID();
    if (driver_id == VK_DRIVER_ID_QUALCOMM_PROPRIETARY ||
        driver_id == VK_DRIVER_ID_ARM_PROPRIETARY || driver_id == VK_DRIVER_ID_MESA_TURNIP) {
        return true;
    }

    for (size_t i = 0; i < 2; i++) {
        is_null[i] = !is_in_ac[i] && check_value(objects[i]->address);
    }

    for (size_t i = 0; i < 2; i++) {
        if (is_null[i]) {
            size_t j = (i + 1) % 2;
            HostConditionalRenderingCompareValueImpl(*objects[j], equal_check);
            return true;
        }
    }

    if (!is_gpu_high) {
        return true;
    }

    if (!is_in_bc[0] && !is_in_bc[1]) {
        // Both queries are in query cache, it's best to just flush.
        return true;
    }
    HostConditionalRenderingCompareBCImpl(object_1.address, equal_check);
    return true;
}

QueryCacheRuntime::~QueryCacheRuntime() = default;

VideoCommon::StreamerInterface* QueryCacheRuntime::GetStreamerInterface(QueryType query_type) {
    switch (query_type) {
    case QueryType::Payload:
        return &impl->guest_streamer;
    case QueryType::ZPassPixelCount64:
        return &impl->sample_streamer;
    case QueryType::StreamingByteCount:
        return &impl->tfb_streamer;
    case QueryType::StreamingPrimitivesNeeded:
    case QueryType::VtgPrimitivesOut:
    case QueryType::StreamingPrimitivesSucceeded:
        return &impl->primitives_succeeded_streamer;
    case QueryType::StreamingPrimitivesNeededMinusSucceeded:
        return &impl->primitives_needed_minus_succeeded_streamer;
    default:
        return nullptr;
    }
}

void QueryCacheRuntime::Barriers(bool is_prebarrier) {
    static constexpr VkMemoryBarrier READ_BARRIER{
        .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
        .pNext = nullptr,
        .srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT,
    };
    static constexpr VkMemoryBarrier WRITE_BARRIER{
        .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
        .pNext = nullptr,
        .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
    };
    impl->scheduler.RequestOutsideRenderPassOperationContext();
    if (is_prebarrier) {
        impl->scheduler.Record([](vk::CommandBuffer cmdbuf) {
            cmdbuf.PipelineBarrier(VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                                   VK_PIPELINE_STAGE_TRANSFER_BIT, 0, READ_BARRIER);
        });
    } else {
        impl->scheduler.Record([](vk::CommandBuffer cmdbuf) {
            cmdbuf.PipelineBarrier(VK_PIPELINE_STAGE_TRANSFER_BIT,
                                   VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, WRITE_BARRIER);
        });
    }
}

template <typename SyncValuesType>
void QueryCacheRuntime::SyncValues(std::span<SyncValuesType> values, VkBuffer base_src_buffer) {
    if (values.size() == 0) {
        return;
    }
    impl->redirect_cache.clear();
    impl->little_cache.clear();
    size_t total_size = 0;
    for (auto& sync_val : values) {
        total_size += sync_val.size;
        bool found = false;
        DAddr base = Common::AlignDown(sync_val.address, Core::DEVICE_PAGESIZE);
        DAddr base_end = base + Core::DEVICE_PAGESIZE;
        for (size_t i = 0; i < impl->little_cache.size(); i++) {
            const auto set_found = [&] {
                impl->redirect_cache.push_back(i);
                found = true;
            };
            auto& loc = impl->little_cache[i];
            if (base < loc.second && loc.first < base_end) {
                set_found();
                break;
            }
            if (loc.first == base_end) {
                loc.first = base;
                set_found();
                break;
            }
            if (loc.second == base) {
                loc.second = base_end;
                set_found();
                break;
            }
        }
        if (!found) {
            impl->redirect_cache.push_back(impl->little_cache.size());
            impl->little_cache.emplace_back(base, base_end);
        }
    }

    // Vulkan part.
    std::scoped_lock lk(impl->buffer_cache.mutex);
    impl->buffer_cache.BufferOperations([&] {
        impl->buffers_to_upload_to.clear();
        for (auto& pair : impl->little_cache) {
            static constexpr auto sync_info = VideoCommon::ObtainBufferSynchronize::FullSynchronize;
            const auto post_op = VideoCommon::ObtainBufferOperation::DoNothing;
            const auto [buffer, offset] = impl->buffer_cache.ObtainCPUBuffer(
                pair.first, static_cast<u32>(pair.second - pair.first), sync_info, post_op);
            impl->buffers_to_upload_to.emplace_back(buffer->Handle(), offset);
        }
    });

    VkBuffer src_buffer;
    [[maybe_unused]] StagingBufferRef ref;
    impl->copies_setup.clear();
    impl->copies_setup.resize(impl->little_cache.size());
    if constexpr (SyncValuesType::GeneratesBaseBuffer) {
        ref = impl->staging_pool.Request(total_size, MemoryUsage::Upload);
        size_t current_offset = ref.offset;
        size_t accumulated_size = 0;
        for (size_t i = 0; i < values.size(); i++) {
            size_t which_copy = impl->redirect_cache[i];
            impl->copies_setup[which_copy].emplace_back(VkBufferCopy{
                .srcOffset = current_offset + accumulated_size,
                .dstOffset = impl->buffers_to_upload_to[which_copy].second + values[i].address -
                             impl->little_cache[which_copy].first,
                .size = values[i].size,
            });
            std::memcpy(ref.mapped_span.data() + accumulated_size, &values[i].value,
                        values[i].size);
            accumulated_size += values[i].size;
        }
        src_buffer = ref.buffer;
    } else {
        for (size_t i = 0; i < values.size(); i++) {
            size_t which_copy = impl->redirect_cache[i];
            impl->copies_setup[which_copy].emplace_back(VkBufferCopy{
                .srcOffset = values[i].offset,
                .dstOffset = impl->buffers_to_upload_to[which_copy].second + values[i].address -
                             impl->little_cache[which_copy].first,
                .size = values[i].size,
            });
        }
        src_buffer = base_src_buffer;
    }

    impl->scheduler.RequestOutsideRenderPassOperationContext();
    impl->scheduler.Record([src_buffer, dst_buffers = std::move(impl->buffers_to_upload_to),
                            vk_copies = std::move(impl->copies_setup)](vk::CommandBuffer cmdbuf) {
        size_t size = dst_buffers.size();
        for (size_t i = 0; i < size; i++) {
            cmdbuf.CopyBuffer(src_buffer, dst_buffers[i].first, vk_copies[i]);
        }
    });
}

} // namespace Vulkan

namespace VideoCommon {

template class QueryCacheBase<Vulkan::QueryCacheParams>;

} // namespace VideoCommon
