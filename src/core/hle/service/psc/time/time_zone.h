// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <mutex>
#include <span>

#include <tz/tz.h>
#include "core/hle/service/psc/time/common.h"

namespace Service::PSC::Time {

class TimeZone {
public:
    TimeZone() = default;

    bool IsInitialized() const {
        return m_initialized;
    }

    void SetInitialized() {
        m_initialized = true;
    }

    void SetTimePoint(const SteadyClockTimePoint& time_point);
    void SetTotalLocationNameCount(u32 count);
    void SetRuleVersion(const RuleVersion& rule_version);
    Result GetLocationName(LocationName& out_name);
    Result GetTotalLocationCount(u32& out_count);
    Result GetRuleVersion(RuleVersion& out_rule_version);
    Result GetTimePoint(SteadyClockTimePoint& out_time_point);

    Result ToCalendarTime(CalendarTime& out_calendar_time,
                          CalendarAdditionalInfo& out_additional_info, s64 time,
                          const Tz::Rule& rule);
    Result ToCalendarTimeWithMyRule(CalendarTime& calendar_time,
                                    CalendarAdditionalInfo& calendar_additional, s64 time);
    Result ParseBinary(const LocationName& name, std::span<const u8> binary);
    Result ParseBinaryInto(Tz::Rule& out_rule, std::span<const u8> binary);
    Result ToPosixTime(u32& out_count, std::span<s64> out_times, size_t out_times_max_count,
                       const CalendarTime& calendar, const Tz::Rule& rule);
    Result ToPosixTimeWithMyRule(u32& out_count, std::span<s64> out_times,
                                 size_t out_times_max_count, const CalendarTime& calendar);

private:
    Result ParseBinaryImpl(Tz::Rule& out_rule, std::span<const u8> binary);
    Result ToCalendarTimeImpl(CalendarTime& out_calendar_time,
                              CalendarAdditionalInfo& out_additional_info, s64 time,
                              const Tz::Rule& rule);
    Result ToPosixTimeImpl(u32& out_count, std::span<s64> out_times, size_t out_times_max_count,
                           const CalendarTime& calendar, const Tz::Rule& rule, s32 is_dst);

    bool m_initialized{};
    std::recursive_mutex m_mutex;
    LocationName m_location{};
    Tz::Rule m_my_rule{};
    SteadyClockTimePoint m_steady_clock_time_point{};
    u32 m_total_location_name_count{};
    RuleVersion m_rule_version{};
};

} // namespace Service::PSC::Time
