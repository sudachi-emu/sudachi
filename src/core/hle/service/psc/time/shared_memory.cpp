// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/core.h"
#include "core/hle/kernel/k_shared_memory.h"
#include "core/hle/service/psc/time/shared_memory.h"

namespace Service::PSC::Time {
namespace {
template <typename T>
constexpr inline T ReadFromLockFreeAtomicType(const LockFreeAtomicType<T>* p) {
    while (true) {
        // Get the counter.
        auto counter = p->m_counter;

        // Get the value.
        auto value = p->m_value[counter % 2];

        // Fence memory.
        std::atomic_thread_fence(std::memory_order_acquire);

        // Check that the counter matches.
        if (counter == p->m_counter) {
            return value;
        }
    }
}

template <typename T>
constexpr inline void WriteToLockFreeAtomicType(LockFreeAtomicType<T>* p, const T& value) {
    // Get the current counter.
    auto counter = p->m_counter;

    // Increment the counter.
    ++counter;

    // Store the updated value.
    p->m_value[counter % 2] = value;

    // Fence memory.
    std::atomic_thread_fence(std::memory_order_release);

    // Set the updated counter.
    p->m_counter = counter;
}
} // namespace

SharedMemory::SharedMemory(Core::System& system)
    : m_system{system}, m_k_shared_memory{m_system.Kernel().GetTimeSharedMem()},
      m_shared_memory_ptr{reinterpret_cast<SharedMemoryStruct*>(m_k_shared_memory.GetPointer())} {
    std::memset(m_shared_memory_ptr, 0, sizeof(*m_shared_memory_ptr));
}

void SharedMemory::SetLocalSystemContext(const SystemClockContext& context) {
    WriteToLockFreeAtomicType(&m_shared_memory_ptr->local_system_clock_contexts, context);
}

void SharedMemory::SetNetworkSystemContext(const SystemClockContext& context) {
    WriteToLockFreeAtomicType(&m_shared_memory_ptr->network_system_clock_contexts, context);
}

void SharedMemory::SetSteadyClockTimePoint(ClockSourceId clock_source_id, s64 time_point) {
    WriteToLockFreeAtomicType(&m_shared_memory_ptr->steady_time_points,
                              {time_point, clock_source_id});
}

void SharedMemory::SetContinuousAdjustment(const ContinuousAdjustmentTimePoint& time_point) {
    WriteToLockFreeAtomicType(&m_shared_memory_ptr->continuous_adjustment_time_points, time_point);
}

void SharedMemory::SetAutomaticCorrection(bool automatic_correction) {
    WriteToLockFreeAtomicType(&m_shared_memory_ptr->automatic_corrections, automatic_correction);
}

void SharedMemory::UpdateBaseTime(s64 time) {
    SteadyClockTimePoint time_point{
        ReadFromLockFreeAtomicType(&m_shared_memory_ptr->steady_time_points)};

    time_point.time_point = time;

    WriteToLockFreeAtomicType(&m_shared_memory_ptr->steady_time_points, time_point);
}

} // namespace Service::PSC::Time
