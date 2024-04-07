// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/scope_exit.h"
#include "core/core.h"
#include "core/hle/kernel/k_process.h"
#include "core/hle/kernel/k_scoped_resource_reservation.h"
#include "core/hle/kernel/k_transfer_memory.h"
#include "core/hle/kernel/svc.h"

namespace Kernel::Svc {
namespace {

constexpr bool IsValidTransferMemoryPermission(MemoryPermission perm) {
    switch (perm) {
    case MemoryPermission::None:
    case MemoryPermission::Read:
    case MemoryPermission::ReadWrite:
        return true;
    default:
        return false;
    }
}

} // Anonymous namespace

/// Creates a TransferMemory object
Result CreateTransferMemory(Core::System& system, Handle* out, u64 address, u64 size,
                            MemoryPermission map_perm) {
    auto& kernel = system.Kernel();

    // Validate the size.
    R_UNLESS(Common::IsAligned(address, PageSize), ResultInvalidAddress);
    R_UNLESS(Common::IsAligned(size, PageSize), ResultInvalidSize);
    R_UNLESS(size > 0, ResultInvalidSize);
    R_UNLESS((address < address + size), ResultInvalidCurrentMemory);

    // Validate the permissions.
    R_UNLESS(IsValidTransferMemoryPermission(map_perm), ResultInvalidNewMemoryPermission);

    // Get the current process and handle table.
    auto& process = GetCurrentProcess(kernel);
    auto& handle_table = process.GetHandleTable();

    // Reserve a new transfer memory from the process resource limit.
    KScopedResourceReservation trmem_reservation(std::addressof(process),
                                                 LimitableResource::TransferMemoryCountMax);
    R_UNLESS(trmem_reservation.Succeeded(), ResultLimitReached);

    // Create the transfer memory.
    KTransferMemory* trmem = KTransferMemory::Create(kernel);
    R_UNLESS(trmem != nullptr, ResultOutOfResource);

    // Ensure the only reference is in the handle table when we're done.
    SCOPE_EXIT {
        trmem->Close();
    };

    // Ensure that the region is in range.
    R_UNLESS(process.GetPageTable().Contains(address, size), ResultInvalidCurrentMemory);

    // Initialize the transfer memory.
    R_TRY(trmem->Initialize(address, size, map_perm));

    // Commit the reservation.
    trmem_reservation.Commit();

    // Register the transfer memory.
    KTransferMemory::Register(kernel, trmem);

    // Add the transfer memory to the handle table.
    R_RETURN(handle_table.Add(out, trmem));
}

Result MapTransferMemory(Core::System& system, Handle trmem_handle, uint64_t address, uint64_t size,
                         MemoryPermission map_perm) {
    // Validate the address/size.
    R_UNLESS(Common::IsAligned(address, PageSize), ResultInvalidAddress);
    R_UNLESS(Common::IsAligned(size, PageSize), ResultInvalidSize);
    R_UNLESS(size > 0, ResultInvalidSize);
    R_UNLESS((address < address + size), ResultInvalidCurrentMemory);

    // Validate the permission.
    R_UNLESS(IsValidTransferMemoryPermission(map_perm), ResultInvalidState);

    // Get the transfer memory.
    KScopedAutoObject trmem = GetCurrentProcess(system.Kernel())
                                  .GetHandleTable()
                                  .GetObject<KTransferMemory>(trmem_handle);
    R_UNLESS(trmem.IsNotNull(), ResultInvalidHandle);

    // Verify that the mapping is in range.
    R_UNLESS(GetCurrentProcess(system.Kernel())
                 .GetPageTable()
                 .CanContain(address, size, KMemoryState::Transferred),
             ResultInvalidMemoryRegion);

    // Map the transfer memory.
    R_TRY(trmem->Map(address, size, map_perm));

    // We succeeded.
    R_SUCCEED();
}

Result UnmapTransferMemory(Core::System& system, Handle trmem_handle, uint64_t address,
                           uint64_t size) {
    // Validate the address/size.
    R_UNLESS(Common::IsAligned(address, PageSize), ResultInvalidAddress);
    R_UNLESS(Common::IsAligned(size, PageSize), ResultInvalidSize);
    R_UNLESS(size > 0, ResultInvalidSize);
    R_UNLESS((address < address + size), ResultInvalidCurrentMemory);

    // Get the transfer memory.
    KScopedAutoObject trmem = GetCurrentProcess(system.Kernel())
                                  .GetHandleTable()
                                  .GetObject<KTransferMemory>(trmem_handle);
    R_UNLESS(trmem.IsNotNull(), ResultInvalidHandle);

    // Verify that the mapping is in range.
    R_UNLESS(GetCurrentProcess(system.Kernel())
                 .GetPageTable()
                 .CanContain(address, size, KMemoryState::Transferred),
             ResultInvalidMemoryRegion);

    // Unmap the transfer memory.
    R_TRY(trmem->Unmap(address, size));

    R_SUCCEED();
}

Result MapTransferMemory64(Core::System& system, Handle trmem_handle, uint64_t address,
                           uint64_t size, MemoryPermission owner_perm) {
    R_RETURN(MapTransferMemory(system, trmem_handle, address, size, owner_perm));
}

Result UnmapTransferMemory64(Core::System& system, Handle trmem_handle, uint64_t address,
                             uint64_t size) {
    R_RETURN(UnmapTransferMemory(system, trmem_handle, address, size));
}

Result CreateTransferMemory64(Core::System& system, Handle* out_handle, uint64_t address,
                              uint64_t size, MemoryPermission map_perm) {
    R_RETURN(CreateTransferMemory(system, out_handle, address, size, map_perm));
}

Result MapTransferMemory64From32(Core::System& system, Handle trmem_handle, uint32_t address,
                                 uint32_t size, MemoryPermission owner_perm) {
    R_RETURN(MapTransferMemory(system, trmem_handle, address, size, owner_perm));
}

Result UnmapTransferMemory64From32(Core::System& system, Handle trmem_handle, uint32_t address,
                                   uint32_t size) {
    R_RETURN(UnmapTransferMemory(system, trmem_handle, address, size));
}

Result CreateTransferMemory64From32(Core::System& system, Handle* out_handle, uint32_t address,
                                    uint32_t size, MemoryPermission map_perm) {
    R_RETURN(CreateTransferMemory(system, out_handle, address, size, map_perm));
}

} // namespace Kernel::Svc
