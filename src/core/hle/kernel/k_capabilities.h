
// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <bitset>
#include <span>

#include "common/bit_field.h"
#include "common/common_types.h"

#include "core/hle/kernel/svc_types.h"
#include "core/hle/result.h"

namespace Kernel {

class KProcessPageTable;
class KernelCore;

class KCapabilities {
public:
    constexpr explicit KCapabilities() = default;

    Result InitializeForKip(std::span<const u32> kern_caps, KProcessPageTable* page_table);
    Result InitializeForUser(std::span<const u32> user_caps, KProcessPageTable* page_table);

    static Result CheckCapabilities(KernelCore& kernel, std::span<const u32> user_caps);

    constexpr u64 GetCoreMask() const {
        return m_core_mask;
    }

    constexpr u64 GetPhysicalCoreMask() const {
        return m_phys_core_mask;
    }

    constexpr u64 GetPriorityMask() const {
        return m_priority_mask;
    }

    constexpr s32 GetHandleTableSize() const {
        return m_handle_table_size;
    }

    constexpr const Svc::SvcAccessFlagSet& GetSvcPermissions() const {
        return m_svc_access_flags;
    }

    constexpr bool IsPermittedSvc(u32 id) const {
        return (id < m_svc_access_flags.size()) && m_svc_access_flags[id];
    }

    constexpr bool IsPermittedInterrupt(u32 id) const {
        return (id < m_irq_access_flags.size()) && m_irq_access_flags[id];
    }

    constexpr bool IsPermittedDebug() const {
        return DebugFlags{m_debug_capabilities}.allow_debug.Value() != 0;
    }

    constexpr bool CanForceDebug() const {
        return DebugFlags{m_debug_capabilities}.force_debug.Value() != 0;
    }

    constexpr u32 GetIntendedKernelMajorVersion() const {
        return KernelVersion{m_intended_kernel_version}.major_version;
    }

    constexpr u32 GetIntendedKernelMinorVersion() const {
        return KernelVersion{m_intended_kernel_version}.minor_version;
    }

private:
    static constexpr size_t InterruptIdCount = 0x400;
    using InterruptFlagSet = std::bitset<InterruptIdCount>;

    enum class CapabilityType : u32 {
        CorePriority = (1U << 3) - 1,
        SyscallMask = (1U << 4) - 1,
        MapRange = (1U << 6) - 1,
        MapIoPage = (1U << 7) - 1,
        MapRegion = (1U << 10) - 1,
        InterruptPair = (1U << 11) - 1,
        ProgramType = (1U << 13) - 1,
        KernelVersion = (1U << 14) - 1,
        HandleTable = (1U << 15) - 1,
        DebugFlags = (1U << 16) - 1,

        Invalid = 0U,
        Padding = ~0U,
    };

    using RawCapabilityValue = u32;

    static constexpr CapabilityType GetCapabilityType(const RawCapabilityValue value) {
        return static_cast<CapabilityType>((~value & (value + 1)) - 1);
    }

    static constexpr u32 GetCapabilityFlag(CapabilityType type) {
        return static_cast<u32>(type) + 1;
    }

    template <CapabilityType Type>
    static constexpr inline u32 CapabilityFlag = static_cast<u32>(Type) + 1;

    template <CapabilityType Type>
    static constexpr inline u32 CapabilityId = std::countr_zero(CapabilityFlag<Type>);

    union CorePriority {
        static_assert(CapabilityId<CapabilityType::CorePriority> + 1 == 4);

        RawCapabilityValue raw;
        BitField<0, 4, CapabilityType> id;
        BitField<4, 6, u32> lowest_thread_priority;
        BitField<10, 6, u32> highest_thread_priority;
        BitField<16, 8, u32> minimum_core_id;
        BitField<24, 8, u32> maximum_core_id;
    };

    union SyscallMask {
        static_assert(CapabilityId<CapabilityType::SyscallMask> + 1 == 5);

        RawCapabilityValue raw;
        BitField<0, 5, CapabilityType> id;
        BitField<5, 24, u32> mask;
        BitField<29, 3, u32> index;
    };

    // #undef MESOSPHERE_ENABLE_LARGE_PHYSICAL_ADDRESS_CAPABILITIES
    static constexpr u64 PhysicalMapAllowedMask = (1ULL << 36) - 1;

    union MapRange {
        static_assert(CapabilityId<CapabilityType::MapRange> + 1 == 7);

        RawCapabilityValue raw;
        BitField<0, 7, CapabilityType> id;
        BitField<7, 24, u32> address;
        BitField<31, 1, u32> read_only;
    };

    union MapRangeSize {
        static_assert(CapabilityId<CapabilityType::MapRange> + 1 == 7);

        RawCapabilityValue raw;
        BitField<0, 7, CapabilityType> id;
        BitField<7, 20, u32> pages;
        BitField<27, 4, u32> reserved;
        BitField<31, 1, u32> normal;
    };

    union MapIoPage {
        static_assert(CapabilityId<CapabilityType::MapIoPage> + 1 == 8);

        RawCapabilityValue raw;
        BitField<0, 8, CapabilityType> id;
        BitField<8, 24, u32> address;
    };

