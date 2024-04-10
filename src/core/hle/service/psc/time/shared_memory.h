// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>

#include "common/common_types.h"
#include "core/hle/service/psc/time/common.h"

namespace Core {
class System;
}

namespace Kernel {
class KSharedMemory;
}

namespace Service::PSC::Time {

template <typename T>
struct LockFreeAtomicType {
    u32 m_counter;
    std::array<T, 2> m_value;
};

struct SharedMemoryStruct {
    LockFreeAtomicType<SteadyClockTimePoint> steady_time_points;
    LockFreeAtomicType<SystemClockContext> local_system_clock_contexts;
    LockFreeAtomicType<SystemClockContext> network_system_clock_contexts;
    LockFreeAtomicType<bool> automatic_corrections;
    LockFreeAtomicType<ContinuousAdjustmentTimePoint> continuous_adjustment_time_points;
    std::array<char, 0xEB8> pad0148;
};
static_assert(offsetof(SharedMemoryStruct, steady_time_points) == 0x0,
              "steady_time_points are in the wrong place!");
static_assert(offsetof(SharedMemoryStruct, local_system_clock_contexts) == 0x38,
              "local_system_clock_contexts are in the wrong place!");
static_assert(offsetof(SharedMemoryStruct, network_system_clock_contexts) == 0x80,
              "network_system_clock_contexts are in the wrong place!");
static_assert(offsetof(SharedMemoryStruct, automatic_corrections) == 0xC8,
              "automatic_corrections are in the wrong place!");
static_assert(offsetof(SharedMemoryStruct, continuous_adjustment_time_points) == 0xD0,
              "continuous_adjustment_time_points are in the wrong place!");
static_assert(sizeof(SharedMemoryStruct) == 0x1000,
              "Time's SharedMemoryStruct has the wrong size!");
static_assert(std::is_trivial_v<SharedMemoryStruct>);

class SharedMemory {
public:
    explicit SharedMemory(Core::System& system);

    Kernel::KSharedMemory& GetKSharedMemory() {
        return m_k_shared_memory;
    }

    void SetLocalSystemContext(const SystemClockContext& context);
    void SetNetworkSystemContext(const SystemClockContext& context);
    void SetSteadyClockTimePoint(ClockSourceId clock_source_id, s64 time_diff);
    void SetContinuousAdjustment(const ContinuousAdjustmentTimePoint& time_point);
    void SetAutomaticCorrection(bool automatic_correction);
    void UpdateBaseTime(s64 time);

private:
    Core::System& m_system;
    Kernel::KSharedMemory& m_k_shared_memory;
    SharedMemoryStruct* m_shared_memory_ptr;
};

} // namespace Service::PSC::Time
