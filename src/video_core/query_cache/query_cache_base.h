// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <functional>
#include <mutex>
#include <optional>
#include <span>
#include <unordered_map>
#include <utility>

#include "common/assert.h"
#include "common/bit_field.h"
#include "common/common_types.h"
#include "video_core/control/channel_state_cache.h"
#include "video_core/host1x/gpu_device_memory_manager.h"
#include "video_core/query_cache/query_base.h"
#include "video_core/query_cache/types.h"

namespace VideoCore {
class RasterizerInterface;
}

namespace Tegra {
class GPU;
}

namespace VideoCommon {

struct LookupData {
    VAddr address;
    QueryBase* found_query;
};

template <typename Traits>
class QueryCacheBase : public VideoCommon::ChannelSetupCaches<VideoCommon::ChannelInfo> {
    using RuntimeType = typename Traits::RuntimeType;

public:
    union QueryLocation {
        BitField<27, 5, u32> stream_id;
        BitField<0, 27, u32> query_id;
        u32 raw;

        std::pair<size_t, size_t> unpack() const {
            return {static_cast<size_t>(stream_id.Value()), static_cast<size_t>(query_id.Value())};
        }
    };

    explicit QueryCacheBase(Tegra::GPU& gpu, VideoCore::RasterizerInterface& rasterizer_,
                            Tegra::MaxwellDeviceMemoryManager& device_memory_,
                            RuntimeType& runtime_);

    ~QueryCacheBase();

    void InvalidateRegion(VAddr addr, std::size_t size) {
        IterateCache<true>(addr, size,
                           [this](QueryLocation location) { InvalidateQuery(location); });
    }

    void FlushRegion(VAddr addr, std::size_t size) {
        bool result = false;
        IterateCache<false>(addr, size, [this, &result](QueryLocation location) {
            result |= SemiFlushQueryDirty(location);
            return result;
        });
        if (result) {
            RequestGuestHostSync();
        }
    }

    static u64 BuildMask(std::span<const QueryType> types) {
        u64 mask = 0;
        for (auto query_type : types) {
            mask |= 1ULL << (static_cast<u64>(query_type));
        }
        return mask;
    }

    /// Return true when a CPU region is modified from the GPU
    [[nodiscard]] bool IsRegionGpuModified(VAddr addr, size_t size) {
        bool result = false;
        IterateCache<false>(addr, size, [this, &result](QueryLocation location) {
            result |= IsQueryDirty(location);
            return result;
        });
        return result;
    }

    void CounterEnable(QueryType counter_type, bool is_enabled);

    void CounterReset(QueryType counter_type);

    void CounterClose(QueryType counter_type);

    void CounterReport(GPUVAddr addr, QueryType counter_type, QueryPropertiesFlags flags,
                       u32 payload, u32 subreport);

    void NotifyWFI();

    bool AccelerateHostConditionalRendering();

    // Async downloads
    void CommitAsyncFlushes();

    bool HasUncommittedFlushes() const;

    bool ShouldWaitAsyncFlushes();

    void PopAsyncFlushes();

    void NotifySegment(bool resume);

    void BindToChannel(s32 id) override;

protected:
    template <bool remove_from_cache, typename Func>
    void IterateCache(VAddr addr, std::size_t size, Func&& func) {
        static constexpr bool RETURNS_BOOL =
            std::is_same_v<std::invoke_result<Func, QueryLocation>, bool>;
        const u64 addr_begin = addr;
        const u64 addr_end = addr_begin + size;

        const u64 page_end = addr_end >> Core::DEVICE_PAGEBITS;
        std::scoped_lock lock(cache_mutex);
        for (u64 page = addr_begin >> Core::DEVICE_PAGEBITS; page <= page_end; ++page) {
            const u64 page_start = page << Core::DEVICE_PAGEBITS;
            const auto in_range = [page_start, addr_begin, addr_end](const u32 query_location) {
                const u64 cache_begin = page_start + query_location;
                const u64 cache_end = cache_begin + sizeof(u32);
                return cache_begin < addr_end && addr_begin < cache_end;
            };
            const auto& it = cached_queries.find(page);
            if (it == std::end(cached_queries)) {
                continue;
            }
            auto& contents = it->second;
            for (auto& query : contents) {
                if (!in_range(query.first)) {
                    continue;
                }
                if constexpr (RETURNS_BOOL) {
                    if (func(query.second)) {
                        return;
                    }
                } else {
                    func(query.second);
                }
            }
            if constexpr (remove_from_cache) {
                const auto in_range2 = [&](const std::pair<u32, QueryLocation>& pair) {
                    return in_range(pair.first);
                };
                std::erase_if(contents, in_range2);
            }
        }
    }

    using ContentCache = std::unordered_map<u64, std::unordered_map<u32, QueryLocation>>;

    void InvalidateQuery(QueryLocation location);
    bool IsQueryDirty(QueryLocation location);
    bool SemiFlushQueryDirty(QueryLocation location);
    void RequestGuestHostSync();
    void UnregisterPending();

    std::unordered_map<u64, std::unordered_map<u32, QueryLocation>> cached_queries;
    std::mutex cache_mutex;

    struct QueryCacheBaseImpl;
    friend struct QueryCacheBaseImpl;
    friend RuntimeType;

    std::unique_ptr<QueryCacheBaseImpl> impl;
};

} // namespace VideoCommon
