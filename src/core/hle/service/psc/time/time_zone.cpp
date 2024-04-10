// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/hle/service/psc/time/time_zone.h"

namespace Service::PSC::Time {
namespace {
constexpr Result ValidateRule(const Tz::Rule& rule) {
    if (rule.typecnt > static_cast<s32>(Tz::TZ_MAX_TYPES) ||
        rule.timecnt > static_cast<s32>(Tz::TZ_MAX_TIMES) ||
        rule.charcnt > static_cast<s32>(Tz::TZ_MAX_CHARS)) {
        R_RETURN(ResultTimeZoneOutOfRange);
    }

    for (s32 i = 0; i < rule.timecnt; i++) {
        if (rule.types[i] >= rule.typecnt) {
            R_RETURN(ResultTimeZoneOutOfRange);
        }
    }

    for (s32 i = 0; i < rule.typecnt; i++) {
        if (rule.ttis[i].tt_desigidx >= static_cast<s32>(rule.chars.size())) {
            R_RETURN(ResultTimeZoneOutOfRange);
        }
    }
    R_SUCCEED();
}

constexpr bool GetTimeZoneTime(s64& out_time, const Tz::Rule& rule, s64 time, s32 index,
                               s32 index_offset) {
    s32 found_idx{};
    s32 expected_index{index + index_offset};
    s64 time_to_find{time + rule.ttis[rule.types[index]].tt_utoff -
                     rule.ttis[rule.types[expected_index]].tt_utoff};

    if (rule.timecnt > 1 && rule.ats[0] <= time_to_find) {
        s32 low{1};
        s32 high{rule.timecnt};

        while (low < high) {
            auto mid{(low + high) / 2};
            if (rule.ats[mid] <= time_to_find) {
                low = mid + 1;
            } else if (rule.ats[mid] > time_to_find) {
                high = mid;
            }
        }
        found_idx = low - 1;
    }

    if (found_idx == expected_index) {
        out_time = time_to_find;
    }
    return found_idx == expected_index;
}
} // namespace

void TimeZone::SetTimePoint(const SteadyClockTimePoint& time_point) {
    std::scoped_lock l{m_mutex};
    m_steady_clock_time_point = time_point;
}

void TimeZone::SetTotalLocationNameCount(u32 count) {
    std::scoped_lock l{m_mutex};
    m_total_location_name_count = count;
}

void TimeZone::SetRuleVersion(const RuleVersion& rule_version) {
    std::scoped_lock l{m_mutex};
    m_rule_version = rule_version;
}

Result TimeZone::GetLocationName(LocationName& out_name) {
    std::scoped_lock l{m_mutex};
    R_UNLESS(m_initialized, ResultClockUninitialized);
    out_name = m_location;
    R_SUCCEED();
}

Result TimeZone::GetTotalLocationCount(u32& out_count) {
    std::scoped_lock l{m_mutex};
    if (!m_initialized) {
        return ResultClockUninitialized;
    }

    out_count = m_total_location_name_count;
    R_SUCCEED();
}

Result TimeZone::GetRuleVersion(RuleVersion& out_rule_version) {
    std::scoped_lock l{m_mutex};
    if (!m_initialized) {
        return ResultClockUninitialized;
    }
    out_rule_version = m_rule_version;
    R_SUCCEED();
}

Result TimeZone::GetTimePoint(SteadyClockTimePoint& out_time_point) {
    std::scoped_lock l{m_mutex};
    if (!m_initialized) {
        return ResultClockUninitialized;
    }
    out_time_point = m_steady_clock_time_point;
    R_SUCCEED();
}

Result TimeZone::ToCalendarTime(CalendarTime& out_calendar_time,
                                CalendarAdditionalInfo& out_additional_info, s64 time,
                                const Tz::Rule& rule) {
    std::scoped_lock l{m_mutex};
    R_RETURN(ToCalendarTimeImpl(out_calendar_time, out_additional_info, time, rule));
}

Result TimeZone::ToCalendarTimeWithMyRule(CalendarTime& calendar_time,
                                          CalendarAdditionalInfo& calendar_additional, s64 time) {
    // This is checked outside the mutex. Bug?
    if (!m_initialized) {
        return ResultClockUninitialized;
    }

    std::scoped_lock l{m_mutex};
    R_RETURN(ToCalendarTimeImpl(calendar_time, calendar_additional, time, m_my_rule));
}

Result TimeZone::ParseBinary(const LocationName& name, std::span<const u8> binary) {
    std::scoped_lock l{m_mutex};

    Tz::Rule tmp_rule{};
    R_TRY(ParseBinaryImpl(tmp_rule, binary));

    m_my_rule = tmp_rule;
    m_location = name;

    R_SUCCEED();
}

Result TimeZone::ParseBinaryInto(Tz::Rule& out_rule, std::span<const u8> binary) {
    std::scoped_lock l{m_mutex};
    R_RETURN(ParseBinaryImpl(out_rule, binary));
}

Result TimeZone::ToPosixTime(u32& out_count, std::span<s64> out_times, size_t out_times_max_count,
                             const CalendarTime& calendar, const Tz::Rule& rule) {
    std::scoped_lock l{m_mutex};

    auto res = ToPosixTimeImpl(out_count, out_times, out_times_max_count, calendar, rule, -1);

    if (res != ResultSuccess) {
        if (res == ResultTimeZoneNotFound) {
            res = ResultSuccess;
            out_count = 0;
        }
    } else if (out_count == 2 && out_times[0] > out_times[1]) {
        std::swap(out_times[0], out_times[1]);
    }
    R_RETURN(res);
}

Result TimeZone::ToPosixTimeWithMyRule(u32& out_count, std::span<s64> out_times,
                                       size_t out_times_max_count, const CalendarTime& calendar) {
    std::scoped_lock l{m_mutex};

    auto res = ToPosixTimeImpl(out_count, out_times, out_times_max_count, calendar, m_my_rule, -1);

    if (res != ResultSuccess) {
        if (res == ResultTimeZoneNotFound) {
            res = ResultSuccess;
            out_count = 0;
        }
    } else if (out_count == 2 && out_times[0] > out_times[1]) {
        std::swap(out_times[0], out_times[1]);
    }
    R_RETURN(res);
}

Result TimeZone::ParseBinaryImpl(Tz::Rule& out_rule, std::span<const u8> binary) {
    if (Tz::ParseTimeZoneBinary(out_rule, binary)) {
        R_RETURN(ResultTimeZoneParseFailed);
    }
    R_SUCCEED();
}

Result TimeZone::ToCalendarTimeImpl(CalendarTime& out_calendar_time,
                                    CalendarAdditionalInfo& out_additional_info, s64 time,
                                    const Tz::Rule& rule) {
    R_TRY(ValidateRule(rule));

    Tz::CalendarTimeInternal calendar_internal{};
    time_t time_tmp{static_cast<time_t>(time)};
    if (Tz::localtime_rz(&calendar_internal, &rule, &time_tmp)) {
        R_RETURN(ResultOverflow);
    }

    out_calendar_time.year = static_cast<s16>(calendar_internal.tm_year + 1900);
    out_calendar_time.month = static_cast<s8>(calendar_internal.tm_mon + 1);
    out_calendar_time.day = static_cast<s8>(calendar_internal.tm_mday);
    out_calendar_time.hour = static_cast<s8>(calendar_internal.tm_hour);
    out_calendar_time.minute = static_cast<s8>(calendar_internal.tm_min);
    out_calendar_time.second = static_cast<s8>(calendar_internal.tm_sec);

    out_additional_info.day_of_week = calendar_internal.tm_wday;
    out_additional_info.day_of_year = calendar_internal.tm_yday;

    std::memcpy(out_additional_info.name.data(), calendar_internal.tm_zone.data(),
                out_additional_info.name.size());
    out_additional_info.name[out_additional_info.name.size() - 1] = '\0';

    out_additional_info.is_dst = calendar_internal.tm_isdst;
    out_additional_info.ut_offset = calendar_internal.tm_utoff;

    R_SUCCEED();
}

Result TimeZone::ToPosixTimeImpl(u32& out_count, std::span<s64> out_times,
                                 size_t out_times_max_count, const CalendarTime& calendar,
                                 const Tz::Rule& rule, s32 is_dst) {
    R_TRY(ValidateRule(rule));

    CalendarTime local_calendar{calendar};

    local_calendar.month -= 1;
    local_calendar.year -= 1900;

    Tz::CalendarTimeInternal internal{
        .tm_sec = local_calendar.second,
        .tm_min = local_calendar.minute,
        .tm_hour = local_calendar.hour,
        .tm_mday = local_calendar.day,
        .tm_mon = local_calendar.month,
        .tm_year = local_calendar.year,
        .tm_wday = 0,
        .tm_yday = 0,
        .tm_isdst = is_dst,
        .tm_zone = {},
        .tm_utoff = 0,
        .time_index = 0,
    };
    time_t time_tmp{};
    auto res = Tz::mktime_tzname(&time_tmp, &rule, &internal);
    s64 time = static_cast<s64>(time_tmp);

    if (res == 1) {
        R_RETURN(ResultOverflow);
    } else if (res == 2) {
        R_RETURN(ResultTimeZoneNotFound);
    }

    if (internal.tm_sec != local_calendar.second || internal.tm_min != local_calendar.minute ||
        internal.tm_hour != local_calendar.hour || internal.tm_mday != local_calendar.day ||
        internal.tm_mon != local_calendar.month || internal.tm_year != local_calendar.year) {
        R_RETURN(ResultTimeZoneNotFound);
    }

    if (res != 0) {
        ASSERT(false);
    }

    out_times[0] = time;
    if (out_times_max_count < 2) {
        out_count = 1;
        R_SUCCEED();
    }

    s64 time2{};
    if (internal.time_index > 0 && GetTimeZoneTime(time2, rule, time, internal.time_index, -1)) {
        out_times[1] = time2;
        out_count = 2;
        R_SUCCEED();
    }

    if (((internal.time_index + 1) < rule.timecnt) &&
        GetTimeZoneTime(time2, rule, time, internal.time_index, 1)) {
        out_times[1] = time2;
        out_count = 2;
        R_SUCCEED();
    }

    out_count = 1;
    R_SUCCEED();
}

} // namespace Service::PSC::Time
