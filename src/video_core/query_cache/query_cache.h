// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <array>
#include <deque>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <utility>

#include "common/assert.h"
#include "common/common_types.h"
#include "common/logging/log.h"
#include "common/scope_exit.h"
#include "common/settings.h"
#include "video_core/engines/maxwell_3d.h"
#include "video_core/gpu.h"
#include "video_core/host1x/gpu_device_memory_manager.h"
#include "video_core/memory_manager.h"
#include "video_core/query_cache/bank_base.h"
#include "video_core/query_cache/query_base.h"
#include "video_core/query_cache/query_cache_base.h"
#include "video_core/query_cache/query_stream.h"
#include "video_core/query_cache/types.h"

namespace VideoCommon {

using Maxwell = Tegra::Engines::Maxwell3D;

struct SyncValuesStruct {
    VAddr address;
    u64 value;
    u64 size;

    static constexpr bool GeneratesBaseBuffer = true;
};

template <typename Traits>
class GuestStreamer : public SimpleStreamer<GuestQuery> {
public:
    using RuntimeType = typename Traits::RuntimeType;

    GuestStreamer(size_t id_, RuntimeType& runtime_)
        : SimpleStreamer<GuestQuery>(id_), runtime{runtime_} {}

    virtual ~GuestStreamer() = default;

    size_t WriteCounter(VAddr address, bool has_timestamp, u32 value,
                        std::optional<u32> subreport = std::nullopt) override {
        auto new_id = BuildQuery(has_timestamp, address, static_cast<u64>(value));
        pending_sync.push_back(new_id);
        return new_id;
    }

    bool HasPendingSync() const override {
        return !pending_sync.empty();
    }

    void SyncWrites() override {
        if (pending_sync.empty()) {
            return;
        }
        std::vector<SyncValuesStruct> sync_values;
        sync_values.reserve(pending_sync.size());
        for (size_t pending_id : pending_sync) {
            auto& query = slot_queries[pending_id];
            if (True(query.flags & QueryFlagBits::IsRewritten) ||
                True(query.flags & QueryFlagBits::IsInvalidated)) {
                continue;
            }
            query.flags |= QueryFlagBits::IsHostSynced;
            sync_values.emplace_back(SyncValuesStruct{
                .address = query.guest_address,
                .value = query.value,
                .size = static_cast<u64>(True(query.flags & QueryFlagBits::HasTimestamp) ? 8 : 4)});
        }
        pending_sync.clear();
        if (sync_values.size() > 0) {
            runtime.template SyncValues<SyncValuesStruct>(sync_values);
        }
    }

private:
    RuntimeType& runtime;
    std::deque<size_t> pending_sync;
};

template <typename Traits>
class StubStreamer : public GuestStreamer<Traits> {
public:
    using RuntimeType = typename Traits::RuntimeType;

    StubStreamer(size_t id_, RuntimeType& runtime_, u32 stub_value_)
        : GuestStreamer<Traits>(id_, runtime_), stub_value{stub_value_} {}

    ~StubStreamer() override = default;

    size_t WriteCounter(VAddr address, bool has_timestamp, [[maybe_unused]] u32 value,
                        std::optional<u32> subreport = std::nullopt) override {
        size_t new_id =
            GuestStreamer<Traits>::WriteCounter(address, has_timestamp, stub_value, subreport);
        return new_id;
    }

private:
    u32 stub_value;
};

template <typename Traits>
struct QueryCacheBase<Traits>::QueryCacheBaseImpl {
    using RuntimeType = typename Traits::RuntimeType;

    QueryCacheBaseImpl(QueryCacheBase<Traits>* owner_, VideoCore::RasterizerInterface& rasterizer_,
                       Tegra::MaxwellDeviceMemoryManager& device_memory_, RuntimeType& runtime_,
                       Tegra::GPU& gpu_)
        : owner{owner_}, rasterizer{rasterizer_}, device_memory{device_memory_}, runtime{runtime_},
          gpu{gpu_} {
        streamer_mask = 0;
        for (size_t i = 0; i < static_cast<size_t>(QueryType::MaxQueryTypes); i++) {
            streamers[i] = runtime.GetStreamerInterface(static_cast<QueryType>(i));
            if (streamers[i]) {
                streamer_mask |= 1ULL << streamers[i]->GetId();
            }
        }
    }

