// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/scope_exit.h"
#include "core/core.h"
#include "core/hle/kernel/k_process.h"
#include "core/hle/kernel/k_shared_memory.h"
#include "core/hle/kernel/svc.h"

namespace Kernel::Svc {
namespace {

constexpr bool IsValidSharedMemoryPermission(MemoryPermission perm) {
    switch (perm) {
    case MemoryPermission::Read:
    case MemoryPermission::ReadWrite:
        return true;
    default:
        return false;
    }
}

[[maybe_unused]] constexpr bool IsValidRemoteSharedMemoryPermission(MemoryPermission perm) {
    return IsValidSharedMemoryPermission(perm) || perm == MemoryPermission::DontCare;
}

} // namespace

Result MapSharedMemory(Core::System& system, Handle shmem_handle, u64 address, u64 size,
                       Svc::MemoryPermission map_perm) {
    LOG_TRACE(Kernel_SVC,
              "called, shared_memory_handle=0x{:X}, addr=0x{:X}, size=0x{:X}, permissions=0x{:08X}",
              shmem_handle, address, size, map_perm);

    // Validate the address/size.
    R_UNLESS(Common::IsAligned(address, PageSize), ResultInvalidAddress);
    R_UNLESS(Common::IsAligned(size, PageSize), ResultInvalidSize);
    R_UNLESS(size > 0, ResultInvalidSize);
    R_UNLESS((address < address + size), ResultInvalidCurrentMemory);

    // Validate the permission.
    R_UNLESS(IsValidSharedMemoryPermission(map_perm), ResultInvalidNewMemoryPermission);

    // Get the current process.
    auto& process = GetCurrentProcess(system.Kernel());
    auto& page_table = process.GetPageTable();

    // Get the shared memory.
    KScopedAutoObject shmem = process.GetHandleTable().GetObject<KSharedMemory>(shmem_handle);
    R_UNLESS(shmem.IsNotNull(), ResultInvalidHandle);

    // Verify that the mapping is in range.
    R_UNLESS(page_table.CanContain(address, size, KMemoryState::Shared), ResultInvalidMemoryRegion);

    // Add the shared memory to the process.
    R_TRY(process.AddSharedMemory(shmem.GetPointerUnsafe(), address, size));

    // Ensure that we clean up the shared memory if we fail to map it.
    ON_RESULT_FAILURE {
        process.RemoveSharedMemory(shmem.GetPointerUnsafe(), address, size);
    };

    // Map the shared memory.
    R_RETURN(shmem->Map(process, address, size, map_perm));
}

Result UnmapSharedMemory(Core::System& system, Handle shmem_handle, u64 address, u64 size) {
    // Validate the address/size.
    R_UNLESS(Common::IsAligned(address, PageSize), ResultInvalidAddress);
    R_UNLESS(Common::IsAligned(size, PageSize), ResultInvalidSize);
    R_UNLESS(size > 0, ResultInvalidSize);
    R_UNLESS((address < address + size), ResultInvalidCurrentMemory);

    // Get the current process.
    auto& process = GetCurrentProcess(system.Kernel());
    auto& page_table = process.GetPageTable();

    // Get the shared memory.
    KScopedAutoObject shmem = process.GetHandleTable().GetObject<KSharedMemory>(shmem_handle);
    R_UNLESS(shmem.IsNotNull(), ResultInvalidHandle);

    // Verify that the mapping is in range.
    R_UNLESS(page_table.CanContain(address, size, KMemoryState::Shared), ResultInvalidMemoryRegion);

    // Unmap the shared memory.
    R_TRY(shmem->Unmap(process, address, size));

    // Remove the shared memory from the process.
    process.RemoveSharedMemory(shmem.GetPointerUnsafe(), address, size);

    R_SUCCEED();
}

Result CreateSharedMemory(Core::System& system, Handle* out_handle, uint64_t size,
                          MemoryPermission owner_perm, MemoryPermission remote_perm) {
    UNIMPLEMENTED();
    R_THROW(ResultNotImplemented);
}

Result MapSharedMemory64(Core::System& system, Handle shmem_handle, uint64_t address, uint64_t size,
                         MemoryPermission map_perm) {
    R_RETURN(MapSharedMemory(system, shmem_handle, address, size, map_perm));
}

Result UnmapSharedMemory64(Core::System& system, Handle shmem_handle, uint64_t address,
                           uint64_t size) {
    R_RETURN(UnmapSharedMemory(system, shmem_handle, address, size));
}

Result CreateSharedMemory64(Core::System& system, Handle* out_handle, uint64_t size,
                            MemoryPermission owner_perm, MemoryPermission remote_perm) {
    R_RETURN(CreateSharedMemory(system, out_handle, size, owner_perm, remote_perm));
}

Result MapSharedMemory64From32(Core::System& system, Handle shmem_handle, uint32_t address,
                               uint32_t size, MemoryPermission map_perm) {
    R_RETURN(MapSharedMemory(system, shmem_handle, address, size, map_perm));
}

Result UnmapSharedMemory64From32(Core::System& system, Handle shmem_handle, uint32_t address,
                                 uint32_t size) {
    R_RETURN(UnmapSharedMemory(system, shmem_handle, address, size));
}

Result CreateSharedMemory64From32(Core::System& system, Handle* out_handle, uint32_t size,
                                  MemoryPermission owner_perm, MemoryPermission remote_perm) {
    R_RETURN(CreateSharedMemory(system, out_handle, size, owner_perm, remote_perm));
}

} // namespace Kernel::Svc
