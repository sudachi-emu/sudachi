// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/hardware_properties.h"
#include "core/hle/kernel/k_capabilities.h"
#include "core/hle/kernel/k_memory_layout.h"
#include "core/hle/kernel/k_process_page_table.h"
#include "core/hle/kernel/k_trace.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/svc_results.h"
#include "core/hle/kernel/svc_version.h"

namespace Kernel {

Result KCapabilities::InitializeForKip(std::span<const u32> kern_caps,
                                       KProcessPageTable* page_table) {
    // We're initializing an initial process.
    m_svc_access_flags.reset();
    m_irq_access_flags.reset();
    m_debug_capabilities = 0;
    m_handle_table_size = 0;
    m_intended_kernel_version = 0;
    m_program_type = 0;

    // Initial processes may run on all cores.
    constexpr u64 VirtMask = Core::Hardware::VirtualCoreMask;
    constexpr u64 PhysMask = Core::Hardware::ConvertVirtualCoreMaskToPhysical(VirtMask);

    m_core_mask = VirtMask;
    m_phys_core_mask = PhysMask;

    // Initial processes may use any user priority they like.
    m_priority_mask = ~0xFULL;

    // Here, Nintendo sets the kernel version to the current kernel version.
    // We will follow suit and set the version to the highest supported kernel version.
    KernelVersion intended_kernel_version{};
    intended_kernel_version.major_version.Assign(Svc::SupportedKernelMajorVersion);
    intended_kernel_version.minor_version.Assign(Svc::SupportedKernelMinorVersion);
    m_intended_kernel_version = intended_kernel_version.raw;

    // Parse the capabilities array.
    R_RETURN(this->SetCapabilities(kern_caps, page_table));
}

Result KCapabilities::InitializeForUser(std::span<const u32> user_caps,
                                        KProcessPageTable* page_table) {
    // We're initializing a user process.
    m_svc_access_flags.reset();
    m_irq_access_flags.reset();
    m_debug_capabilities = 0;
    m_handle_table_size = 0;
    m_intended_kernel_version = 0;
    m_program_type = 0;

    // User processes must specify what cores/priorities they can use.
    m_core_mask = 0;
    m_priority_mask = 0;

    // Parse the user capabilities array.
    R_RETURN(this->SetCapabilities(user_caps, page_table));
}

Result KCapabilities::SetCorePriorityCapability(const u32 cap) {
    // We can't set core/priority if we've already set them.
    R_UNLESS(m_core_mask == 0, ResultInvalidArgument);
    R_UNLESS(m_priority_mask == 0, ResultInvalidArgument);

    // Validate the core/priority.
    CorePriority pack{cap};
    const u32 min_core = pack.minimum_core_id;
    const u32 max_core = pack.maximum_core_id;
    const u32 max_prio = pack.lowest_thread_priority;
    const u32 min_prio = pack.highest_thread_priority;

    R_UNLESS(min_core <= max_core, ResultInvalidCombination);
    R_UNLESS(min_prio <= max_prio, ResultInvalidCombination);
    R_UNLESS(max_core < Core::Hardware::NumVirtualCores, ResultInvalidCoreId);

    ASSERT(max_prio < Common::BitSize<u64>());

    // Set core mask.
    for (auto core_id = min_core; core_id <= max_core; core_id++) {
        m_core_mask |= (1ULL << core_id);
    }
    ASSERT((m_core_mask & Core::Hardware::VirtualCoreMask) == m_core_mask);

    // Set physical core mask.
    m_phys_core_mask = Core::Hardware::ConvertVirtualCoreMaskToPhysical(m_core_mask);

    // Set priority mask.
    for (auto prio = min_prio; prio <= max_prio; prio++) {
        m_priority_mask |= (1ULL << prio);
    }

    // We must have some core/priority we can use.
    R_UNLESS(m_core_mask != 0, ResultInvalidArgument);
    R_UNLESS(m_priority_mask != 0, ResultInvalidArgument);

    // Processes must not have access to kernel thread priorities.
    R_UNLESS((m_priority_mask & 0xF) == 0, ResultInvalidArgument);

    R_SUCCEED();
}

Result KCapabilities::SetSyscallMaskCapability(const u32 cap, u32& set_svc) {
    // Validate the index.
    SyscallMask pack{cap};
    const u32 mask = pack.mask;
    const u32 index = pack.index;

    const u32 index_flag = (1U << index);
    R_UNLESS((set_svc & index_flag) == 0, ResultInvalidCombination);
    set_svc |= index_flag;

    // Set SVCs.
    for (size_t i = 0; i < decltype(SyscallMask::mask)::bits; i++) {
        const u32 svc_id = static_cast<u32>(decltype(SyscallMask::mask)::bits * index + i);
        if (mask & (1U << i)) {
            R_UNLESS(this->SetSvcAllowed(svc_id), ResultOutOfRange);
        }
    }

    R_SUCCEED();
}

Result KCapabilities::MapRange_(const u32 cap, const u32 size_cap, KProcessPageTable* page_table) {
    const auto range_pack = MapRange{cap};
    const auto size_pack = MapRangeSize{size_cap};

    // Get/validate address/size
    const u64 phys_addr = range_pack.address.Value() * PageSize;

    // Validate reserved bits are unused.
    R_UNLESS(size_pack.reserved.Value() == 0, ResultOutOfRange);

    const size_t num_pages = size_pack.pages;
    const size_t size = num_pages * PageSize;
    R_UNLESS(num_pages != 0, ResultInvalidSize);
    R_UNLESS(phys_addr < phys_addr + size, ResultInvalidAddress);
    R_UNLESS(((phys_addr + size - 1) & ~PhysicalMapAllowedMask) == 0, ResultInvalidAddress);

    // Do the mapping.
    [[maybe_unused]] const KMemoryPermission perm = range_pack.read_only.Value()
                                                        ? KMemoryPermission::UserRead
                                                        : KMemoryPermission::UserReadWrite;
    if (MapRangeSize{size_cap}.normal) {
        R_RETURN(page_table->MapStatic(phys_addr, size, perm));
    } else {
        R_RETURN(page_table->MapIo(phys_addr, size, perm));
    }
}

Result KCapabilities::MapIoPage_(const u32 cap, KProcessPageTable* page_table) {
    // Get/validate address/size
    const u64 phys_addr = MapIoPage{cap}.address.Value() * PageSize;
    const size_t num_pages = 1;
    const size_t size = num_pages * PageSize;
    R_UNLESS(phys_addr < phys_addr + size, ResultInvalidAddress);
    R_UNLESS(((phys_addr + size - 1) & ~PhysicalMapAllowedMask) == 0, ResultInvalidAddress);

    // Do the mapping.
    R_RETURN(page_table->MapIo(phys_addr, size, KMemoryPermission::UserReadWrite));
}

template <typename F>
Result KCapabilities::ProcessMapRegionCapability(const u32 cap, F f) {
    // Define the allowed memory regions.
    constexpr std::array<KMemoryRegionType, 4> MemoryRegions{
        KMemoryRegionType_None,
        KMemoryRegionType_KernelTraceBuffer,
        KMemoryRegionType_OnMemoryBootImage,
        KMemoryRegionType_DTB,
    };

    // Extract regions/read only.
    const MapRegion pack{cap};
    const std::array<RegionType, 3> types{pack.region0, pack.region1, pack.region2};
    const std::array<u32, 3> ro{pack.read_only0, pack.read_only1, pack.read_only2};

    for (size_t i = 0; i < types.size(); i++) {
        const auto type = types[i];
        const auto perm = ro[i] ? KMemoryPermission::UserRead : KMemoryPermission::UserReadWrite;
        switch (type) {
        case RegionType::NoMapping:
            break;
        case RegionType::KernelTraceBuffer:
            if constexpr (!IsKTraceEnabled) {
                break;
            }
            [[fallthrough]];
        case RegionType::OnMemoryBootImage:
        case RegionType::DTB:
            R_TRY(f(MemoryRegions[static_cast<u32>(type)], perm));
            break;
        default:
            R_THROW(ResultNotFound);
        }
    }

    R_SUCCEED();
}

Result KCapabilities::MapRegion_(const u32 cap, KProcessPageTable* page_table) {
    // Map each region into the process's page table.
    return ProcessMapRegionCapability(
        cap, [page_table](KMemoryRegionType region_type, KMemoryPermission perm) -> Result {
            R_RETURN(page_table->MapRegion(region_type, perm));
        });
}

Result KCapabilities::CheckMapRegion(KernelCore& kernel, const u32 cap) {
    // Check that each region has a physical backing store.
    return ProcessMapRegionCapability(
        cap, [&](KMemoryRegionType region_type, KMemoryPermission perm) -> Result {
            R_UNLESS(kernel.MemoryLayout().GetPhysicalMemoryRegionTree().FindFirstDerived(
                         region_type) != nullptr,
                     ResultOutOfRange);
            R_SUCCEED();
        });
}

Result KCapabilities::SetInterruptPairCapability(const u32 cap) {
    // Extract interrupts.
    const InterruptPair pack{cap};
    const std::array<u32, 2> ids{pack.interrupt_id0, pack.interrupt_id1};

    for (size_t i = 0; i < ids.size(); i++) {
        if (ids[i] != PaddingInterruptId) {
            UNIMPLEMENTED();
            // R_UNLESS(Kernel::GetInterruptManager().IsInterruptDefined(ids[i]), ResultOutOfRange);
            // R_UNLESS(this->SetInterruptPermitted(ids[i]), ResultOutOfRange);
        }
    }

    R_SUCCEED();
}

Result KCapabilities::SetProgramTypeCapability(const u32 cap) {
    // Validate.
    const ProgramType pack{cap};
    R_UNLESS(pack.reserved == 0, ResultReservedUsed);

    m_program_type = pack.type;
    R_SUCCEED();
}

Result KCapabilities::SetKernelVersionCapability(const u32 cap) {
    // Ensure we haven't set our version before.
    R_UNLESS(KernelVersion{m_intended_kernel_version}.major_version == 0, ResultInvalidArgument);

    // Set, ensure that we set a valid version.
    m_intended_kernel_version = cap;
    R_UNLESS(KernelVersion{m_intended_kernel_version}.major_version != 0, ResultInvalidArgument);

    R_SUCCEED();
}

Result KCapabilities::SetHandleTableCapability(const u32 cap) {
    // Validate.
    const HandleTable pack{cap};
    R_UNLESS(pack.reserved == 0, ResultReservedUsed);

    m_handle_table_size = pack.size;
    R_SUCCEED();
}

Result KCapabilities::SetDebugFlagsCapability(const u32 cap) {
    // Validate.
    const DebugFlags pack{cap};
    R_UNLESS(pack.reserved == 0, ResultReservedUsed);

    DebugFlags debug_capabilities{m_debug_capabilities};
    debug_capabilities.allow_debug.Assign(pack.allow_debug);
    debug_capabilities.force_debug.Assign(pack.force_debug);
    m_debug_capabilities = debug_capabilities.raw;

    R_SUCCEED();
}

Result KCapabilities::SetCapability(const u32 cap, u32& set_flags, u32& set_svc,
                                    KProcessPageTable* page_table) {
    // Validate this is a capability we can act on.
    const auto type = GetCapabilityType(cap);
    R_UNLESS(type != CapabilityType::Invalid, ResultInvalidArgument);

    // If the type is padding, we have no work to do.
    R_SUCCEED_IF(type == CapabilityType::Padding);

    // Check that we haven't already processed this capability.
    const auto flag = GetCapabilityFlag(type);
    R_UNLESS(((set_flags & InitializeOnceFlags) & flag) == 0, ResultInvalidCombination);
    set_flags |= flag;

    // Process the capability.
    switch (type) {
    case CapabilityType::CorePriority:
        R_RETURN(this->SetCorePriorityCapability(cap));
    case CapabilityType::SyscallMask:
        R_RETURN(this->SetSyscallMaskCapability(cap, set_svc));
    case CapabilityType::MapIoPage:
        R_RETURN(this->MapIoPage_(cap, page_table));
    case CapabilityType::MapRegion:
        R_RETURN(this->MapRegion_(cap, page_table));
    case CapabilityType::InterruptPair:
        R_RETURN(this->SetInterruptPairCapability(cap));
    case CapabilityType::ProgramType:
        R_RETURN(this->SetProgramTypeCapability(cap));
    case CapabilityType::KernelVersion:
        R_RETURN(this->SetKernelVersionCapability(cap));
    case CapabilityType::HandleTable:
        R_RETURN(this->SetHandleTableCapability(cap));
    case CapabilityType::DebugFlags:
        R_RETURN(this->SetDebugFlagsCapability(cap));
    default:
        R_THROW(ResultInvalidArgument);
    }
}

Result KCapabilities::SetCapabilities(std::span<const u32> caps, KProcessPageTable* page_table) {
    u32 set_flags = 0, set_svc = 0;

    for (size_t i = 0; i < caps.size(); i++) {
        const u32 cap{caps[i]};

        if (GetCapabilityType(cap) == CapabilityType::MapRange) {
            // Check that the pair cap exists.
            R_UNLESS((++i) < caps.size(), ResultInvalidCombination);

            // Check the pair cap is a map range cap.
            const u32 size_cap{caps[i]};
            R_UNLESS(GetCapabilityType(size_cap) == CapabilityType::MapRange,
                     ResultInvalidCombination);

            // Map the range.
            R_TRY(this->MapRange_(cap, size_cap, page_table));
        } else {
            R_TRY(this->SetCapability(cap, set_flags, set_svc, page_table));
        }
    }

    R_SUCCEED();
}

Result KCapabilities::CheckCapabilities(KernelCore& kernel, std::span<const u32> caps) {
    for (auto cap : caps) {
        // Check the capability refers to a valid region.
        if (GetCapabilityType(cap) == CapabilityType::MapRegion) {
            R_TRY(CheckMapRegion(kernel, cap));
        }
    }

    R_SUCCEED();
}

} // namespace Kernel
