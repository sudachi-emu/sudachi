// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <atomic>
#include <mutex>
#include <set>
#include <shared_mutex>

#include "common/host_memory.h"
#include "common/intrusive_red_black_tree.h"

namespace Common {

struct SeparateHeapMap {
    Common::IntrusiveRedBlackTreeNode addr_node{};
    Common::IntrusiveRedBlackTreeNode tick_node{};
    VAddr vaddr{};
    PAddr paddr{};
    size_t size{};
    size_t tick{};
    MemoryPermission perm{};
    bool is_resident{};
};

struct SeparateHeapMapAddrComparator {
    static constexpr int Compare(const SeparateHeapMap& lhs, const SeparateHeapMap& rhs) {
        if (lhs.vaddr < rhs.vaddr) {
            return -1;
        } else if (lhs.vaddr <= (rhs.vaddr + rhs.size - 1)) {
            return 0;
        } else {
            return 1;
        }
    }
};

struct SeparateHeapMapTickComparator {
    static constexpr int Compare(const SeparateHeapMap& lhs, const SeparateHeapMap& rhs) {
        if (lhs.tick < rhs.tick) {
            return -1;
        } else if (lhs.tick > rhs.tick) {
            return 1;
        } else {
            return SeparateHeapMapAddrComparator::Compare(lhs, rhs);
        }
    }
};

class HeapTracker {
public:
    explicit HeapTracker(Common::HostMemory& buffer);
    ~HeapTracker();

    void Map(size_t virtual_offset, size_t host_offset, size_t length, MemoryPermission perm,
             bool is_separate_heap);
    void Unmap(size_t virtual_offset, size_t size, bool is_separate_heap);
    void Protect(size_t virtual_offset, size_t length, MemoryPermission perm);
    u8* VirtualBasePointer() {
        return m_buffer.VirtualBasePointer();
    }

    bool DeferredMapSeparateHeap(u8* fault_address);
    bool DeferredMapSeparateHeap(size_t virtual_offset);

private:
    using AddrTreeTraits =
        Common::IntrusiveRedBlackTreeMemberTraitsDeferredAssert<&SeparateHeapMap::addr_node>;
    using AddrTree = AddrTreeTraits::TreeType<SeparateHeapMapAddrComparator>;

    using TickTreeTraits =
        Common::IntrusiveRedBlackTreeMemberTraitsDeferredAssert<&SeparateHeapMap::tick_node>;
    using TickTree = TickTreeTraits::TreeType<SeparateHeapMapTickComparator>;

    AddrTree m_mappings{};
    TickTree m_resident_mappings{};

private:
    void SplitHeapMap(VAddr offset, size_t size);
    void SplitHeapMapLocked(VAddr offset);

    AddrTree::iterator GetNearestHeapMapLocked(VAddr offset);

    void RebuildSeparateHeapAddressSpace();

private:
    Common::HostMemory& m_buffer;
    const s64 m_max_resident_map_count;

    std::shared_mutex m_rebuild_lock{};
    std::mutex m_lock{};
    s64 m_map_count{};
    s64 m_resident_map_count{};
    size_t m_tick{};
};

} // namespace Common
