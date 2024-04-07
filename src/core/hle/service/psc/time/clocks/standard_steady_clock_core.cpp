// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <chrono>

#include "core/core.h"
#include "core/core_timing.h"
#include "core/hle/service/psc/time/clocks/standard_steady_clock_core.h"

namespace Service::PSC::Time {

void StandardSteadyClockCore::Initialize(ClockSourceId clock_source_id, s64 rtc_offset,
                                         s64 internal_offset, s64 test_offset,
                                         bool is_rtc_reset_detected) {
    m_clock_source_id = clock_source_id;
    m_rtc_offset = rtc_offset;
    m_internal_offset = internal_offset;
    m_test_offset = test_offset;
    if (is_rtc_reset_detected) {
        SetResetDetected();
    }
    SetInitialized();
}

void StandardSteadyClockCore::SetRtcOffset(s64 offset) {
    m_rtc_offset = offset;
}

void StandardSteadyClockCore::SetContinuousAdjustment(ClockSourceId clock_source_id, s64 time) {
    auto ticks{m_system.CoreTiming().GetClockTicks()};

    m_continuous_adjustment_time_point.rtc_offset = ConvertToTimeSpan(ticks).count();
    m_continuous_adjustment_time_point.diff_scale = 0;
    m_continuous_adjustment_time_point.shift_amount = 0;
    m_continuous_adjustment_time_point.lower = time;
    m_continuous_adjustment_time_point.upper = time;
    m_continuous_adjustment_time_point.clock_source_id = clock_source_id;
}

void StandardSteadyClockCore::GetContinuousAdjustment(
    ContinuousAdjustmentTimePoint& out_time_point) const {
    out_time_point = m_continuous_adjustment_time_point;
}

void StandardSteadyClockCore::UpdateContinuousAdjustmentTime(s64 in_time) {
    auto ticks{m_system.CoreTiming().GetClockTicks()};
    auto uptime_ns{ConvertToTimeSpan(ticks).count()};
    auto adjusted_time{((uptime_ns - m_continuous_adjustment_time_point.rtc_offset) *
                        m_continuous_adjustment_time_point.diff_scale) >>
                       m_continuous_adjustment_time_point.shift_amount};
    auto expected_time{adjusted_time + m_continuous_adjustment_time_point.lower};

    auto last_time_point{m_continuous_adjustment_time_point.upper};
    m_continuous_adjustment_time_point.upper = in_time;
    auto t1{std::min<s64>(expected_time, last_time_point)};
    expected_time = std::max<s64>(expected_time, last_time_point);
    expected_time = m_continuous_adjustment_time_point.diff_scale >= 0 ? t1 : expected_time;

    auto new_diff{in_time < expected_time ? -55 : 55};

    m_continuous_adjustment_time_point.rtc_offset = uptime_ns;
    m_continuous_adjustment_time_point.shift_amount = expected_time == in_time ? 0 : 14;
    m_continuous_adjustment_time_point.diff_scale = expected_time == in_time ? 0 : new_diff;
    m_continuous_adjustment_time_point.lower = expected_time;
}

Result StandardSteadyClockCore::GetCurrentTimePointImpl(SteadyClockTimePoint& out_time_point) {
    auto current_time_ns = GetCurrentRawTimePointImpl();
    auto current_time_s =
        std::chrono::duration_cast<std::chrono::seconds>(std::chrono::nanoseconds(current_time_ns));
    out_time_point.time_point = current_time_s.count();
    out_time_point.clock_source_id = m_clock_source_id;
    R_SUCCEED();
}

s64 StandardSteadyClockCore::GetCurrentRawTimePointImpl() {
    std::scoped_lock l{m_mutex};
    auto ticks{static_cast<s64>(m_system.CoreTiming().GetClockTicks())};
    auto current_time_ns = m_rtc_offset + ConvertToTimeSpan(ticks).count();
    auto time_point = std::max<s64>(current_time_ns, m_cached_time_point);
    m_cached_time_point = time_point;
    return time_point;
}

s64 StandardSteadyClockCore::GetTestOffsetImpl() const {
    return m_test_offset;
}

void StandardSteadyClockCore::SetTestOffsetImpl(s64 offset) {
    m_test_offset = offset;
}

s64 StandardSteadyClockCore::GetInternalOffsetImpl() const {
    return m_internal_offset;
}

void StandardSteadyClockCore::SetInternalOffsetImpl(s64 offset) {
    m_internal_offset = offset;
}

} // namespace Service::PSC::Time
