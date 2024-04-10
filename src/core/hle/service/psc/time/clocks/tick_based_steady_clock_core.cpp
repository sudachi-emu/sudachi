// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <chrono>

#include "core/core.h"
#include "core/core_timing.h"
#include "core/hle/service/psc/time/clocks/tick_based_steady_clock_core.h"

namespace Service::PSC::Time {

Result TickBasedSteadyClockCore::GetCurrentTimePointImpl(SteadyClockTimePoint& out_time_point) {
    auto ticks{m_system.CoreTiming().GetClockTicks()};
    auto current_time_s =
        std::chrono::duration_cast<std::chrono::seconds>(ConvertToTimeSpan(ticks)).count();
    out_time_point.time_point = current_time_s;
    out_time_point.clock_source_id = m_clock_source_id;
    R_SUCCEED();
}

s64 TickBasedSteadyClockCore::GetCurrentRawTimePointImpl() {
    SteadyClockTimePoint time_point{};
    if (GetCurrentTimePointImpl(time_point) != ResultSuccess) {
        LOG_ERROR(Service_Time, "Failed to GetCurrentTimePoint!");
    }
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
               std::chrono::seconds(time_point.time_point))
        .count();
}

s64 TickBasedSteadyClockCore::GetTestOffsetImpl() const {
    return 0;
}

void TickBasedSteadyClockCore::SetTestOffsetImpl(s64 offset) {}

s64 TickBasedSteadyClockCore::GetInternalOffsetImpl() const {
    return 0;
}

void TickBasedSteadyClockCore::SetInternalOffsetImpl(s64 offset) {}

} // namespace Service::PSC::Time
