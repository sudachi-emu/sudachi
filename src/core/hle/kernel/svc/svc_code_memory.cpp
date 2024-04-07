// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/scope_exit.h"
#include "core/core.h"
#include "core/hle/kernel/k_code_memory.h"
#include "core/hle/kernel/k_process.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/svc.h"

namespace Kernel::Svc {
namespace {

constexpr bool IsValidMapCodeMemoryPermission(MemoryPermission perm) {
    return perm == MemoryPermission::ReadWrite;
}

constexpr bool IsValidMapToOwnerCodeMemoryPermission(MemoryPermission perm) {
    return perm == MemoryPermission::Read || perm == MemoryPermission::ReadExecute;
}

constexpr bool IsValidUnmapCodeMemoryPermission(MemoryPermission perm) {
    return perm == MemoryPermission::None;
}

constexpr bool IsValidUnmapFromOwnerCodeMemoryPermission(MemoryPermission perm) {
    return perm == MemoryPermission::None;
}

} // namespace

Result CreateCodeMemory(Core::System& system, Handle* out, u64 address, uint64_t size) {
    LOG_TRACE(Kernel_SVC, "called, address=0x{:X}, size=0x{:X}", address, size);

    // Get kernel instance.
    auto& kernel = system.Kernel();

    // Validate address / size.
    R_UNLESS(Common::IsAligned(address, PageSize), ResultInvalidAddress);
    R_UNLESS(Common::IsAligned(size, PageSize), ResultInvalidSize);
    R_UNLESS(size > 0, ResultInvalidSize);
    R_UNLESS((address < address + size), ResultInvalidCurrentMemory);

    // Create the code memory.

    KCodeMemory* code_mem = KCodeMemory::Create(kernel);
    R_UNLESS(code_mem != nullptr, ResultOutOfResource);
    SCOPE_EXIT {
        code_mem->Close();
    };

    // Verify that the region is in range.
    R_UNLESS(GetCurrentProcess(system.Kernel()).GetPageTable().Contains(address, size),
             ResultInvalidCurrentMemory);

    // Initialize the code memory.
    R_TRY(code_mem->Initialize(system.DeviceMemory(), address, size));

    // Register the code memory.
    KCodeMemory::Register(kernel, code_mem);

    // Add the code memory to the handle table.
    R_TRY(GetCurrentProcess(system.Kernel()).GetHandleTable().Add(out, code_mem));

    R_SUCCEED();
}

Result ControlCodeMemory(Core::System& system, Handle code_memory_handle,
                         CodeMemoryOperation operation, u64 address, uint64_t size,
                         MemoryPermission perm) {

    LOG_TRACE(Kernel_SVC,
              "called, code_memory_handle=0x{:X}, operation=0x{:X}, address=0x{:X}, size=0x{:X}, "
              "permission=0x{:X}",
              code_memory_handle, operation, address, size, perm);

    // Validate the address / size.
    R_UNLESS(Common::IsAligned(address, PageSize), ResultInvalidAddress);
    R_UNLESS(Common::IsAligned(size, PageSize), ResultInvalidSize);
    R_UNLESS(size > 0, ResultInvalidSize);
    R_UNLESS((address < address + size), ResultInvalidCurrentMemory);

    // Get the code memory from its handle.
    KScopedAutoObject code_mem = GetCurrentProcess(system.Kernel())
                                     .GetHandleTable()
                                     .GetObject<KCodeMemory>(code_memory_handle);
    R_UNLESS(code_mem.IsNotNull(), ResultInvalidHandle);

    // NOTE: Here, Atmosphere extends the SVC to allow code memory operations on one's own process.
    // This enables homebrew usage of these SVCs for JIT.

    // Perform the operation.
    switch (operation) {
    case CodeMemoryOperation::Map: {
        // Check that the region is in range.
        R_UNLESS(GetCurrentProcess(system.Kernel())
                     .GetPageTable()
                     .CanContain(address, size, KMemoryState::CodeOut),
                 ResultInvalidMemoryRegion);

        // Check the memory permission.
        R_UNLESS(IsValidMapCodeMemoryPermission(perm), ResultInvalidNewMemoryPermission);

        // Map the memory.
        R_TRY(code_mem->Map(address, size));
    } break;
    case CodeMemoryOperation::Unmap: {
        // Check that the region is in range.
        R_UNLESS(GetCurrentProcess(system.Kernel())
                     .GetPageTable()
                     .CanContain(address, size, KMemoryState::CodeOut),
                 ResultInvalidMemoryRegion);

        // Check the memory permission.
        R_UNLESS(IsValidUnmapCodeMemoryPermission(perm), ResultInvalidNewMemoryPermission);

        // Unmap the memory.
        R_TRY(code_mem->Unmap(address, size));
    } break;
    case CodeMemoryOperation::MapToOwner: {
        // Check that the region is in range.
        R_UNLESS(code_mem->GetOwner()->GetPageTable().CanContain(address, size,
                                                                 KMemoryState::GeneratedCode),
                 ResultInvalidMemoryRegion);

        // Check the memory permission.
        R_UNLESS(IsValidMapToOwnerCodeMemoryPermission(perm), ResultInvalidNewMemoryPermission);

        // Map the memory to its owner.
        R_TRY(code_mem->MapToOwner(address, size, perm));
    } break;
    case CodeMemoryOperation::UnmapFromOwner: {
        // Check that the region is in range.
        R_UNLESS(code_mem->GetOwner()->GetPageTable().CanContain(address, size,
                                                                 KMemoryState::GeneratedCode),
                 ResultInvalidMemoryRegion);

        // Check the memory permission.
        R_UNLESS(IsValidUnmapFromOwnerCodeMemoryPermission(perm), ResultInvalidNewMemoryPermission);

        // Unmap the memory from its owner.
        R_TRY(code_mem->UnmapFromOwner(address, size));
    } break;
    default:
        R_THROW(ResultInvalidEnumValue);
    }

    R_SUCCEED();
}

Result CreateCodeMemory64(Core::System& system, Handle* out_handle, uint64_t address,
                          uint64_t size) {
    R_RETURN(CreateCodeMemory(system, out_handle, address, size));
}

Result ControlCodeMemory64(Core::System& system, Handle code_memory_handle,
                           CodeMemoryOperation operation, uint64_t address, uint64_t size,
                           MemoryPermission perm) {
    R_RETURN(ControlCodeMemory(system, code_memory_handle, operation, address, size, perm));
}

Result CreateCodeMemory64From32(Core::System& system, Handle* out_handle, uint32_t address,
                                uint32_t size) {
    R_RETURN(CreateCodeMemory(system, out_handle, address, size));
}

Result ControlCodeMemory64From32(Core::System& system, Handle code_memory_handle,
                                 CodeMemoryOperation operation, uint64_t address, uint64_t size,
                                 MemoryPermission perm) {
    R_RETURN(ControlCodeMemory(system, code_memory_handle, operation, address, size, perm));
}

} // namespace Kernel::Svc
