// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>
#include <mutex>
#include <span>
#include <vector>

#include "core/hle/service/cmif_types.h"
#include "core/hle/service/ipc_helpers.h"
#include "core/hle/service/psc/time/common.h"
#include "core/hle/service/server_manager.h"
#include "core/hle/service/service.h"

namespace Core {
class System;
}

namespace Tz {
struct Rule;
}

namespace Service::Set {
class ISystemSettingsServer;
}

namespace Service::PSC::Time {
class TimeZoneService;
}

namespace Service::Glue::Time {
class FileTimestampWorker;
class TimeZoneBinary;

class TimeZoneService final : public ServiceFramework<TimeZoneService> {
    using InRule = InLargeData<Tz::Rule, BufferAttr_HipcMapAlias>;
    using OutRule = OutLargeData<Tz::Rule, BufferAttr_HipcMapAlias>;

public:
    explicit TimeZoneService(
        Core::System& system, FileTimestampWorker& file_timestamp_worker,
        bool can_write_timezone_device_location, TimeZoneBinary& time_zone_binary,
        std::shared_ptr<Service::PSC::Time::TimeZoneService> time_zone_service);

    ~TimeZoneService() override;

    Result GetDeviceLocationName(Out<Service::PSC::Time::LocationName> out_location_name);
    Result SetDeviceLocationName(const Service::PSC::Time::LocationName& location_name);
    Result GetTotalLocationNameCount(Out<u32> out_count);
    Result LoadLocationNameList(
        Out<u32> out_count,
        OutArray<Service::PSC::Time::LocationName, BufferAttr_HipcMapAlias> out_names, u32 index);
    Result LoadTimeZoneRule(OutRule out_rule,
                            const Service::PSC::Time::LocationName& location_name);
    Result GetTimeZoneRuleVersion(Out<Service::PSC::Time::RuleVersion> out_rule_version);
    Result GetDeviceLocationNameAndUpdatedTime(
        Out<Service::PSC::Time::LocationName> location_name,
        Out<Service::PSC::Time::SteadyClockTimePoint> out_time_point);
    Result SetDeviceLocationNameWithTimeZoneRule(
        const Service::PSC::Time::LocationName& location_name,
        InBuffer<BufferAttr_HipcAutoSelect> binary);
    Result ParseTimeZoneBinary(OutRule out_rule, InBuffer<BufferAttr_HipcAutoSelect> binary);
    Result GetDeviceLocationNameOperationEventReadableHandle(
        OutCopyHandle<Kernel::KReadableEvent> out_event);
    Result ToCalendarTime(Out<Service::PSC::Time::CalendarTime> out_calendar_time,
                          Out<Service::PSC::Time::CalendarAdditionalInfo> out_additional_info,
                          s64 time, InRule rule);
    Result ToCalendarTimeWithMyRule(
        Out<Service::PSC::Time::CalendarTime> out_calendar_time,
        Out<Service::PSC::Time::CalendarAdditionalInfo> out_additional_info, s64 time);
    Result ToPosixTime(Out<u32> out_count, OutArray<s64, BufferAttr_HipcPointer> out_times,
                       const Service::PSC::Time::CalendarTime& calendar_time, InRule rule);
    Result ToPosixTimeWithMyRule(Out<u32> out_count,
                                 OutArray<s64, BufferAttr_HipcPointer> out_times,
                                 const Service::PSC::Time::CalendarTime& calendar_time);

private:
    Core::System& m_system;
    std::shared_ptr<Service::Set::ISystemSettingsServer> m_set_sys;

    bool m_can_write_timezone_device_location;
    FileTimestampWorker& m_file_timestamp_worker;
    std::shared_ptr<Service::PSC::Time::TimeZoneService> m_wrapped_service;
    std::mutex m_mutex;
    bool operation_event_initialized{};
    Service::PSC::Time::OperationEvent m_operation_event;
    TimeZoneBinary& m_time_zone_binary;

    std::mutex m_list_mutex;
    Common::IntrusiveListBaseTraits<Service::PSC::Time::OperationEvent>::ListType m_list_nodes{};
};

} // namespace Service::Glue::Time