    enum class RegionType : u32 {
        NoMapping = 0,
        KernelTraceBuffer = 1,
        OnMemoryBootImage = 2,
        DTB = 3,
    };

    union MapRegion {
        static_assert(CapabilityId<CapabilityType::MapRegion> + 1 == 11);

        RawCapabilityValue raw;
        BitField<0, 11, CapabilityType> id;
        BitField<11, 6, RegionType> region0;
        BitField<17, 1, u32> read_only0;
        BitField<18, 6, RegionType> region1;
        BitField<24, 1, u32> read_only1;
        BitField<25, 6, RegionType> region2;
        BitField<31, 1, u32> read_only2;
    };

    union InterruptPair {
        static_assert(CapabilityId<CapabilityType::InterruptPair> + 1 == 12);

        RawCapabilityValue raw;
        BitField<0, 12, CapabilityType> id;
        BitField<12, 10, u32> interrupt_id0;
        BitField<22, 10, u32> interrupt_id1;
    };

    union ProgramType {
        static_assert(CapabilityId<CapabilityType::ProgramType> + 1 == 14);

        RawCapabilityValue raw;
        BitField<0, 14, CapabilityType> id;
        BitField<14, 3, u32> type;
        BitField<17, 15, u32> reserved;
    };

    union KernelVersion {
        static_assert(CapabilityId<CapabilityType::KernelVersion> + 1 == 15);

        RawCapabilityValue raw;
        BitField<0, 15, CapabilityType> id;
        BitField<15, 4, u32> minor_version;
        BitField<19, 13, u32> major_version;
    };

    union HandleTable {
        static_assert(CapabilityId<CapabilityType::HandleTable> + 1 == 16);

        RawCapabilityValue raw;
        BitField<0, 16, CapabilityType> id;
        BitField<16, 10, u32> size;
        BitField<26, 6, u32> reserved;
    };

    union DebugFlags {
        static_assert(CapabilityId<CapabilityType::DebugFlags> + 1 == 17);

        RawCapabilityValue raw;
        BitField<0, 17, CapabilityType> id;
        BitField<17, 1, u32> allow_debug;
        BitField<18, 1, u32> force_debug;
        BitField<19, 13, u32> reserved;
    };

    static_assert(sizeof(CorePriority) == 4);
    static_assert(sizeof(SyscallMask) == 4);
    static_assert(sizeof(MapRange) == 4);
    static_assert(sizeof(MapRangeSize) == 4);
    static_assert(sizeof(MapIoPage) == 4);
    static_assert(sizeof(MapRegion) == 4);
    static_assert(sizeof(InterruptPair) == 4);
    static_assert(sizeof(ProgramType) == 4);
    static_assert(sizeof(KernelVersion) == 4);
    static_assert(sizeof(HandleTable) == 4);
    static_assert(sizeof(DebugFlags) == 4);

    static constexpr u32 InitializeOnceFlags =
        CapabilityFlag<CapabilityType::CorePriority> | CapabilityFlag<CapabilityType::ProgramType> |
        CapabilityFlag<CapabilityType::KernelVersion> |
        CapabilityFlag<CapabilityType::HandleTable> | CapabilityFlag<CapabilityType::DebugFlags>;

    static const u32 PaddingInterruptId = 0x3FF;
    static_assert(PaddingInterruptId < InterruptIdCount);

private:
    constexpr bool SetSvcAllowed(u32 id) {
        if (id < m_svc_access_flags.size()) [[likely]] {
            m_svc_access_flags[id] = true;
            return true;
        } else {
            return false;
        }
    }

    constexpr bool SetInterruptPermitted(u32 id) {
        if (id < m_irq_access_flags.size()) [[likely]] {
            m_irq_access_flags[id] = true;
            return true;
        } else {
            return false;
        }
    }

    Result SetCorePriorityCapability(const u32 cap);
    Result SetSyscallMaskCapability(const u32 cap, u32& set_svc);
    Result MapRange_(const u32 cap, const u32 size_cap, KProcessPageTable* page_table);
    Result MapIoPage_(const u32 cap, KProcessPageTable* page_table);
    Result MapRegion_(const u32 cap, KProcessPageTable* page_table);
    Result SetInterruptPairCapability(const u32 cap);
    Result SetProgramTypeCapability(const u32 cap);
    Result SetKernelVersionCapability(const u32 cap);
    Result SetHandleTableCapability(const u32 cap);
    Result SetDebugFlagsCapability(const u32 cap);

    template <typename F>
    static Result ProcessMapRegionCapability(const u32 cap, F f);
    static Result CheckMapRegion(KernelCore& kernel, const u32 cap);

    Result SetCapability(const u32 cap, u32& set_flags, u32& set_svc,
                         KProcessPageTable* page_table);
    Result SetCapabilities(std::span<const u32> caps, KProcessPageTable* page_table);

private:
    Svc::SvcAccessFlagSet m_svc_access_flags{};
    InterruptFlagSet m_irq_access_flags{};
    u64 m_core_mask{};
    u64 m_phys_core_mask{};
    u64 m_priority_mask{};
    u32 m_debug_capabilities{};
    s32 m_handle_table_size{};
    u32 m_intended_kernel_version{};
    u32 m_program_type{};
};

} // namespace Kernel
