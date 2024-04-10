// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "core/hle/service/cmif_types.h"
#include "core/hle/service/ipc_helpers.h"
#include "core/hle/service/psc/time/common.h"
#include "core/hle/service/psc/time/manager.h"
#include "core/hle/service/server_manager.h"
#include "core/hle/service/service.h"

namespace Core {
class System;
}

namespace Tz {
struct Rule;
}

namespace Service::PSC::Time {

class TimeZoneService final : public ServiceFramework<TimeZoneService> {
    using InRule = InLargeData<Tz::Rule, BufferAttr_HipcMapAlias>;
    using OutRule = OutLargeData<Tz::Rule, BufferAttr_HipcMapAlias>;

public:
    explicit TimeZoneService(Core::System& system, StandardSteadyClockCore& clock_core,
                             TimeZone& time_zone, bool can_write_timezone_device_location);

    ~TimeZoneService() override = default;

    Result GetDeviceLocationName(Out<LocationName> out_location_name);
    Result SetDeviceLocationName(const LocationName& location_name);
    Result GetTotalLocationNameCount(Out<u32> out_count);
    Result LoadLocationNameList(Out<u32> out_count,
                                OutArray<LocationName, BufferAttr_HipcMapAlias> out_names,
                                u32 index);
    Result LoadTimeZoneRule(OutRule out_rule, const LocationName& location_name);
    Result GetTimeZoneRuleVersion(Out<RuleVersion> out_rule_version);
    Result GetDeviceLocationNameAndUpdatedTime(Out<LocationName> location_name,
                                               Out<SteadyClockTimePoint> out_time_point);
    Result SetDeviceLocationNameWithTimeZoneRule(const LocationName& location_name,
                                                 InBuffer<BufferAttr_HipcAutoSelect> binary);
    Result ParseTimeZoneBinary(OutRule out_rule, InBuffer<BufferAttr_HipcAutoSelect> binary);
    Result GetDeviceLocationNameOperationEventReadableHandle(
        OutCopyHandle<Kernel::KReadableEvent> out_event);
    Result ToCalendarTime(Out<CalendarTime> out_calendar_time,
                          Out<CalendarAdditionalInfo> out_additional_info, s64 time, InRule rule);
    Result ToCalendarTimeWithMyRule(Out<CalendarTime> out_calendar_time,
                                    Out<CalendarAdditionalInfo> out_additional_info, s64 time);
    Result ToPosixTime(Out<u32> out_count, OutArray<s64, BufferAttr_HipcPointer> out_times,
                       const CalendarTime& calendar_time, InRule rule);
    Result ToPosixTimeWithMyRule(Out<u32> out_count,
                                 OutArray<s64, BufferAttr_HipcPointer> out_times,
                                 const CalendarTime& calendar_time);

private:
    Core::System& m_system;

    StandardSteadyClockCore& m_clock_core;
    TimeZone& m_time_zone;
    bool m_can_write_timezone_device_location;
};

} // namespace Service::PSC::Time
