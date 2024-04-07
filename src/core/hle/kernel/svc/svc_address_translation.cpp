// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/hle/kernel/svc.h"
#include "core/hle/kernel/svc_results.h"

namespace Kernel::Svc {

Result QueryPhysicalAddress(Core::System& system, lp64::PhysicalMemoryInfo* out_info,
                            uint64_t address) {
    UNIMPLEMENTED();
    R_THROW(ResultNotImplemented);
}

Result QueryIoMapping(Core::System& system, uint64_t* out_address, uint64_t* out_size,
                      uint64_t physical_address, uint64_t size) {
    UNIMPLEMENTED();
    R_THROW(ResultNotImplemented);
}

Result QueryPhysicalAddress64(Core::System& system, lp64::PhysicalMemoryInfo* out_info,
                              uint64_t address) {
    R_RETURN(QueryPhysicalAddress(system, out_info, address));
}

Result QueryIoMapping64(Core::System& system, uint64_t* out_address, uint64_t* out_size,
                        uint64_t physical_address, uint64_t size) {
    R_RETURN(QueryIoMapping(system, out_address, out_size, physical_address, size));
}

Result QueryPhysicalAddress64From32(Core::System& system, ilp32::PhysicalMemoryInfo* out_info,
                                    uint32_t address) {
    lp64::PhysicalMemoryInfo info{};
    R_TRY(QueryPhysicalAddress(system, std::addressof(info), address));

    *out_info = {
        .physical_address = info.physical_address,
        .virtual_address = static_cast<u32>(info.virtual_address),
        .size = static_cast<u32>(info.size),
    };
    R_SUCCEED();
}

Result QueryIoMapping64From32(Core::System& system, uint64_t* out_address, uint64_t* out_size,
                              uint64_t physical_address, uint32_t size) {
    R_RETURN(QueryIoMapping(system, reinterpret_cast<uint64_t*>(out_address),
                            reinterpret_cast<uint64_t*>(out_size), physical_address, size));
}

} // namespace Kernel::Svc
