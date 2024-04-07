// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/core.h"
#include "core/hle/kernel/k_process.h"
#include "core/hle/kernel/svc.h"

namespace Kernel::Svc {

/// Set the process heap to a given Size. It can both extend and shrink the heap.
Result SetHeapSize(Core::System& system, u64* out_address, u64 size) {
    LOG_TRACE(Kernel_SVC, "called, heap_size=0x{:X}", size);

    // Validate size.
    R_UNLESS(Common::IsAligned(size, HeapSizeAlignment), ResultInvalidSize);
    R_UNLESS(size < MainMemorySizeMax, ResultInvalidSize);

    // Set the heap size.
    KProcessAddress address{};
    R_TRY(GetCurrentProcess(system.Kernel())
              .GetPageTable()
              .SetHeapSize(std::addressof(address), size));

    // We succeeded.
    *out_address = GetInteger(address);
    R_SUCCEED();
}

/// Maps memory at a desired address
Result MapPhysicalMemory(Core::System& system, u64 addr, u64 size) {
    LOG_DEBUG(Kernel_SVC, "called, addr=0x{:016X}, size=0x{:X}", addr, size);

    if (!Common::Is4KBAligned(addr)) {
        LOG_ERROR(Kernel_SVC, "Address is not aligned to 4KB, 0x{:016X}", addr);
        R_THROW(ResultInvalidAddress);
    }

    if (!Common::Is4KBAligned(size)) {
        LOG_ERROR(Kernel_SVC, "Size is not aligned to 4KB, 0x{:X}", size);
        R_THROW(ResultInvalidSize);
    }

    if (size == 0) {
        LOG_ERROR(Kernel_SVC, "Size is zero");
        R_THROW(ResultInvalidSize);
    }

    if (!(addr < addr + size)) {
        LOG_ERROR(Kernel_SVC, "Size causes 64-bit overflow of address");
        R_THROW(ResultInvalidMemoryRegion);
    }

    KProcess* const current_process{GetCurrentProcessPointer(system.Kernel())};
    auto& page_table{current_process->GetPageTable()};

    if (current_process->GetTotalSystemResourceSize() == 0) {
        LOG_ERROR(Kernel_SVC, "System Resource Size is zero");
        R_THROW(ResultInvalidState);
    }

    if (!page_table.Contains(addr, size)) {
        LOG_ERROR(Kernel_SVC,
                  "Address is not within the address space, addr=0x{:016X}, size=0x{:016X}", addr,
                  size);
        R_THROW(ResultInvalidMemoryRegion);
    }

    if (!page_table.IsInAliasRegion(addr, size)) {
        LOG_ERROR(Kernel_SVC,
                  "Address is not within the alias region, addr=0x{:016X}, size=0x{:016X}", addr,
                  size);
        R_THROW(ResultInvalidMemoryRegion);
    }

    R_RETURN(page_table.MapPhysicalMemory(addr, size));
}

/// Unmaps memory previously mapped via MapPhysicalMemory
Result UnmapPhysicalMemory(Core::System& system, u64 addr, u64 size) {
    LOG_DEBUG(Kernel_SVC, "called, addr=0x{:016X}, size=0x{:X}", addr, size);

    if (!Common::Is4KBAligned(addr)) {
        LOG_ERROR(Kernel_SVC, "Address is not aligned to 4KB, 0x{:016X}", addr);
        R_THROW(ResultInvalidAddress);
    }

    if (!Common::Is4KBAligned(size)) {
        LOG_ERROR(Kernel_SVC, "Size is not aligned to 4KB, 0x{:X}", size);
        R_THROW(ResultInvalidSize);
    }

    if (size == 0) {
        LOG_ERROR(Kernel_SVC, "Size is zero");
        R_THROW(ResultInvalidSize);
    }

    if (!(addr < addr + size)) {
        LOG_ERROR(Kernel_SVC, "Size causes 64-bit overflow of address");
        R_THROW(ResultInvalidMemoryRegion);
    }

    KProcess* const current_process{GetCurrentProcessPointer(system.Kernel())};
    auto& page_table{current_process->GetPageTable()};

    if (current_process->GetTotalSystemResourceSize() == 0) {
        LOG_ERROR(Kernel_SVC, "System Resource Size is zero");
        R_THROW(ResultInvalidState);
    }

    if (!page_table.Contains(addr, size)) {
        LOG_ERROR(Kernel_SVC,
                  "Address is not within the address space, addr=0x{:016X}, size=0x{:016X}", addr,
                  size);
        R_THROW(ResultInvalidMemoryRegion);
    }

    if (!page_table.IsInAliasRegion(addr, size)) {
        LOG_ERROR(Kernel_SVC,
                  "Address is not within the alias region, addr=0x{:016X}, size=0x{:016X}", addr,
                  size);
        R_THROW(ResultInvalidMemoryRegion);
    }

    R_RETURN(page_table.UnmapPhysicalMemory(addr, size));
}

Result MapPhysicalMemoryUnsafe(Core::System& system, uint64_t address, uint64_t size) {
    UNIMPLEMENTED();
    R_THROW(ResultNotImplemented);
}

Result UnmapPhysicalMemoryUnsafe(Core::System& system, uint64_t address, uint64_t size) {
    UNIMPLEMENTED();
    R_THROW(ResultNotImplemented);
}

Result SetUnsafeLimit(Core::System& system, uint64_t limit) {
    UNIMPLEMENTED();
    R_THROW(ResultNotImplemented);
}

Result SetHeapSize64(Core::System& system, uint64_t* out_address, uint64_t size) {
    R_RETURN(SetHeapSize(system, out_address, size));
}

Result MapPhysicalMemory64(Core::System& system, uint64_t address, uint64_t size) {
    R_RETURN(MapPhysicalMemory(system, address, size));
}

Result UnmapPhysicalMemory64(Core::System& system, uint64_t address, uint64_t size) {
    R_RETURN(UnmapPhysicalMemory(system, address, size));
}

Result MapPhysicalMemoryUnsafe64(Core::System& system, uint64_t address, uint64_t size) {
    R_RETURN(MapPhysicalMemoryUnsafe(system, address, size));
}

Result UnmapPhysicalMemoryUnsafe64(Core::System& system, uint64_t address, uint64_t size) {
    R_RETURN(UnmapPhysicalMemoryUnsafe(system, address, size));
}

Result SetUnsafeLimit64(Core::System& system, uint64_t limit) {
    R_RETURN(SetUnsafeLimit(system, limit));
}

Result SetHeapSize64From32(Core::System& system, uint64_t* out_address, uint32_t size) {
    R_RETURN(SetHeapSize(system, out_address, size));
}

Result MapPhysicalMemory64From32(Core::System& system, uint32_t address, uint32_t size) {
    R_RETURN(MapPhysicalMemory(system, address, size));
}

Result UnmapPhysicalMemory64From32(Core::System& system, uint32_t address, uint32_t size) {
    R_RETURN(UnmapPhysicalMemory(system, address, size));
}

Result MapPhysicalMemoryUnsafe64From32(Core::System& system, uint32_t address, uint32_t size) {
    R_RETURN(MapPhysicalMemoryUnsafe(system, address, size));
}

Result UnmapPhysicalMemoryUnsafe64From32(Core::System& system, uint32_t address, uint32_t size) {
    R_RETURN(UnmapPhysicalMemoryUnsafe(system, address, size));
}

Result SetUnsafeLimit64From32(Core::System& system, uint32_t limit) {
    R_RETURN(SetUnsafeLimit(system, limit));
}

} // namespace Kernel::Svc
