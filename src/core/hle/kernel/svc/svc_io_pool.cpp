// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/hle/kernel/svc.h"
#include "core/hle/kernel/svc_results.h"

namespace Kernel::Svc {

Result CreateIoPool(Core::System& system, Handle* out, IoPoolType pool_type) {
    UNIMPLEMENTED();
    R_THROW(ResultNotImplemented);
}

Result CreateIoRegion(Core::System& system, Handle* out, Handle io_pool_handle, uint64_t phys_addr,
                      uint64_t size, MemoryMapping mapping, MemoryPermission perm) {
    UNIMPLEMENTED();
    R_THROW(ResultNotImplemented);
}

Result MapIoRegion(Core::System& system, Handle io_region_handle, uint64_t address, uint64_t size,
                   MemoryPermission map_perm) {
    UNIMPLEMENTED();
    R_THROW(ResultNotImplemented);
}

Result UnmapIoRegion(Core::System& system, Handle io_region_handle, uint64_t address,
                     uint64_t size) {
    UNIMPLEMENTED();
    R_THROW(ResultNotImplemented);
}

Result CreateIoPool64(Core::System& system, Handle* out_handle, IoPoolType pool_type) {
    R_RETURN(CreateIoPool(system, out_handle, pool_type));
}

Result CreateIoRegion64(Core::System& system, Handle* out_handle, Handle io_pool,
                        uint64_t physical_address, uint64_t size, MemoryMapping mapping,
                        MemoryPermission perm) {
    R_RETURN(CreateIoRegion(system, out_handle, io_pool, physical_address, size, mapping, perm));
}

Result MapIoRegion64(Core::System& system, Handle io_region, uint64_t address, uint64_t size,
                     MemoryPermission perm) {
    R_RETURN(MapIoRegion(system, io_region, address, size, perm));
}

Result UnmapIoRegion64(Core::System& system, Handle io_region, uint64_t address, uint64_t size) {
    R_RETURN(UnmapIoRegion(system, io_region, address, size));
}

Result CreateIoPool64From32(Core::System& system, Handle* out_handle, IoPoolType pool_type) {
    R_RETURN(CreateIoPool(system, out_handle, pool_type));
}

Result CreateIoRegion64From32(Core::System& system, Handle* out_handle, Handle io_pool,
                              uint64_t physical_address, uint32_t size, MemoryMapping mapping,
                              MemoryPermission perm) {
    R_RETURN(CreateIoRegion(system, out_handle, io_pool, physical_address, size, mapping, perm));
}

Result MapIoRegion64From32(Core::System& system, Handle io_region, uint32_t address, uint32_t size,
                           MemoryPermission perm) {
    R_RETURN(MapIoRegion(system, io_region, address, size, perm));
}

Result UnmapIoRegion64From32(Core::System& system, Handle io_region, uint32_t address,
                             uint32_t size) {
    R_RETURN(UnmapIoRegion(system, io_region, address, size));
}

} // namespace Kernel::Svc
