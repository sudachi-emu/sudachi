// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <chrono>
#include <fmt/format.h>

#include "common/common_types.h"
#include "common/intrusive_list.h"
#include "common/uuid.h"
#include "common/wall_clock.h"
#include "core/hle/kernel/k_event.h"
#include "core/hle/service/kernel_helpers.h"
#include "core/hle/service/psc/time/errors.h"

namespace Core {
class System;
}

namespace Service::PSC::Time {
using ClockSourceId = Common::UUID;

enum class TimeType : u8 {
    UserSystemClock = 0,
    NetworkSystemClock = 1,
    LocalSystemClock = 2,
};

struct SteadyClockTimePoint {
    constexpr bool IdMatches(const SteadyClockTimePoint& other) const {
        return clock_source_id == other.clock_source_id;
    }
    bool operator==(const SteadyClockTimePoint& other) const = default;

    s64 time_point;
    ClockSourceId clock_source_id;
};
static_assert(sizeof(SteadyClockTimePoint) == 0x18, "SteadyClockTimePoint has the wrong size!");
static_assert(std::is_trivial_v<ClockSourceId>);

struct SystemClockContext {
    bool operator==(const SystemClockContext& other) const = default;

    s64 offset;
    SteadyClockTimePoint steady_time_point;
};
static_assert(sizeof(SystemClockContext) == 0x20, "SystemClockContext has the wrong size!");
static_assert(std::is_trivial_v<SystemClockContext>);

struct CalendarTime {
    s16 year;
    s8 month;
    s8 day;
    s8 hour;
    s8 minute;
    s8 second;
};
static_assert(sizeof(CalendarTime) == 0x8, "CalendarTime has the wrong size!");

struct CalendarAdditionalInfo {
    s32 day_of_week;
    s32 day_of_year;
    std::array<char, 8> name;
    s32 is_dst;
    s32 ut_offset;
};
static_assert(sizeof(CalendarAdditionalInfo) == 0x18, "CalendarAdditionalInfo has the wrong size!");

using LocationName = std::array<char, 0x24>;
static_assert(sizeof(LocationName) == 0x24, "LocationName has the wrong size!");

using RuleVersion = std::array<char, 0x10>;
static_assert(sizeof(RuleVersion) == 0x10, "RuleVersion has the wrong size!");

struct ClockSnapshot {
    SystemClockContext user_context;
    SystemClockContext network_context;
    s64 user_time;
    s64 network_time;
    CalendarTime user_calendar_time;
    CalendarTime network_calendar_time;
    CalendarAdditionalInfo user_calendar_additional_time;
    CalendarAdditionalInfo network_calendar_additional_time;
    SteadyClockTimePoint steady_clock_time_point;
    LocationName location_name;
    bool is_automatic_correction_enabled;
    TimeType type;
    u16 unk_CE;
};
static_assert(sizeof(ClockSnapshot) == 0xD0, "ClockSnapshot has the wrong size!");
static_assert(std::is_trivial_v<ClockSnapshot>);

struct ContinuousAdjustmentTimePoint {
    s64 rtc_offset;
    s64 diff_scale;
    s64 shift_amount;
    s64 lower;
    s64 upper;
    ClockSourceId clock_source_id;
};
static_assert(sizeof(ContinuousAdjustmentTimePoint) == 0x38,
              "ContinuousAdjustmentTimePoint has the wrong size!");
static_assert(std::is_trivial_v<ContinuousAdjustmentTimePoint>);

struct AlarmInfo {
    s64 alert_time;
    u32 priority;
};
static_assert(sizeof(AlarmInfo) == 0x10, "AlarmInfo has the wrong size!");

struct StaticServiceSetupInfo {
    bool can_write_local_clock;
    bool can_write_user_clock;
    bool can_write_network_clock;
    bool can_write_timezone_device_location;
    bool can_write_steady_clock;
    bool can_write_uninitialized_clock;
};
static_assert(sizeof(StaticServiceSetupInfo) == 0x6, "StaticServiceSetupInfo has the wrong size!");

struct OperationEvent : public Common::IntrusiveListBaseNode<OperationEvent> {
    using OperationEventList = Common::IntrusiveListBaseTraits<OperationEvent>::ListType;

    OperationEvent(Core::System& system);
    ~OperationEvent();

    KernelHelpers::ServiceContext m_ctx;
    Kernel::KEvent* m_event{};
};

constexpr inline std::chrono::nanoseconds ConvertToTimeSpan(s64 ticks) {
    constexpr auto one_second_ns{
        std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::seconds(1)).count()};

    constexpr s64 max{Common::WallClock::CNTFRQ *
                      (std::numeric_limits<s64>::max() / one_second_ns)};

    if (ticks > max) {
        return std::chrono::nanoseconds(std::numeric_limits<s64>::max());
    } else if (ticks < -max) {
        return std::chrono::nanoseconds(std::numeric_limits<s64>::min());
    }

    auto a{ticks / Common::WallClock::CNTFRQ * one_second_ns};
    auto b{((ticks % Common::WallClock::CNTFRQ) * one_second_ns) / Common::WallClock::CNTFRQ};

    return std::chrono::nanoseconds(a + b);
}

