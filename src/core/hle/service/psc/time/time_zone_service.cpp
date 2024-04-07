// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <tz/tz.h>

#include "common/scope_exit.h"
#include "core/core.h"
#include "core/hle/service/cmif_serialization.h"
#include "core/hle/service/psc/time/time_zone_service.h"

namespace Service::PSC::Time {

TimeZoneService::TimeZoneService(Core::System& system_, StandardSteadyClockCore& clock_core,
                                 TimeZone& time_zone, bool can_write_timezone_device_location)
    : ServiceFramework{system_, "ITimeZoneService"}, m_system{system}, m_clock_core{clock_core},
      m_time_zone{time_zone},
      m_can_write_timezone_device_location{can_write_timezone_device_location} {
    // clang-format off
    static const FunctionInfo functions[] = {
        {0,   D<&TimeZoneService::GetDeviceLocationName>, "GetDeviceLocationName"},
        {1,   D<&TimeZoneService::SetDeviceLocationName>, "SetDeviceLocationName"},
        {2,   D<&TimeZoneService::GetTotalLocationNameCount>, "GetTotalLocationNameCount"},
        {3,   D<&TimeZoneService::LoadLocationNameList>, "LoadLocationNameList"},
        {4,   D<&TimeZoneService::LoadTimeZoneRule>, "LoadTimeZoneRule"},
        {5,   D<&TimeZoneService::GetTimeZoneRuleVersion>, "GetTimeZoneRuleVersion"},
        {6,   D<&TimeZoneService::GetDeviceLocationNameAndUpdatedTime>, "GetDeviceLocationNameAndUpdatedTime"},
        {7,   D<&TimeZoneService::SetDeviceLocationNameWithTimeZoneRule>, "SetDeviceLocationNameWithTimeZoneRule"},
        {8,   D<&TimeZoneService::ParseTimeZoneBinary>, "ParseTimeZoneBinary"},
        {20,  D<&TimeZoneService::GetDeviceLocationNameOperationEventReadableHandle>, "GetDeviceLocationNameOperationEventReadableHandle"},
        {100, D<&TimeZoneService::ToCalendarTime>, "ToCalendarTime"},
        {101, D<&TimeZoneService::ToCalendarTimeWithMyRule>, "ToCalendarTimeWithMyRule"},
        {201, D<&TimeZoneService::ToPosixTime>, "ToPosixTime"},
        {202, D<&TimeZoneService::ToPosixTimeWithMyRule>, "ToPosixTimeWithMyRule"},
    };
    // clang-format on
    RegisterHandlers(functions);
}

Result TimeZoneService::GetDeviceLocationName(Out<LocationName> out_location_name) {
    SCOPE_EXIT {
        LOG_DEBUG(Service_Time, "called. out_location_name={}", *out_location_name);
    };

    R_RETURN(m_time_zone.GetLocationName(*out_location_name));
}

Result TimeZoneService::SetDeviceLocationName(const LocationName& location_name) {
    LOG_DEBUG(Service_Time, "called. This function is not implemented!");

    R_UNLESS(m_can_write_timezone_device_location, ResultPermissionDenied);
    R_RETURN(ResultNotImplemented);
}

Result TimeZoneService::GetTotalLocationNameCount(Out<u32> out_count) {
    SCOPE_EXIT {
        LOG_DEBUG(Service_Time, "called. out_count={}", *out_count);
    };

    R_RETURN(m_time_zone.GetTotalLocationCount(*out_count));
}

Result TimeZoneService::LoadLocationNameList(
    Out<u32> out_count, OutArray<LocationName, BufferAttr_HipcMapAlias> out_names, u32 index) {
    LOG_DEBUG(Service_Time, "called. This function is not implemented!");

    R_RETURN(ResultNotImplemented);
}

Result TimeZoneService::LoadTimeZoneRule(OutRule out_rule, const LocationName& location_name) {
    LOG_DEBUG(Service_Time, "called. This function is not implemented!");

    R_RETURN(ResultNotImplemented);
}

Result TimeZoneService::GetTimeZoneRuleVersion(Out<RuleVersion> out_rule_version) {
    SCOPE_EXIT {
        LOG_DEBUG(Service_Time, "called. out_rule_version={}", *out_rule_version);
    };

    R_RETURN(m_time_zone.GetRuleVersion(*out_rule_version));
}

Result TimeZoneService::GetDeviceLocationNameAndUpdatedTime(
    Out<LocationName> out_location_name, Out<SteadyClockTimePoint> out_time_point) {
    SCOPE_EXIT {
        LOG_DEBUG(Service_Time, "called. out_location_name={} out_time_point={}",
                  *out_location_name, *out_time_point);
    };

    R_TRY(m_time_zone.GetLocationName(*out_location_name));
    R_RETURN(m_time_zone.GetTimePoint(*out_time_point));
}

Result TimeZoneService::SetDeviceLocationNameWithTimeZoneRule(
    const LocationName& location_name, InBuffer<BufferAttr_HipcAutoSelect> binary) {
    LOG_DEBUG(Service_Time, "called. location_name={}", location_name);

    R_UNLESS(m_can_write_timezone_device_location, ResultPermissionDenied);
    R_TRY(m_time_zone.ParseBinary(location_name, binary));

    SteadyClockTimePoint time_point{};
    R_TRY(m_clock_core.GetCurrentTimePoint(time_point));

    m_time_zone.SetTimePoint(time_point);
    R_SUCCEED();
}

Result TimeZoneService::ParseTimeZoneBinary(OutRule out_rule,
                                            InBuffer<BufferAttr_HipcAutoSelect> binary) {
    LOG_DEBUG(Service_Time, "called.");

    R_RETURN(m_time_zone.ParseBinaryInto(*out_rule, binary));
}

Result TimeZoneService::GetDeviceLocationNameOperationEventReadableHandle(
    OutCopyHandle<Kernel::KReadableEvent> out_event) {
    LOG_DEBUG(Service_Time, "called. This function is not implemented!");

    R_RETURN(ResultNotImplemented);
}

Result TimeZoneService::ToCalendarTime(Out<CalendarTime> out_calendar_time,
                                       Out<CalendarAdditionalInfo> out_additional_info, s64 time,
                                       InRule rule) {
    SCOPE_EXIT {
        LOG_DEBUG(Service_Time, "called. time={} out_calendar_time={} out_additional_info={}", time,
                  *out_calendar_time, *out_additional_info);
    };

    R_RETURN(
        m_time_zone.ToCalendarTime(*out_calendar_time, *out_additional_info, time, *rule.Get()));
}

Result TimeZoneService::ToCalendarTimeWithMyRule(Out<CalendarTime> out_calendar_time,
                                                 Out<CalendarAdditionalInfo> out_additional_info,
                                                 s64 time) {
    SCOPE_EXIT {
        LOG_DEBUG(Service_Time, "called. time={} out_calendar_time={} out_additional_info={}", time,
                  *out_calendar_time, *out_additional_info);
    };

    R_RETURN(m_time_zone.ToCalendarTimeWithMyRule(*out_calendar_time, *out_additional_info, time));
}

Result TimeZoneService::ToPosixTime(Out<u32> out_count,
                                    OutArray<s64, BufferAttr_HipcPointer> out_times,
                                    const CalendarTime& calendar_time, InRule rule) {
    SCOPE_EXIT {
        LOG_DEBUG(Service_Time,
                  "called. calendar_time={} out_count={} out_times[0]={} out_times[1]={} ",
                  calendar_time, *out_count, out_times[0], out_times[1]);
    };

    R_RETURN(
        m_time_zone.ToPosixTime(*out_count, out_times, out_times.size(), calendar_time, *rule));
}

Result TimeZoneService::ToPosixTimeWithMyRule(Out<u32> out_count,
                                              OutArray<s64, BufferAttr_HipcPointer> out_times,
                                              const CalendarTime& calendar_time) {
    SCOPE_EXIT {
        LOG_DEBUG(Service_Time,
                  "called. calendar_time={} out_count={} out_times[0]={} out_times[1]={} ",
                  calendar_time, *out_count, out_times[0], out_times[1]);
    };

    R_RETURN(
        m_time_zone.ToPosixTimeWithMyRule(*out_count, out_times, out_times.size(), calendar_time));
}

} // namespace Service::PSC::Time