    template <typename Func>
    void ForEachStreamerIn(u64 mask, Func&& func) {
        static constexpr bool RETURNS_BOOL =
            std::is_same_v<std::invoke_result<Func, StreamerInterface*>, bool>;
        while (mask != 0) {
            size_t position = std::countr_zero(mask);
            mask &= ~(1ULL << position);
            if constexpr (RETURNS_BOOL) {
                if (func(streamers[position])) {
                    return;
                }
            } else {
                func(streamers[position]);
            }
        }
    }

    template <typename Func>
    void ForEachStreamer(Func&& func) {
        ForEachStreamerIn(streamer_mask, func);
    }

    QueryBase* ObtainQuery(QueryCacheBase<Traits>::QueryLocation location) {
        size_t which_stream = location.stream_id.Value();
        auto* streamer = streamers[which_stream];
        if (!streamer) {
            return nullptr;
        }
        return streamer->GetQuery(location.query_id.Value());
    }

    QueryCacheBase<Traits>* owner;
    VideoCore::RasterizerInterface& rasterizer;
    Tegra::MaxwellDeviceMemoryManager& device_memory;
    RuntimeType& runtime;
    Tegra::GPU& gpu;
    std::array<StreamerInterface*, static_cast<size_t>(QueryType::MaxQueryTypes)> streamers;
    u64 streamer_mask;
    std::mutex flush_guard;
    std::deque<u64> flushes_pending;
    std::vector<QueryCacheBase<Traits>::QueryLocation> pending_unregister;
};

template <typename Traits>
QueryCacheBase<Traits>::QueryCacheBase(Tegra::GPU& gpu_,
                                       VideoCore::RasterizerInterface& rasterizer_,
                                       Tegra::MaxwellDeviceMemoryManager& device_memory_,
                                       RuntimeType& runtime_)
    : cached_queries{} {
    impl = std::make_unique<QueryCacheBase<Traits>::QueryCacheBaseImpl>(
        this, rasterizer_, device_memory_, runtime_, gpu_);
}

template <typename Traits>
QueryCacheBase<Traits>::~QueryCacheBase() = default;

template <typename Traits>
void QueryCacheBase<Traits>::CounterEnable(QueryType counter_type, bool is_enabled) {
    size_t index = static_cast<size_t>(counter_type);
    StreamerInterface* streamer = impl->streamers[index];
    if (!streamer) [[unlikely]] {
        UNREACHABLE();
        return;
    }
    if (is_enabled) {
        streamer->StartCounter();
    } else {
        streamer->PauseCounter();
    }
}

template <typename Traits>
void QueryCacheBase<Traits>::CounterClose(QueryType counter_type) {
    size_t index = static_cast<size_t>(counter_type);
    StreamerInterface* streamer = impl->streamers[index];
    if (!streamer) [[unlikely]] {
        UNREACHABLE();
        return;
    }
    streamer->CloseCounter();
}

template <typename Traits>
void QueryCacheBase<Traits>::CounterReset(QueryType counter_type) {
    size_t index = static_cast<size_t>(counter_type);
    StreamerInterface* streamer = impl->streamers[index];
    if (!streamer) [[unlikely]] {
        UNIMPLEMENTED();
        return;
    }
    streamer->ResetCounter();
}

template <typename Traits>
void QueryCacheBase<Traits>::BindToChannel(s32 id) {
    VideoCommon::ChannelSetupCaches<VideoCommon::ChannelInfo>::BindToChannel(id);
    impl->runtime.Bind3DEngine(maxwell3d);
}

template <typename Traits>
void QueryCacheBase<Traits>::CounterReport(GPUVAddr addr, QueryType counter_type,
                                           QueryPropertiesFlags flags, u32 payload, u32 subreport) {
    const bool has_timestamp = True(flags & QueryPropertiesFlags::HasTimeout);
    const bool is_fence = True(flags & QueryPropertiesFlags::IsAFence);
    size_t streamer_id = static_cast<size_t>(counter_type);
    auto* streamer = impl->streamers[streamer_id];
    if (streamer == nullptr) [[unlikely]] {
        counter_type = QueryType::Payload;
        payload = 1U;
        streamer_id = static_cast<size_t>(counter_type);
        streamer = impl->streamers[streamer_id];
    }
    auto cpu_addr_opt = gpu_memory->GpuToCpuAddress(addr);
    if (!cpu_addr_opt) [[unlikely]] {
        return;
    }
    DAddr cpu_addr = *cpu_addr_opt;
    const size_t new_query_id = streamer->WriteCounter(cpu_addr, has_timestamp, payload, subreport);
    auto* query = streamer->GetQuery(new_query_id);
    if (is_fence) {
        query->flags |= QueryFlagBits::IsFence;
    }
    QueryLocation query_location{};
    query_location.stream_id.Assign(static_cast<u32>(streamer_id));
    query_location.query_id.Assign(static_cast<u32>(new_query_id));
    const auto gen_caching_indexing = [](VAddr cur_addr) {
        return std::make_pair<u64, u32>(cur_addr >> Core::DEVICE_PAGEBITS,
                                        static_cast<u32>(cur_addr & Core::DEVICE_PAGEMASK));
    };
    u8* pointer = impl->device_memory.template GetPointer<u8>(cpu_addr);
    u8* pointer_timestamp = impl->device_memory.template GetPointer<u8>(cpu_addr + 8);
    bool is_synced = !Settings::IsGPULevelHigh() && is_fence;
    std::function<void()> operation([this, is_synced, streamer, query_base = query, query_location,
                                     pointer, pointer_timestamp] {
        if (True(query_base->flags & QueryFlagBits::IsInvalidated)) {
            if (!is_synced) [[likely]] {
                impl->pending_unregister.push_back(query_location);
            }
            return;
        }
        if (False(query_base->flags & QueryFlagBits::IsFinalValueSynced)) [[unlikely]] {
            ASSERT(false);
            return;
        }
        query_base->value += streamer->GetAmendValue();
        streamer->SetAccumulationValue(query_base->value);
        if (True(query_base->flags & QueryFlagBits::HasTimestamp)) {
            u64 timestamp = impl->gpu.GetTicks();
            std::memcpy(pointer_timestamp, &timestamp, sizeof(timestamp));
            std::memcpy(pointer, &query_base->value, sizeof(query_base->value));
        } else {
            u32 value = static_cast<u32>(query_base->value);
            std::memcpy(pointer, &value, sizeof(value));
        }
        if (!is_synced) [[likely]] {
            impl->pending_unregister.push_back(query_location);
        }
    });
    if (is_fence) {
        impl->rasterizer.SignalFence(std::move(operation));
    } else {
        if (!Settings::IsGPULevelHigh() && counter_type == QueryType::Payload) {
            if (has_timestamp) {
                u64 timestamp = impl->gpu.GetTicks();
                u64 value = static_cast<u64>(payload);
                std::memcpy(pointer_timestamp, &timestamp, sizeof(timestamp));
                std::memcpy(pointer, &value, sizeof(value));
            } else {
                std::memcpy(pointer, &payload, sizeof(payload));
            }
            streamer->Free(new_query_id);
            return;
        }
        impl->rasterizer.SyncOperation(std::move(operation));
    }
    if (is_synced) {
        streamer->Free(new_query_id);
        return;
    }
    auto [cont_addr, base] = gen_caching_indexing(cpu_addr);
    {
        std::scoped_lock lock(cache_mutex);
        auto it1 = cached_queries.try_emplace(cont_addr);
        auto& sub_container = it1.first->second;
        auto it_current = sub_container.find(base);
        if (it_current == sub_container.end()) {
            sub_container.insert_or_assign(base, query_location);
            return;
        }
        auto* old_query = impl->ObtainQuery(it_current->second);
        old_query->flags |= QueryFlagBits::IsRewritten;
        sub_container.insert_or_assign(base, query_location);
    }
}

template <typename Traits>
void QueryCacheBase<Traits>::UnregisterPending() {
    const auto gen_caching_indexing = [](VAddr cur_addr) {
        return std::make_pair<u64, u32>(cur_addr >> Core::DEVICE_PAGEBITS,
                                        static_cast<u32>(cur_addr & Core::DEVICE_PAGEMASK));
    };
    std::scoped_lock lock(cache_mutex);
    for (QueryLocation loc : impl->pending_unregister) {
        const auto [streamer_id, query_id] = loc.unpack();
        auto* streamer = impl->streamers[streamer_id];
        if (!streamer) [[unlikely]] {
            continue;
        }
        auto* query = streamer->GetQuery(query_id);
        auto [cont_addr, base] = gen_caching_indexing(query->guest_address);
        auto it1 = cached_queries.find(cont_addr);
        if (it1 != cached_queries.end()) {
            auto it2 = it1->second.find(base);
            if (it2 != it1->second.end()) {
                if (it2->second.raw == loc.raw) {
                    it1->second.erase(it2);
                }
            }
        }
        streamer->Free(query_id);
    }
    impl->pending_unregister.clear();
}

template <typename Traits>
void QueryCacheBase<Traits>::NotifyWFI() {
    bool should_sync = false;
    impl->ForEachStreamer(
        [&should_sync](StreamerInterface* streamer) { should_sync |= streamer->HasPendingSync(); });
    if (!should_sync) {
        return;
    }

    impl->ForEachStreamer([](StreamerInterface* streamer) { streamer->PresyncWrites(); });
    impl->runtime.Barriers(true);
    impl->ForEachStreamer([](StreamerInterface* streamer) { streamer->SyncWrites(); });
    impl->runtime.Barriers(false);
}

template <typename Traits>
void QueryCacheBase<Traits>::NotifySegment(bool resume) {
    if (resume) {
        impl->runtime.ResumeHostConditionalRendering();
    } else {
        CounterClose(VideoCommon::QueryType::ZPassPixelCount64);
        CounterClose(VideoCommon::QueryType::StreamingByteCount);
        impl->runtime.PauseHostConditionalRendering();
    }
}

template <typename Traits>
bool QueryCacheBase<Traits>::AccelerateHostConditionalRendering() {
    bool qc_dirty = false;
    const auto gen_lookup = [this, &qc_dirty](GPUVAddr address) -> VideoCommon::LookupData {
        auto cpu_addr_opt = gpu_memory->GpuToCpuAddress(address);
        if (!cpu_addr_opt) [[unlikely]] {
            return VideoCommon::LookupData{
                .address = 0,
                .found_query = nullptr,
            };
        }
        VAddr cpu_addr = *cpu_addr_opt;
        std::scoped_lock lock(cache_mutex);
        auto it1 = cached_queries.find(cpu_addr >> Core::DEVICE_PAGEBITS);
        if (it1 == cached_queries.end()) {
            return VideoCommon::LookupData{
                .address = cpu_addr,
                .found_query = nullptr,
            };
        }
        auto& sub_container = it1->second;
        auto it_current = sub_container.find(cpu_addr & Core::DEVICE_PAGEMASK);

        if (it_current == sub_container.end()) {
            auto it_current_2 = sub_container.find((cpu_addr & Core::DEVICE_PAGEMASK) + 4);
            if (it_current_2 == sub_container.end()) {
                return VideoCommon::LookupData{
                    .address = cpu_addr,
                    .found_query = nullptr,
                };
            }
        }
        auto* query = impl->ObtainQuery(it_current->second);
        qc_dirty |= True(query->flags & QueryFlagBits::IsHostManaged) &&
                    False(query->flags & QueryFlagBits::IsGuestSynced);
        return VideoCommon::LookupData{
            .address = cpu_addr,
            .found_query = query,
        };
    };

    auto& regs = maxwell3d->regs;
    if (regs.render_enable_override != Maxwell::Regs::RenderEnable::Override::UseRenderEnable) {
        impl->runtime.EndHostConditionalRendering();
        return false;
    }
    const ComparisonMode mode = static_cast<ComparisonMode>(regs.render_enable.mode);
    const GPUVAddr address = regs.render_enable.Address();
    switch (mode) {
    case ComparisonMode::True:
        impl->runtime.EndHostConditionalRendering();
        return false;
    case ComparisonMode::False:
        impl->runtime.EndHostConditionalRendering();
        return false;
    case ComparisonMode::Conditional: {
        VideoCommon::LookupData object_1{gen_lookup(address)};
        return impl->runtime.HostConditionalRenderingCompareValue(object_1, qc_dirty);
    }
    case ComparisonMode::IfEqual: {
        VideoCommon::LookupData object_1{gen_lookup(address)};
        VideoCommon::LookupData object_2{gen_lookup(address + 16)};
        return impl->runtime.HostConditionalRenderingCompareValues(object_1, object_2, qc_dirty,
                                                                   true);
    }
    case ComparisonMode::IfNotEqual: {
        VideoCommon::LookupData object_1{gen_lookup(address)};
        VideoCommon::LookupData object_2{gen_lookup(address + 16)};
        return impl->runtime.HostConditionalRenderingCompareValues(object_1, object_2, qc_dirty,
                                                                   false);
    }
    default:
        return false;
    }
}

// Async downloads
template <typename Traits>
void QueryCacheBase<Traits>::CommitAsyncFlushes() {
    // Make sure to have the results synced in Host.
    NotifyWFI();

    u64 mask{};
    {
        std::scoped_lock lk(impl->flush_guard);
        impl->ForEachStreamer([&mask](StreamerInterface* streamer) {
            bool local_result = streamer->HasUnsyncedQueries();
            if (local_result) {
                mask |= 1ULL << streamer->GetId();
            }
        });
        impl->flushes_pending.push_back(mask);
    }
    std::function<void()> func([this] { UnregisterPending(); });
    impl->rasterizer.SyncOperation(std::move(func));
    if (mask == 0) {
        return;
    }
    u64 ran_mask = ~mask;
    while (mask) {
        impl->ForEachStreamerIn(mask, [&mask, &ran_mask](StreamerInterface* streamer) {
            u64 dep_mask = streamer->GetDependentMask();
            if ((dep_mask & ~ran_mask) != 0) {
                return;
            }
            u64 index = streamer->GetId();
            ran_mask |= (1ULL << index);
            mask &= ~(1ULL << index);
            streamer->PushUnsyncedQueries();
        });
    }
}

template <typename Traits>
bool QueryCacheBase<Traits>::HasUncommittedFlushes() const {
    bool result = false;
    impl->ForEachStreamer([&result](StreamerInterface* streamer) {
        result |= streamer->HasUnsyncedQueries();
        return result;
    });
    return result;
}

template <typename Traits>
bool QueryCacheBase<Traits>::ShouldWaitAsyncFlushes() {
    std::scoped_lock lk(impl->flush_guard);
    return !impl->flushes_pending.empty() && impl->flushes_pending.front() != 0ULL;
}

template <typename Traits>
void QueryCacheBase<Traits>::PopAsyncFlushes() {
    u64 mask;
    {
        std::scoped_lock lk(impl->flush_guard);
        mask = impl->flushes_pending.front();
        impl->flushes_pending.pop_front();
    }
    if (mask == 0) {
        return;
    }
    u64 ran_mask = ~mask;
    while (mask) {
        impl->ForEachStreamerIn(mask, [&mask, &ran_mask](StreamerInterface* streamer) {
            u64 dep_mask = streamer->GetDependenceMask();
            if ((dep_mask & ~ran_mask) != 0) {
                return;
            }
            u64 index = streamer->GetId();
            ran_mask |= (1ULL << index);
            mask &= ~(1ULL << index);
            streamer->PopUnsyncedQueries();
        });
    }
}

// Invalidation

template <typename Traits>
void QueryCacheBase<Traits>::InvalidateQuery(QueryCacheBase<Traits>::QueryLocation location) {
    auto* query_base = impl->ObtainQuery(location);
    if (!query_base) {
        return;
    }
    query_base->flags |= QueryFlagBits::IsInvalidated;
}

template <typename Traits>
bool QueryCacheBase<Traits>::IsQueryDirty(QueryCacheBase<Traits>::QueryLocation location) {
    auto* query_base = impl->ObtainQuery(location);
    if (!query_base) {
        return false;
    }
    return True(query_base->flags & QueryFlagBits::IsHostManaged) &&
           False(query_base->flags & QueryFlagBits::IsGuestSynced);
}

template <typename Traits>
bool QueryCacheBase<Traits>::SemiFlushQueryDirty(QueryCacheBase<Traits>::QueryLocation location) {
    auto* query_base = impl->ObtainQuery(location);
    if (!query_base) {
        return false;
    }
    if (True(query_base->flags & QueryFlagBits::IsFinalValueSynced) &&
        False(query_base->flags & QueryFlagBits::IsGuestSynced)) {
        auto* ptr = impl->device_memory.template GetPointer<u8>(query_base->guest_address);
        if (True(query_base->flags & QueryFlagBits::HasTimestamp)) {
            std::memcpy(ptr, &query_base->value, sizeof(query_base->value));
            return false;
        }
        u32 value_l = static_cast<u32>(query_base->value);
        std::memcpy(ptr, &value_l, sizeof(value_l));
        return false;
    }
    return True(query_base->flags & QueryFlagBits::IsHostManaged) &&
           False(query_base->flags & QueryFlagBits::IsGuestSynced);
}

template <typename Traits>
void QueryCacheBase<Traits>::RequestGuestHostSync() {
    impl->rasterizer.ReleaseFences();
}

} // namespace VideoCommon
