// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/core.h"
#include "core/hle/kernel/k_process.h"
#include "core/hle/kernel/svc.h"
#include "core/hle/kernel/svc_results.h"
#include "core/hle/kernel/svc_types.h"

namespace Kernel::Svc {

void FlushEntireDataCache(Core::System& system) {
    UNIMPLEMENTED();
}

Result FlushDataCache(Core::System& system, uint64_t address, uint64_t size) {
    UNIMPLEMENTED();
    R_THROW(ResultNotImplemented);
}

Result InvalidateProcessDataCache(Core::System& system, Handle process_handle, uint64_t address,
                                  uint64_t size) {
    UNIMPLEMENTED();
    R_THROW(ResultNotImplemented);
}

Result StoreProcessDataCache(Core::System& system, Handle process_handle, uint64_t address,
                             uint64_t size) {
    UNIMPLEMENTED();
    R_THROW(ResultNotImplemented);
}

Result FlushProcessDataCache(Core::System& system, Handle process_handle, u64 address, u64 size) {
    // Validate address/size.
    R_UNLESS(size > 0, ResultInvalidSize);
    R_UNLESS(address == static_cast<uint64_t>(address), ResultInvalidCurrentMemory);
    R_UNLESS(size == static_cast<uint64_t>(size), ResultInvalidCurrentMemory);

    // Get the process from its handle.
    KScopedAutoObject process =
        GetCurrentProcess(system.Kernel()).GetHandleTable().GetObject<KProcess>(process_handle);
    R_UNLESS(process.IsNotNull(), ResultInvalidHandle);

    // Verify the region is within range.
    auto& page_table = process->GetPageTable();
    R_UNLESS(page_table.Contains(address, size), ResultInvalidCurrentMemory);

    // Perform the operation.
    R_RETURN(GetCurrentMemory(system.Kernel()).FlushDataCache(address, size));
}

void FlushEntireDataCache64(Core::System& system) {
    FlushEntireDataCache(system);
}

Result FlushDataCache64(Core::System& system, uint64_t address, uint64_t size) {
    R_RETURN(FlushDataCache(system, address, size));
}

Result InvalidateProcessDataCache64(Core::System& system, Handle process_handle, uint64_t address,
                                    uint64_t size) {
    R_RETURN(InvalidateProcessDataCache(system, process_handle, address, size));
}

Result StoreProcessDataCache64(Core::System& system, Handle process_handle, uint64_t address,
                               uint64_t size) {
    R_RETURN(StoreProcessDataCache(system, process_handle, address, size));
}

Result FlushProcessDataCache64(Core::System& system, Handle process_handle, uint64_t address,
                               uint64_t size) {
    R_RETURN(FlushProcessDataCache(system, process_handle, address, size));
}

void FlushEntireDataCache64From32(Core::System& system) {
    return FlushEntireDataCache(system);
}

Result FlushDataCache64From32(Core::System& system, uint32_t address, uint32_t size) {
    R_RETURN(FlushDataCache(system, address, size));
}

Result InvalidateProcessDataCache64From32(Core::System& system, Handle process_handle,
                                          uint64_t address, uint64_t size) {
    R_RETURN(InvalidateProcessDataCache(system, process_handle, address, size));
}

Result StoreProcessDataCache64From32(Core::System& system, Handle process_handle, uint64_t address,
                                     uint64_t size) {
    R_RETURN(StoreProcessDataCache(system, process_handle, address, size));
}

Result FlushProcessDataCache64From32(Core::System& system, Handle process_handle, uint64_t address,
                                     uint64_t size) {
    R_RETURN(FlushProcessDataCache(system, process_handle, address, size));
}

} // namespace Kernel::Svc
