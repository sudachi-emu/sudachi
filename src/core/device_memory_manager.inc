// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <atomic>
#include <limits>
#include <memory>
#include <type_traits>

#include "common/address_space.h"
#include "common/address_space.inc"
#include "common/alignment.h"
#include "common/assert.h"
#include "common/div_ceil.h"
#include "common/scope_exit.h"
#include "common/settings.h"
#include "core/device_memory.h"
#include "core/device_memory_manager.h"
#include "core/memory.h"

namespace Core {

namespace {

class MultiAddressContainer {
public:
    MultiAddressContainer() = default;
    ~MultiAddressContainer() = default;

    void GatherValues(u32 start_entry, Common::ScratchBuffer<u32>& buffer) {
        buffer.resize(8);
        buffer.resize(0);
        size_t index = 0;
        const auto add_value = [&](u32 value) {
            buffer.resize(index + 1);
            buffer[index++] = value;
        };

        u32 iter_entry = start_entry;
        Entry* current = &storage[iter_entry - 1];
        add_value(current->value);
        while (current->next_entry != 0) {
            iter_entry = current->next_entry;
            current = &storage[iter_entry - 1];
            add_value(current->value);
        }
    }

    u32 Register(u32 value) {
        return RegisterImplementation(value);
    }

    void Register(u32 value, u32 start_entry) {
        auto entry_id = RegisterImplementation(value);
        u32 iter_entry = start_entry;
        Entry* current = &storage[iter_entry - 1];
        while (current->next_entry != 0) {
            iter_entry = current->next_entry;
            current = &storage[iter_entry - 1];
        }
        current->next_entry = entry_id;
    }

    std::pair<bool, u32> Unregister(u32 value, u32 start_entry) {
        u32 iter_entry = start_entry;
        Entry* previous{};
        Entry* current = &storage[iter_entry - 1];
        Entry* next{};
        bool more_than_one_remaining = false;
        u32 result_start{start_entry};
        size_t count = 0;
        while (current->value != value) {
            count++;
            previous = current;
            iter_entry = current->next_entry;
            current = &storage[iter_entry - 1];
        }
        // Find next
        u32 next_entry = current->next_entry;
        if (next_entry != 0) {
            next = &storage[next_entry - 1];
            more_than_one_remaining = next->next_entry != 0 || previous != nullptr;
        }
        if (previous) {
            previous->next_entry = next_entry;
        } else {
            result_start = next_entry;
        }
        free_entries.emplace_back(iter_entry);
        return std::make_pair(more_than_one_remaining || count > 1, result_start);
    }

    u32 ReleaseEntry(u32 start_entry) {
        Entry* current = &storage[start_entry - 1];
        free_entries.emplace_back(start_entry);
        return current->value;
    }

private:
    u32 RegisterImplementation(u32 value) {
        auto entry_id = GetNewEntry();
        auto& entry = storage[entry_id - 1];
        entry.next_entry = 0;
        entry.value = value;
        return entry_id;
    }
    u32 GetNewEntry() {
        if (!free_entries.empty()) {
            u32 result = free_entries.front();
            free_entries.pop_front();
            return result;
        }
        storage.emplace_back();
        u32 new_entry = static_cast<u32>(storage.size());
        return new_entry;
    }

    struct Entry {
        u32 next_entry{};
        u32 value{};
    };

    std::deque<Entry> storage;
    std::deque<u32> free_entries;
};

struct EmptyAllocator {
    EmptyAllocator([[maybe_unused]] DAddr address) {}
};

} // namespace

template <typename DTraits>
struct DeviceMemoryManagerAllocator {
    static constexpr size_t device_virtual_bits = DTraits::device_virtual_bits;
    static constexpr DAddr first_address = 1ULL << Memory::SUDACHI_PAGEBITS;
    static constexpr DAddr max_device_area = 1ULL << device_virtual_bits;

    DeviceMemoryManagerAllocator() : main_allocator(first_address) {}

