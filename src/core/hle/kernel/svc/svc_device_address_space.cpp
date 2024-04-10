// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/alignment.h"
#include "common/scope_exit.h"
#include "core/core.h"
#include "core/hle/kernel/k_device_address_space.h"
#include "core/hle/kernel/k_process.h"
#include "core/hle/kernel/svc.h"

namespace Kernel::Svc {

constexpr inline u64 DeviceAddressSpaceAlignMask = (1ULL << 22) - 1;

constexpr bool IsProcessAndDeviceAligned(uint64_t process_address, uint64_t device_address) {
    return (process_address & DeviceAddressSpaceAlignMask) ==
           (device_address & DeviceAddressSpaceAlignMask);
}

Result CreateDeviceAddressSpace(Core::System& system, Handle* out, uint64_t das_address,
                                uint64_t das_size) {
    // Validate input.
    R_UNLESS(Common::IsAligned(das_address, PageSize), ResultInvalidMemoryRegion);
    R_UNLESS(Common::IsAligned(das_size, PageSize), ResultInvalidMemoryRegion);
    R_UNLESS(das_size > 0, ResultInvalidMemoryRegion);
    R_UNLESS((das_address < das_address + das_size), ResultInvalidMemoryRegion);

    // Create the device address space.
    KDeviceAddressSpace* das = KDeviceAddressSpace::Create(system.Kernel());
    R_UNLESS(das != nullptr, ResultOutOfResource);
    SCOPE_EXIT {
        das->Close();
    };

    // Initialize the device address space.
    R_TRY(das->Initialize(das_address, das_size));

    // Register the device address space.
    KDeviceAddressSpace::Register(system.Kernel(), das);

    // Add to the handle table.
    R_TRY(GetCurrentProcess(system.Kernel()).GetHandleTable().Add(out, das));

    R_SUCCEED();
}

Result AttachDeviceAddressSpace(Core::System& system, DeviceName device_name, Handle das_handle) {
    // Get the device address space.
    KScopedAutoObject das = GetCurrentProcess(system.Kernel())
                                .GetHandleTable()
                                .GetObject<KDeviceAddressSpace>(das_handle);
    R_UNLESS(das.IsNotNull(), ResultInvalidHandle);

    // Attach.
    R_RETURN(das->Attach(device_name));
}

Result DetachDeviceAddressSpace(Core::System& system, DeviceName device_name, Handle das_handle) {
    // Get the device address space.
    KScopedAutoObject das = GetCurrentProcess(system.Kernel())
                                .GetHandleTable()
                                .GetObject<KDeviceAddressSpace>(das_handle);
    R_UNLESS(das.IsNotNull(), ResultInvalidHandle);

    // Detach.
    R_RETURN(das->Detach(device_name));
}

constexpr bool IsValidDeviceMemoryPermission(MemoryPermission device_perm) {
    switch (device_perm) {
    case MemoryPermission::Read:
    case MemoryPermission::Write:
    case MemoryPermission::ReadWrite:
        return true;
    default:
        return false;
    }
}

Result MapDeviceAddressSpaceByForce(Core::System& system, Handle das_handle, Handle process_handle,
                                    uint64_t process_address, uint64_t size,
                                    uint64_t device_address, u32 option) {
    // Decode the option.
    const MapDeviceAddressSpaceOption option_pack{option};
    const auto device_perm = option_pack.permission;
    const auto reserved = option_pack.reserved;

    // Validate input.
    R_UNLESS(Common::IsAligned(process_address, PageSize), ResultInvalidAddress);
    R_UNLESS(Common::IsAligned(device_address, PageSize), ResultInvalidAddress);
    R_UNLESS(Common::IsAligned(size, PageSize), ResultInvalidSize);
    R_UNLESS(size > 0, ResultInvalidSize);
    R_UNLESS((process_address < process_address + size), ResultInvalidCurrentMemory);
    R_UNLESS((device_address < device_address + size), ResultInvalidMemoryRegion);
    R_UNLESS((process_address == static_cast<uint64_t>(process_address)),
             ResultInvalidCurrentMemory);
    R_UNLESS(IsValidDeviceMemoryPermission(device_perm), ResultInvalidNewMemoryPermission);
    R_UNLESS(reserved == 0, ResultInvalidEnumValue);

    // Get the device address space.
    KScopedAutoObject das = GetCurrentProcess(system.Kernel())
                                .GetHandleTable()
                                .GetObject<KDeviceAddressSpace>(das_handle);
    R_UNLESS(das.IsNotNull(), ResultInvalidHandle);

    // Get the process.
    KScopedAutoObject process =
        GetCurrentProcess(system.Kernel()).GetHandleTable().GetObject<KProcess>(process_handle);
    R_UNLESS(process.IsNotNull(), ResultInvalidHandle);

    // Validate that the process address is within range.
    auto& page_table = process->GetPageTable();
    R_UNLESS(page_table.Contains(process_address, size), ResultInvalidCurrentMemory);

    // Map.
    R_RETURN(
        das->MapByForce(std::addressof(page_table), process_address, size, device_address, option));
}

Result MapDeviceAddressSpaceAligned(Core::System& system, Handle das_handle, Handle process_handle,
                                    uint64_t process_address, uint64_t size,
                                    uint64_t device_address, u32 option) {
    // Decode the option.
    const MapDeviceAddressSpaceOption option_pack{option};
    const auto device_perm = option_pack.permission;
    const auto reserved = option_pack.reserved;

    // Validate input.
    R_UNLESS(Common::IsAligned(process_address, PageSize), ResultInvalidAddress);
    R_UNLESS(Common::IsAligned(device_address, PageSize), ResultInvalidAddress);
    R_UNLESS(IsProcessAndDeviceAligned(process_address, device_address), ResultInvalidAddress);
    R_UNLESS(Common::IsAligned(size, PageSize), ResultInvalidSize);
    R_UNLESS(size > 0, ResultInvalidSize);
    R_UNLESS((process_address < process_address + size), ResultInvalidCurrentMemory);
    R_UNLESS((device_address < device_address + size), ResultInvalidMemoryRegion);
    R_UNLESS((process_address == static_cast<uint64_t>(process_address)),
             ResultInvalidCurrentMemory);
    R_UNLESS(IsValidDeviceMemoryPermission(device_perm), ResultInvalidNewMemoryPermission);
    R_UNLESS(reserved == 0, ResultInvalidEnumValue);

    // Get the device address space.
    KScopedAutoObject das = GetCurrentProcess(system.Kernel())
                                .GetHandleTable()
                                .GetObject<KDeviceAddressSpace>(das_handle);
    R_UNLESS(das.IsNotNull(), ResultInvalidHandle);

    // Get the process.
    KScopedAutoObject process =
        GetCurrentProcess(system.Kernel()).GetHandleTable().GetObject<KProcess>(process_handle);
    R_UNLESS(process.IsNotNull(), ResultInvalidHandle);

    // Validate that the process address is within range.
    auto& page_table = process->GetPageTable();
    R_UNLESS(page_table.Contains(process_address, size), ResultInvalidCurrentMemory);

    // Map.
    R_RETURN(
        das->MapAligned(std::addressof(page_table), process_address, size, device_address, option));
}

Result UnmapDeviceAddressSpace(Core::System& system, Handle das_handle, Handle process_handle,
                               uint64_t process_address, uint64_t size, uint64_t device_address) {
    // Validate input.
    R_UNLESS(Common::IsAligned(process_address, PageSize), ResultInvalidAddress);
    R_UNLESS(Common::IsAligned(device_address, PageSize), ResultInvalidAddress);
    R_UNLESS(Common::IsAligned(size, PageSize), ResultInvalidSize);
    R_UNLESS(size > 0, ResultInvalidSize);
    R_UNLESS((process_address < process_address + size), ResultInvalidCurrentMemory);
    R_UNLESS((device_address < device_address + size), ResultInvalidMemoryRegion);
    R_UNLESS((process_address == static_cast<uint64_t>(process_address)),
             ResultInvalidCurrentMemory);

    // Get the device address space.
    KScopedAutoObject das = GetCurrentProcess(system.Kernel())
                                .GetHandleTable()
                                .GetObject<KDeviceAddressSpace>(das_handle);
    R_UNLESS(das.IsNotNull(), ResultInvalidHandle);

    // Get the process.
    KScopedAutoObject process =
        GetCurrentProcess(system.Kernel()).GetHandleTable().GetObject<KProcess>(process_handle);
    R_UNLESS(process.IsNotNull(), ResultInvalidHandle);

    // Validate that the process address is within range.
    auto& page_table = process->GetPageTable();
    R_UNLESS(page_table.Contains(process_address, size), ResultInvalidCurrentMemory);

    R_RETURN(das->Unmap(std::addressof(page_table), process_address, size, device_address));
}

Result CreateDeviceAddressSpace64(Core::System& system, Handle* out_handle, uint64_t das_address,
                                  uint64_t das_size) {
    R_RETURN(CreateDeviceAddressSpace(system, out_handle, das_address, das_size));
}

Result AttachDeviceAddressSpace64(Core::System& system, DeviceName device_name, Handle das_handle) {
    R_RETURN(AttachDeviceAddressSpace(system, device_name, das_handle));
}

Result DetachDeviceAddressSpace64(Core::System& system, DeviceName device_name, Handle das_handle) {
    R_RETURN(DetachDeviceAddressSpace(system, device_name, das_handle));
}

Result MapDeviceAddressSpaceByForce64(Core::System& system, Handle das_handle,
                                      Handle process_handle, uint64_t process_address,
                                      uint64_t size, uint64_t device_address, u32 option) {
    R_RETURN(MapDeviceAddressSpaceByForce(system, das_handle, process_handle, process_address, size,
                                          device_address, option));
}

Result MapDeviceAddressSpaceAligned64(Core::System& system, Handle das_handle,
                                      Handle process_handle, uint64_t process_address,
                                      uint64_t size, uint64_t device_address, u32 option) {
    R_RETURN(MapDeviceAddressSpaceAligned(system, das_handle, process_handle, process_address, size,
                                          device_address, option));
}

Result UnmapDeviceAddressSpace64(Core::System& system, Handle das_handle, Handle process_handle,
                                 uint64_t process_address, uint64_t size, uint64_t device_address) {
    R_RETURN(UnmapDeviceAddressSpace(system, das_handle, process_handle, process_address, size,
                                     device_address));
}

Result CreateDeviceAddressSpace64From32(Core::System& system, Handle* out_handle,
                                        uint64_t das_address, uint64_t das_size) {
    R_RETURN(CreateDeviceAddressSpace(system, out_handle, das_address, das_size));
}

Result AttachDeviceAddressSpace64From32(Core::System& system, DeviceName device_name,
                                        Handle das_handle) {
    R_RETURN(AttachDeviceAddressSpace(system, device_name, das_handle));
}

Result DetachDeviceAddressSpace64From32(Core::System& system, DeviceName device_name,
                                        Handle das_handle) {
    R_RETURN(DetachDeviceAddressSpace(system, device_name, das_handle));
}

Result MapDeviceAddressSpaceByForce64From32(Core::System& system, Handle das_handle,
                                            Handle process_handle, uint64_t process_address,
                                            uint32_t size, uint64_t device_address, u32 option) {
    R_RETURN(MapDeviceAddressSpaceByForce(system, das_handle, process_handle, process_address, size,
                                          device_address, option));
}

Result MapDeviceAddressSpaceAligned64From32(Core::System& system, Handle das_handle,
                                            Handle process_handle, uint64_t process_address,
                                            uint32_t size, uint64_t device_address, u32 option) {
    R_RETURN(MapDeviceAddressSpaceAligned(system, das_handle, process_handle, process_address, size,
                                          device_address, option));
}

Result UnmapDeviceAddressSpace64From32(Core::System& system, Handle das_handle,
                                       Handle process_handle, uint64_t process_address,
                                       uint32_t size, uint64_t device_address) {
    R_RETURN(UnmapDeviceAddressSpace(system, das_handle, process_handle, process_address, size,
                                     device_address));
}

} // namespace Kernel::Svc