constexpr inline Result GetSpanBetweenTimePoints(s64* out_seconds, const SteadyClockTimePoint& a,
                                                 const SteadyClockTimePoint& b) {
    R_UNLESS(out_seconds, ResultInvalidArgument);
    R_UNLESS(a.IdMatches(b), ResultInvalidArgument);
    R_UNLESS(a.time_point >= 0 || b.time_point <= a.time_point + std::numeric_limits<s64>::max(),
             ResultOverflow);
    R_UNLESS(a.time_point < 0 || b.time_point >= a.time_point + std::numeric_limits<s64>::min(),
             ResultOverflow);

    *out_seconds = b.time_point - a.time_point;
    R_SUCCEED();
}

} // namespace Service::PSC::Time

template <>
struct fmt::formatter<Service::PSC::Time::TimeType> : fmt::formatter<fmt::string_view> {
    template <typename FormatContext>
    auto format(Service::PSC::Time::TimeType type, FormatContext& ctx) {
        const string_view name = [type] {
            using Service::PSC::Time::TimeType;
            switch (type) {
            case TimeType::UserSystemClock:
                return "UserSystemClock";
            case TimeType::NetworkSystemClock:
                return "NetworkSystemClock";
            case TimeType::LocalSystemClock:
                return "LocalSystemClock";
            }
            return "Invalid";
        }();
        return formatter<string_view>::format(name, ctx);
    }
};

template <>
struct fmt::formatter<Service::PSC::Time::SteadyClockTimePoint> : fmt::formatter<fmt::string_view> {
    template <typename FormatContext>
    auto format(const Service::PSC::Time::SteadyClockTimePoint& time_point,
                FormatContext& ctx) const {
        return fmt::format_to(ctx.out(), "[time_point={}]", time_point.time_point);
    }
};

template <>
struct fmt::formatter<Service::PSC::Time::SystemClockContext> : fmt::formatter<fmt::string_view> {
    template <typename FormatContext>
    auto format(const Service::PSC::Time::SystemClockContext& context, FormatContext& ctx) const {
        return fmt::format_to(ctx.out(), "[offset={} steady_time_point={}]", context.offset,
                              context.steady_time_point.time_point);
    }
};

template <>
struct fmt::formatter<Service::PSC::Time::CalendarTime> : fmt::formatter<fmt::string_view> {
    template <typename FormatContext>
    auto format(const Service::PSC::Time::CalendarTime& calendar, FormatContext& ctx) const {
        return fmt::format_to(ctx.out(), "[{:02}/{:02}/{:04} {:02}:{:02}:{:02}]", calendar.day,
                              calendar.month, calendar.year, calendar.hour, calendar.minute,
                              calendar.second);
    }
};

template <>
struct fmt::formatter<Service::PSC::Time::CalendarAdditionalInfo>
    : fmt::formatter<fmt::string_view> {
    template <typename FormatContext>
    auto format(const Service::PSC::Time::CalendarAdditionalInfo& additional,
                FormatContext& ctx) const {
        return fmt::format_to(ctx.out(), "[weekday={} yearday={} name={} is_dst={} ut_offset={}]",
                              additional.day_of_week, additional.day_of_year,
                              additional.name.data(), additional.is_dst, additional.ut_offset);
    }
};

template <>
struct fmt::formatter<Service::PSC::Time::LocationName> : fmt::formatter<fmt::string_view> {
    template <typename FormatContext>
    auto format(const Service::PSC::Time::LocationName& name, FormatContext& ctx) const {
        return formatter<string_view>::format(name.data(), ctx);
    }
};

template <>
struct fmt::formatter<Service::PSC::Time::RuleVersion> : fmt::formatter<fmt::string_view> {
    template <typename FormatContext>
    auto format(const Service::PSC::Time::RuleVersion& version, FormatContext& ctx) const {
        return formatter<string_view>::format(version.data(), ctx);
    }
};

template <>
struct fmt::formatter<Service::PSC::Time::ClockSnapshot> : fmt::formatter<fmt::string_view> {
    template <typename FormatContext>
    auto format(const Service::PSC::Time::ClockSnapshot& snapshot, FormatContext& ctx) const {
        return fmt::format_to(
            ctx.out(),
            "[user_context={} network_context={} user_time={} network_time={} "
            "user_calendar_time={} "
            "network_calendar_time={} user_calendar_additional_time={} "
            "network_calendar_additional_time={} steady_clock_time_point={} location={} "
            "is_automatic_correction_enabled={} type={}]",
            snapshot.user_context, snapshot.network_context, snapshot.user_time,
            snapshot.network_time, snapshot.user_calendar_time, snapshot.network_calendar_time,
            snapshot.user_calendar_additional_time, snapshot.network_calendar_additional_time,
            snapshot.steady_clock_time_point, snapshot.location_name,
            snapshot.is_automatic_correction_enabled, snapshot.type);
    }
};

template <>
struct fmt::formatter<Service::PSC::Time::ContinuousAdjustmentTimePoint>
    : fmt::formatter<fmt::string_view> {
    template <typename FormatContext>
    auto format(const Service::PSC::Time::ContinuousAdjustmentTimePoint& time_point,
                FormatContext& ctx) const {
        return fmt::format_to(ctx.out(),
                              "[rtc_offset={} diff_scale={} shift_amount={} lower={} upper={}]",
                              time_point.rtc_offset, time_point.diff_scale, time_point.shift_amount,
                              time_point.lower, time_point.upper);
    }
};