    Common::FlatAllocator<DAddr, 0, device_virtual_bits> main_allocator;
    MultiAddressContainer multi_dev_address;

    /// Returns true when vaddr -> vaddr+size is fully contained in the buffer
    template <bool pin_area>
    [[nodiscard]] bool IsInBounds(VAddr addr, u64 size) const noexcept {
        return addr >= 0 && addr + size <= max_device_area;
    }

    DAddr Allocate(size_t size) {
        return main_allocator.Allocate(size);
    }

    void AllocateFixed(DAddr b_address, size_t b_size) {
        main_allocator.AllocateFixed(b_address, b_size);
    }

    void Free(DAddr b_address, size_t b_size) {
        main_allocator.Free(b_address, b_size);
    }
};

template <typename Traits>
DeviceMemoryManager<Traits>::DeviceMemoryManager(const DeviceMemory& device_memory_)
    : physical_base{reinterpret_cast<const uintptr_t>(device_memory_.buffer.BackingBasePointer())},
      device_inter{nullptr}, compressed_physical_ptr(device_as_size >> Memory::SUDACHI_PAGEBITS),
      compressed_device_addr(1ULL << ((Settings::values.memory_layout_mode.GetValue() ==
                                               Settings::MemoryLayout::Memory_4Gb
                                           ? physical_min_bits
                                           : physical_max_bits) -
                                      Memory::SUDACHI_PAGEBITS)),
      continuity_tracker(device_as_size >> Memory::SUDACHI_PAGEBITS),
      cpu_backing_address(device_as_size >> Memory::SUDACHI_PAGEBITS) {
    impl = std::make_unique<DeviceMemoryManagerAllocator<Traits>>();
    cached_pages = std::make_unique<CachedPages>();

    const size_t total_virtual = device_as_size >> Memory::SUDACHI_PAGEBITS;
    for (size_t i = 0; i < total_virtual; i++) {
        compressed_physical_ptr[i] = 0;
        continuity_tracker[i] = 1;
        cpu_backing_address[i] = 0;
    }
    const size_t total_phys = 1ULL << ((Settings::values.memory_layout_mode.GetValue() ==
                                                Settings::MemoryLayout::Memory_4Gb
                                            ? physical_min_bits
                                            : physical_max_bits) -
                                       Memory::SUDACHI_PAGEBITS);
    for (size_t i = 0; i < total_phys; i++) {
        compressed_device_addr[i] = 0;
    }
}

template <typename Traits>
DeviceMemoryManager<Traits>::~DeviceMemoryManager() = default;

template <typename Traits>
void DeviceMemoryManager<Traits>::BindInterface(DeviceInterface* device_inter_) {
    device_inter = device_inter_;
}

template <typename Traits>
DAddr DeviceMemoryManager<Traits>::Allocate(size_t size) {
    return impl->Allocate(size);
}

template <typename Traits>
void DeviceMemoryManager<Traits>::AllocateFixed(DAddr start, size_t size) {
    return impl->AllocateFixed(start, size);
}

template <typename Traits>
void DeviceMemoryManager<Traits>::Free(DAddr start, size_t size) {
    impl->Free(start, size);
}

template <typename Traits>
void DeviceMemoryManager<Traits>::Map(DAddr address, VAddr virtual_address, size_t size, Asid asid,
                                      bool track) {
    Core::Memory::Memory* process_memory = registered_processes[asid.id];
    size_t start_page_d = address >> Memory::SUDACHI_PAGEBITS;
    size_t num_pages = Common::AlignUp(size, Memory::SUDACHI_PAGESIZE) >> Memory::SUDACHI_PAGEBITS;
    std::scoped_lock lk(mapping_guard);
    for (size_t i = 0; i < num_pages; i++) {
        const VAddr new_vaddress = virtual_address + i * Memory::SUDACHI_PAGESIZE;
        auto* ptr = process_memory->GetPointerSilent(Common::ProcessAddress(new_vaddress));
        if (ptr == nullptr) [[unlikely]] {
            compressed_physical_ptr[start_page_d + i] = 0;
            continue;
        }
        auto phys_addr = static_cast<u32>(GetRawPhysicalAddr(ptr) >> Memory::SUDACHI_PAGEBITS) + 1U;
        compressed_physical_ptr[start_page_d + i] = phys_addr;
        InsertCPUBacking(start_page_d + i, new_vaddress, asid);
        const u32 base_dev = compressed_device_addr[phys_addr - 1U];
        const u32 new_dev = static_cast<u32>(start_page_d + i);
        if (base_dev == 0) [[likely]] {
            compressed_device_addr[phys_addr - 1U] = new_dev;
            continue;
        }
        u32 start_id = base_dev & MULTI_MASK;
        if ((base_dev >> MULTI_FLAG_BITS) == 0) {
            start_id = impl->multi_dev_address.Register(base_dev);
            compressed_device_addr[phys_addr - 1U] = MULTI_FLAG | start_id;
        }
        impl->multi_dev_address.Register(new_dev, start_id);
    }
    if (track) {
        TrackContinuityImpl(address, virtual_address, size, asid);
    }
}

template <typename Traits>
void DeviceMemoryManager<Traits>::Unmap(DAddr address, size_t size) {
    size_t start_page_d = address >> Memory::SUDACHI_PAGEBITS;
    size_t num_pages = Common::AlignUp(size, Memory::SUDACHI_PAGESIZE) >> Memory::SUDACHI_PAGEBITS;
    device_inter->InvalidateRegion(address, size);
    std::scoped_lock lk(mapping_guard);
    for (size_t i = 0; i < num_pages; i++) {
        auto phys_addr = compressed_physical_ptr[start_page_d + i];
        compressed_physical_ptr[start_page_d + i] = 0;
        cpu_backing_address[start_page_d + i] = 0;
        if (phys_addr != 0) [[likely]] {
            const u32 base_dev = compressed_device_addr[phys_addr - 1U];
            if ((base_dev >> MULTI_FLAG_BITS) == 0) [[likely]] {
                compressed_device_addr[phys_addr - 1] = 0;
                continue;
            }
            const auto [more_entries, new_start] = impl->multi_dev_address.Unregister(
                static_cast<u32>(start_page_d + i), base_dev & MULTI_MASK);
            if (!more_entries) {
                compressed_device_addr[phys_addr - 1] =
                    impl->multi_dev_address.ReleaseEntry(new_start);
                continue;
            }
            compressed_device_addr[phys_addr - 1] = new_start | MULTI_FLAG;
        }
    }
}
template <typename Traits>
void DeviceMemoryManager<Traits>::TrackContinuityImpl(DAddr address, VAddr virtual_address,
                                                      size_t size, Asid asid) {
    Core::Memory::Memory* process_memory = registered_processes[asid.id];
    size_t start_page_d = address >> Memory::SUDACHI_PAGEBITS;
    size_t num_pages = Common::AlignUp(size, Memory::SUDACHI_PAGESIZE) >> Memory::SUDACHI_PAGEBITS;
    uintptr_t last_ptr = 0;
    size_t page_count = 1;
    for (size_t i = num_pages; i > 0; i--) {
        size_t index = i - 1;
        const VAddr new_vaddress = virtual_address + index * Memory::SUDACHI_PAGESIZE;
        const uintptr_t new_ptr = reinterpret_cast<uintptr_t>(
            process_memory->GetPointerSilent(Common::ProcessAddress(new_vaddress)));
        if (new_ptr + page_size == last_ptr) {
            page_count++;
        } else {
            page_count = 1;
        }
        last_ptr = new_ptr;
        continuity_tracker[start_page_d + index] = static_cast<u32>(page_count);
    }
}
template <typename Traits>
u8* DeviceMemoryManager<Traits>::GetSpan(const DAddr src_addr, const std::size_t size) {
    size_t page_index = src_addr >> page_bits;
    size_t subbits = src_addr & page_mask;
    if ((static_cast<size_t>(continuity_tracker[page_index]) << page_bits) >= size + subbits) {
        return GetPointer<u8>(src_addr);
    }
    return nullptr;
}

template <typename Traits>
const u8* DeviceMemoryManager<Traits>::GetSpan(const DAddr src_addr, const std::size_t size) const {
    size_t page_index = src_addr >> page_bits;
    size_t subbits = src_addr & page_mask;
    if ((static_cast<size_t>(continuity_tracker[page_index]) << page_bits) >= size + subbits) {
        return GetPointer<u8>(src_addr);
    }
    return nullptr;
}

template <typename Traits>
void DeviceMemoryManager<Traits>::InnerGatherDeviceAddresses(Common::ScratchBuffer<u32>& buffer,
                                                             PAddr address) {
    size_t phys_addr = address >> page_bits;
    std::scoped_lock lk(mapping_guard);
    u32 backing = compressed_device_addr[phys_addr];
    if ((backing >> MULTI_FLAG_BITS) != 0) {
        impl->multi_dev_address.GatherValues(backing & MULTI_MASK, buffer);
        return;
    }
    buffer.resize(1);
    buffer[0] = backing;
}

template <typename Traits>
template <typename T>
T* DeviceMemoryManager<Traits>::GetPointer(DAddr address) {
    const size_t index = address >> Memory::SUDACHI_PAGEBITS;
    const size_t offset = address & Memory::SUDACHI_PAGEMASK;
    auto phys_addr = compressed_physical_ptr[index];
    if (phys_addr == 0) [[unlikely]] {
        return nullptr;
    }
    return GetPointerFromRaw<T>((static_cast<PAddr>(phys_addr - 1) << Memory::SUDACHI_PAGEBITS) +
                                offset);
}

template <typename Traits>
template <typename T>
const T* DeviceMemoryManager<Traits>::GetPointer(DAddr address) const {
    const size_t index = address >> Memory::SUDACHI_PAGEBITS;
    const size_t offset = address & Memory::SUDACHI_PAGEMASK;
    auto phys_addr = compressed_physical_ptr[index];
    if (phys_addr == 0) [[unlikely]] {
        return nullptr;
    }
    return GetPointerFromRaw<T>((static_cast<PAddr>(phys_addr - 1) << Memory::SUDACHI_PAGEBITS) +
                                offset);
}

template <typename Traits>
template <typename T>
void DeviceMemoryManager<Traits>::Write(DAddr address, T value) {
    T* ptr = GetPointer<T>(address);
    if (!ptr) [[unlikely]] {
        return;
    }
    std::memcpy(ptr, &value, sizeof(T));
}

template <typename Traits>
template <typename T>
T DeviceMemoryManager<Traits>::Read(DAddr address) const {
    const T* ptr = GetPointer<T>(address);
    T result{};
    if (!ptr) [[unlikely]] {
        return result;
    }
    std::memcpy(&result, ptr, sizeof(T));
    return result;
}

template <typename Traits>
void DeviceMemoryManager<Traits>::WalkBlock(DAddr addr, std::size_t size, auto on_unmapped,
                                            auto on_memory, auto increment) {
    std::size_t remaining_size = size;
    std::size_t page_index = addr >> Memory::SUDACHI_PAGEBITS;
    std::size_t page_offset = addr & Memory::SUDACHI_PAGEMASK;

    while (remaining_size) {
        const size_t next_pages = static_cast<std::size_t>(continuity_tracker[page_index]);
        const std::size_t copy_amount =
            std::min((next_pages << Memory::SUDACHI_PAGEBITS) - page_offset, remaining_size);
        const auto current_vaddr =
            static_cast<u64>((page_index << Memory::SUDACHI_PAGEBITS) + page_offset);
        SCOPE_EXIT{
            page_index += next_pages;
            page_offset = 0;
            increment(copy_amount);
            remaining_size -= copy_amount;
        };

        auto phys_addr = compressed_physical_ptr[page_index];
        if (phys_addr == 0) {
            on_unmapped(copy_amount, current_vaddr);
            continue;
        }
        auto* mem_ptr = GetPointerFromRaw<u8>(
            (static_cast<PAddr>(phys_addr - 1) << Memory::SUDACHI_PAGEBITS) + page_offset);
        on_memory(copy_amount, mem_ptr);
    }
}

template <typename Traits>
void DeviceMemoryManager<Traits>::ReadBlock(DAddr address, void* dest_pointer, size_t size) {
    device_inter->FlushRegion(address, size);
    WalkBlock(
        address, size,
        [&](size_t copy_amount, DAddr current_vaddr) {
            LOG_ERROR(
                HW_Memory,
                "Unmapped Device ReadBlock @ 0x{:016X} (start address = 0x{:016X}, size = {})",
                current_vaddr, address, size);
            std::memset(dest_pointer, 0, copy_amount);
        },
        [&](size_t copy_amount, const u8* const src_ptr) {
            std::memcpy(dest_pointer, src_ptr, copy_amount);
        },
        [&](const std::size_t copy_amount) {
            dest_pointer = static_cast<u8*>(dest_pointer) + copy_amount;
        });
}

template <typename Traits>
void DeviceMemoryManager<Traits>::WriteBlock(DAddr address, const void* src_pointer, size_t size) {
    WalkBlock(
        address, size,
        [&](size_t copy_amount, DAddr current_vaddr) {
            LOG_ERROR(
                HW_Memory,
                "Unmapped Device WriteBlock @ 0x{:016X} (start address = 0x{:016X}, size = {})",
                current_vaddr, address, size);
        },
        [&](size_t copy_amount, u8* const dst_ptr) {
            std::memcpy(dst_ptr, src_pointer, copy_amount);
        },
        [&](const std::size_t copy_amount) {
            src_pointer = static_cast<const u8*>(src_pointer) + copy_amount;
        });
    device_inter->InvalidateRegion(address, size);
}

template <typename Traits>
void DeviceMemoryManager<Traits>::ReadBlockUnsafe(DAddr address, void* dest_pointer, size_t size) {
    WalkBlock(
        address, size,
        [&](size_t copy_amount, DAddr current_vaddr) {
            LOG_ERROR(
                HW_Memory,
                "Unmapped Device ReadBlock @ 0x{:016X} (start address = 0x{:016X}, size = {})",
                current_vaddr, address, size);
            std::memset(dest_pointer, 0, copy_amount);
        },
        [&](size_t copy_amount, const u8* const src_ptr) {
            std::memcpy(dest_pointer, src_ptr, copy_amount);
        },
        [&](const std::size_t copy_amount) {
            dest_pointer = static_cast<u8*>(dest_pointer) + copy_amount;
        });
}

template <typename Traits>
void DeviceMemoryManager<Traits>::WriteBlockUnsafe(DAddr address, const void* src_pointer,
                                                   size_t size) {
    WalkBlock(
        address, size,
        [&](size_t copy_amount, DAddr current_vaddr) {
            LOG_ERROR(
                HW_Memory,
                "Unmapped Device WriteBlock @ 0x{:016X} (start address = 0x{:016X}, size = {})",
                current_vaddr, address, size);
        },
        [&](size_t copy_amount, u8* const dst_ptr) {
            std::memcpy(dst_ptr, src_pointer, copy_amount);
        },
        [&](const std::size_t copy_amount) {
            src_pointer = static_cast<const u8*>(src_pointer) + copy_amount;
        });
}

template <typename Traits>
Asid DeviceMemoryManager<Traits>::RegisterProcess(Memory::Memory* memory_device_inter) {
    size_t new_id{};
    if (!id_pool.empty()) {
        new_id = id_pool.front();
        id_pool.pop_front();
        registered_processes[new_id] = memory_device_inter;
    } else {
        registered_processes.emplace_back(memory_device_inter);
        new_id = registered_processes.size() - 1U;
    }
    return Asid{new_id};
}

template <typename Traits>
void DeviceMemoryManager<Traits>::UnregisterProcess(Asid asid) {
    registered_processes[asid.id] = nullptr;
    id_pool.push_front(asid.id);
}

template <typename Traits>
void DeviceMemoryManager<Traits>::UpdatePagesCachedCount(DAddr addr, size_t size, s32 delta) {
    Common::ScopedRangeLock lk(counter_guard, addr, size);
    u64 uncache_begin = 0;
    u64 cache_begin = 0;
    u64 uncache_bytes = 0;
    u64 cache_bytes = 0;
    const auto MarkRegionCaching = &DeviceMemoryManager<Traits>::DeviceMethods::MarkRegionCaching;

    std::atomic_thread_fence(std::memory_order_acquire);
    const size_t page_end = Common::DivCeil(addr + size, Memory::SUDACHI_PAGESIZE);
    size_t page = addr >> Memory::SUDACHI_PAGEBITS;
    auto [asid, base_vaddress] = ExtractCPUBacking(page);
    auto* memory_device_inter = registered_processes[asid.id];
    const auto release_pending = [&] {
        if (uncache_bytes > 0) {
            if (memory_device_inter != nullptr) {
                MarkRegionCaching(memory_device_inter, uncache_begin << Memory::SUDACHI_PAGEBITS,
                                  uncache_bytes, false);
            }
            uncache_bytes = 0;
        }
        if (cache_bytes > 0) {
            if (memory_device_inter != nullptr) {
                MarkRegionCaching(memory_device_inter, cache_begin << Memory::SUDACHI_PAGEBITS,
                                  cache_bytes, true);
            }
            cache_bytes = 0;
        }
    };
    size_t old_vpage = (base_vaddress >> Memory::SUDACHI_PAGEBITS) - 1;
    for (; page != page_end; ++page) {
        CounterAtomicType& count = cached_pages->at(page >> subentries_shift).Count(page);
        auto [asid_2, vpage] = ExtractCPUBacking(page);
        vpage >>= Memory::SUDACHI_PAGEBITS;

        if (vpage == 0) [[unlikely]] {
            release_pending();
            continue;
        }

        if (asid.id != asid_2.id) [[unlikely]] {
            release_pending();
            memory_device_inter = registered_processes[asid_2.id];
        }

        if (vpage != old_vpage + 1) [[unlikely]] {
            release_pending();
        }

        old_vpage = vpage;

        // Adds or subtracts 1, as count is a unsigned 8-bit value
        count.fetch_add(static_cast<CounterType>(delta), std::memory_order_release);

        // Assume delta is either -1 or 1
        if (count.load(std::memory_order::relaxed) == 0) {
            if (uncache_bytes == 0) {
                uncache_begin = vpage;
            }
            uncache_bytes += Memory::SUDACHI_PAGESIZE;
        } else if (uncache_bytes > 0) {
            MarkRegionCaching(memory_device_inter, uncache_begin << Memory::SUDACHI_PAGEBITS,
                              uncache_bytes, false);
            uncache_bytes = 0;
        }
        if (count.load(std::memory_order::relaxed) == 1 && delta > 0) {
            if (cache_bytes == 0) {
                cache_begin = vpage;
            }
            cache_bytes += Memory::SUDACHI_PAGESIZE;
        } else if (cache_bytes > 0) {
            MarkRegionCaching(memory_device_inter, cache_begin << Memory::SUDACHI_PAGEBITS,
                              cache_bytes, true);
            cache_bytes = 0;
        }
    }
    release_pending();
}

} // namespace Core
