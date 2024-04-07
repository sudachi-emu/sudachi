// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <mutex>
#include <boost/icl/interval_set.hpp>

namespace Common {

class FreeRegionManager {
public:
    explicit FreeRegionManager() = default;
    ~FreeRegionManager() = default;

    void SetAddressSpace(void* start, size_t size) {
        this->FreeBlock(start, size);
    }

    std::pair<void*, size_t> FreeBlock(void* block_ptr, size_t size) {
        std::scoped_lock lk(m_mutex);

        // Check to see if we are adjacent to any regions.
        auto start_address = reinterpret_cast<uintptr_t>(block_ptr);
        auto end_address = start_address + size;
        auto it = m_free_regions.find({start_address - 1, end_address + 1});

        // If we are, join with them, ensuring we stay in bounds.
        if (it != m_free_regions.end()) {
            start_address = std::min(start_address, it->lower());
            end_address = std::max(end_address, it->upper());
        }

        // Free the relevant region.
        m_free_regions.insert({start_address, end_address});

        // Return the adjusted pointers.
        block_ptr = reinterpret_cast<void*>(start_address);
        size = end_address - start_address;
        return {block_ptr, size};
    }

    void AllocateBlock(void* block_ptr, size_t size) {
        std::scoped_lock lk(m_mutex);

        auto address = reinterpret_cast<uintptr_t>(block_ptr);
        m_free_regions.subtract({address, address + size});
    }

private:
    std::mutex m_mutex;
    boost::icl::interval_set<uintptr_t> m_free_regions;
};

} // namespace Common
