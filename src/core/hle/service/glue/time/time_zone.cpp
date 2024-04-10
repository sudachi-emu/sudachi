// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <chrono>

#include "common/scope_exit.h"
#include "core/core.h"
#include "core/hle/kernel/svc.h"
#include "core/hle/service/cmif_serialization.h"
#include "core/hle/service/glue/time/file_timestamp_worker.h"
#include "core/hle/service/glue/time/time_zone.h"
#include "core/hle/service/glue/time/time_zone_binary.h"
#include "core/hle/service/psc/time/time_zone_service.h"
#include "core/hle/service/set/system_settings_server.h"
#include "core/hle/service/sm/sm.h"

namespace Service::Glue::Time {

TimeZoneService::TimeZoneService(
    Core::System& system_, FileTimestampWorker& file_timestamp_worker,
    bool can_write_timezone_device_location, TimeZoneBinary& time_zone_binary,
    std::shared_ptr<Service::PSC::Time::TimeZoneService> time_zone_service)
    : ServiceFramework{system_, "ITimeZoneService"}, m_system{system},
      m_can_write_timezone_device_location{can_write_timezone_device_location},
      m_file_timestamp_worker{file_timestamp_worker},
      m_wrapped_service{std::move(time_zone_service)}, m_operation_event{m_system},
      m_time_zone_binary{time_zone_binary} {
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

    m_set_sys =
        m_system.ServiceManager().GetService<Service::Set::ISystemSettingsServer>("set:sys", true);
}

TimeZoneService::~TimeZoneService() = default;

Result TimeZoneService::GetDeviceLocationName(
    Out<Service::PSC::Time::LocationName> out_location_name) {
    SCOPE_EXIT {
        LOG_DEBUG(Service_Time, "called. out_location_name={}", *out_location_name);
    };

    R_RETURN(m_wrapped_service->GetDeviceLocationName(out_location_name));
}

Result TimeZoneService::SetDeviceLocationName(
    const Service::PSC::Time::LocationName& location_name) {
    LOG_DEBUG(Service_Time, "called. location_name={}", location_name);

    R_UNLESS(m_can_write_timezone_device_location, Service::PSC::Time::ResultPermissionDenied);
    R_UNLESS(m_time_zone_binary.IsValid(location_name), Service::PSC::Time::ResultTimeZoneNotFound);

    std::scoped_lock l{m_mutex};

    std::span<const u8> binary{};
    size_t binary_size{};
    R_TRY(m_time_zone_binary.GetTimeZoneRule(binary, binary_size, location_name))

    R_TRY(m_wrapped_service->SetDeviceLocationNameWithTimeZoneRule(location_name, binary));

    m_file_timestamp_worker.SetFilesystemPosixTime();

    Service::PSC::Time::SteadyClockTimePoint time_point{};
    Service::PSC::Time::LocationName name{};
    R_TRY(m_wrapped_service->GetDeviceLocationNameAndUpdatedTime(&name, &time_point));

    m_set_sys->SetDeviceTimeZoneLocationName(name);
    m_set_sys->SetDeviceTimeZoneLocationUpdatedTime(time_point);

    std::scoped_lock m{m_list_mutex};
    for (auto& operation_event : m_list_nodes) {
        operation_event.m_event->Signal();
    }
    R_SUCCEED();
}

Result TimeZoneService::GetTotalLocationNameCount(Out<u32> out_count) {
    SCOPE_EXIT {
        LOG_DEBUG(Service_Time, "called. out_count={}", *out_count);
    };

    R_RETURN(m_wrapped_service->GetTotalLocationNameCount(out_count));
}

Result TimeZoneService::LoadLocationNameList(
    Out<u32> out_count,
    OutArray<Service::PSC::Time::LocationName, BufferAttr_HipcMapAlias> out_names, u32 index) {
    SCOPE_EXIT {
        LOG_DEBUG(Service_Time, "called. index={} out_count={} out_names[0]={} out_names[1]={}",
                  index, *out_count, out_names[0], out_names[1]);
    };

    std::scoped_lock l{m_mutex};
    R_RETURN(
        m_time_zone_binary.GetTimeZoneLocationList(*out_count, out_names, out_names.size(), index));
}

Result TimeZoneService::LoadTimeZoneRule(OutRule out_rule,
                                         const Service::PSC::Time::LocationName& name) {
    LOG_DEBUG(Service_Time, "called. name={}", name);

    std::scoped_lock l{m_mutex};
    std::span<const u8> binary{};
    size_t binary_size{};
    R_TRY(m_time_zone_binary.GetTimeZoneRule(binary, binary_size, name))
    R_RETURN(m_wrapped_service->ParseTimeZoneBinary(out_rule, binary));
}

Result TimeZoneService::GetTimeZoneRuleVersion(
    Out<Service::PSC::Time::RuleVersion> out_rule_version) {
    SCOPE_EXIT {
        LOG_DEBUG(Service_Time, "called. out_rule_version={}", *out_rule_version);
    };

    R_RETURN(m_wrapped_service->GetTimeZoneRuleVersion(out_rule_version));
}

Result TimeZoneService::GetDeviceLocationNameAndUpdatedTime(
    Out<Service::PSC::Time::LocationName> location_name,
    Out<Service::PSC::Time::SteadyClockTimePoint> out_time_point) {
    SCOPE_EXIT {
        LOG_DEBUG(Service_Time, "called. location_name={} out_time_point={}", *location_name,
                  *out_time_point);
    };

    R_RETURN(m_wrapped_service->GetDeviceLocationNameAndUpdatedTime(location_name, out_time_point));
}

Result TimeZoneService::SetDeviceLocationNameWithTimeZoneRule(
    const Service::PSC::Time::LocationName& location_name,
    InBuffer<BufferAttr_HipcAutoSelect> binary) {
    LOG_DEBUG(Service_Time, "called. location_name={}", location_name);

    R_UNLESS(m_can_write_timezone_device_location, Service::PSC::Time::ResultPermissionDenied);
    R_RETURN(Service::PSC::Time::ResultNotImplemented);
}

Result TimeZoneService::ParseTimeZoneBinary(OutRule out_rule,
                                            InBuffer<BufferAttr_HipcAutoSelect> binary) {
    LOG_DEBUG(Service_Time, "called.");

    R_RETURN(Service::PSC::Time::ResultNotImplemented);
}

Result TimeZoneService::GetDeviceLocationNameOperationEventReadableHandle(
    OutCopyHandle<Kernel::KReadableEvent> out_event) {
    LOG_DEBUG(Service_Time, "called.");

    if (!operation_event_initialized) {
        operation_event_initialized = false;

        m_operation_event.m_ctx.CloseEvent(m_operation_event.m_event);
        m_operation_event.m_event =
            m_operation_event.m_ctx.CreateEvent("Psc:TimeZoneService:OperationEvent");
        operation_event_initialized = true;
        std::scoped_lock l{m_mutex};
        m_list_nodes.push_back(m_operation_event);
    }

    *out_event = &m_operation_event.m_event->GetReadableEvent();
    R_SUCCEED();
}

Result TimeZoneService::ToCalendarTime(
    Out<Service::PSC::Time::CalendarTime> out_calendar_time,
    Out<Service::PSC::Time::CalendarAdditionalInfo> out_additional_info, s64 time, InRule rule) {
    SCOPE_EXIT {
        LOG_DEBUG(Service_Time, "called. time={} out_calendar_time={} out_additional_info={}", time,
                  *out_calendar_time, *out_additional_info);
    };

    R_RETURN(m_wrapped_service->ToCalendarTime(out_calendar_time, out_additional_info, time, rule));
}

Result TimeZoneService::ToCalendarTimeWithMyRule(
    Out<Service::PSC::Time::CalendarTime> out_calendar_time,
    Out<Service::PSC::Time::CalendarAdditionalInfo> out_additional_info, s64 time) {
    SCOPE_EXIT {
        LOG_DEBUG(Service_Time, "called. time={} out_calendar_time={} out_additional_info={}", time,
                  *out_calendar_time, *out_additional_info);
    };

    R_RETURN(
        m_wrapped_service->ToCalendarTimeWithMyRule(out_calendar_time, out_additional_info, time));
}

Result TimeZoneService::ToPosixTime(Out<u32> out_count,
                                    OutArray<s64, BufferAttr_HipcPointer> out_times,
                                    const Service::PSC::Time::CalendarTime& calendar_time,
                                    InRule rule) {
    SCOPE_EXIT {
        LOG_DEBUG(Service_Time,
                  "called. calendar_time={} out_count={} out_times[0]={} out_times[1]={}",
                  calendar_time, *out_count, out_times[0], out_times[1]);
    };

    R_RETURN(m_wrapped_service->ToPosixTime(out_count, out_times, calendar_time, rule));
}

Result TimeZoneService::ToPosixTimeWithMyRule(
    Out<u32> out_count, OutArray<s64, BufferAttr_HipcPointer> out_times,
    const Service::PSC::Time::CalendarTime& calendar_time) {
    SCOPE_EXIT {
        LOG_DEBUG(Service_Time,
                  "called. calendar_time={} out_count={} out_times[0]={} out_times[1]={}",
                  calendar_time, *out_count, out_times[0], out_times[1]);
    };

    R_RETURN(m_wrapped_service->ToPosixTimeWithMyRule(out_count, out_times, calendar_time));
}

} // namespace Service::Glue::Time
