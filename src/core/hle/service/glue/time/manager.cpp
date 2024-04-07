// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <chrono>

#include "core/core.h"
#include "core/core_timing.h"

#include "common/settings.h"
#include "common/time_zone.h"
#include "core/file_sys/vfs/vfs.h"
#include "core/hle/kernel/svc.h"
#include "core/hle/service/glue/time/manager.h"
#include "core/hle/service/psc/time/service_manager.h"
#include "core/hle/service/psc/time/static.h"
#include "core/hle/service/psc/time/system_clock.h"
#include "core/hle/service/psc/time/time_zone_service.h"
#include "core/hle/service/set/system_settings_server.h"
#include "core/hle/service/sm/sm.h"

namespace Service::Glue::Time {

static s64 CalendarTimeToEpoch(Service::PSC::Time::CalendarTime calendar) {
    constexpr auto is_leap = [](s32 year) -> bool {
        return (((year) % 4) == 0 && (((year) % 100) != 0 || ((year) % 400) == 0));
    };
    constexpr std::array<s32, 12> MonthStartDayOfYear{
        0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334,
    };

    s16 month_s16{calendar.month};
    s8 month{static_cast<s8>(((month_s16 * 43) & ~std::numeric_limits<s16>::max()) +
                             ((month_s16 * 43) >> 9))};
    s8 month_index{static_cast<s8>(calendar.month - 12 * month)};
    if (month_index == 0) {
        month_index = 12;
    }
    s32 year{(month + calendar.year) - !month_index};
    s32 v8{year >= 0 ? year : year + 3};

    s64 days_since_epoch = calendar.day + MonthStartDayOfYear[month_index - 1];
    days_since_epoch += (year * 365) + (v8 / 4) - (year / 100) + (year / 400) - 365;

    if (month_index <= 2 && is_leap(year)) {
        days_since_epoch--;
    }
    auto epoch_s{((24ll * days_since_epoch + calendar.hour) * 60ll + calendar.minute) * 60ll +
                 calendar.second};
    return epoch_s - 62135683200ll;
}

static s64 GetEpochTimeFromInitialYear(
    std::shared_ptr<Service::Set::ISystemSettingsServer>& set_sys) {
    s32 year{2000};
    set_sys->GetSettingsItemValueImpl(year, "time", "standard_user_clock_initial_year");

    Service::PSC::Time::CalendarTime calendar{
        .year = static_cast<s16>(year),
        .month = 1,
        .day = 1,
        .hour = 0,
        .minute = 0,
        .second = 0,
    };
    return CalendarTimeToEpoch(calendar);
}

static Service::PSC::Time::LocationName GetTimeZoneString(
    TimeZoneBinary& time_zone_binary, Service::PSC::Time::LocationName& in_name) {
    auto configured_zone = Settings::GetTimeZoneString(Settings::values.time_zone_index.GetValue());

    Service::PSC::Time::LocationName configured_name{};
    std::memcpy(configured_name.data(), configured_zone.data(),
                std::min(configured_name.size(), configured_zone.size()));

    if (!time_zone_binary.IsValid(configured_name)) {
        configured_zone = Common::TimeZone::FindSystemTimeZone();
        configured_name = {};
        std::memcpy(configured_name.data(), configured_zone.data(),
                    std::min(configured_name.size(), configured_zone.size()));
    }

    ASSERT_MSG(time_zone_binary.IsValid(configured_name), "Invalid time zone {}!",
               configured_name.data());

    return configured_name;
}

TimeManager::TimeManager(Core::System& system)
    : m_steady_clock_resource{system}, m_time_zone_binary{system},
      m_worker{system, m_steady_clock_resource, m_file_timestamp_worker} {
    m_time_m =
        system.ServiceManager().GetService<Service::PSC::Time::ServiceManager>("time:m", true);

    auto res = m_time_m->GetStaticServiceAsServiceManager(&m_time_sm);
    ASSERT(res == ResultSuccess);

    m_set_sys =
        system.ServiceManager().GetService<Service::Set::ISystemSettingsServer>("set:sys", true);

    res = m_time_zone_binary.Mount();
    ASSERT(res == ResultSuccess);

    m_worker.Initialize(m_time_sm, m_set_sys);

    res = m_time_sm->GetStandardUserSystemClock(&m_file_timestamp_worker.m_system_clock);
    ASSERT(res == ResultSuccess);

    res = m_time_sm->GetTimeZoneService(&m_file_timestamp_worker.m_time_zone);
    ASSERT(res == ResultSuccess);

    res = SetupStandardSteadyClockCore();
    ASSERT(res == ResultSuccess);

    Service::PSC::Time::SystemClockContext user_clock_context{};
    res = m_set_sys->GetUserSystemClockContext(&user_clock_context);
    ASSERT(res == ResultSuccess);

    // TODO the local clock should initialise with this epoch time, and be updated somewhere else on
    // first boot to update it, but I haven't been able to find that point (likely via ntc's auto
    // correct as it's defaulted to be enabled). So to get a time that isn't stuck in the past for
    // first boot, grab the current real seconds.
    auto epoch_time{GetEpochTimeFromInitialYear(m_set_sys)};
    if (user_clock_context == Service::PSC::Time::SystemClockContext{}) {
        m_steady_clock_resource.GetRtcTimeInSeconds(epoch_time);
    }

    res = m_time_m->SetupStandardLocalSystemClockCore(user_clock_context, epoch_time);
    ASSERT(res == ResultSuccess);

    Service::PSC::Time::SystemClockContext network_clock_context{};
    res = m_set_sys->GetNetworkSystemClockContext(&network_clock_context);
    ASSERT(res == ResultSuccess);

    s32 network_accuracy_m{};
    m_set_sys->GetSettingsItemValueImpl<s32>(network_accuracy_m, "time",
                                             "standard_network_clock_sufficient_accuracy_minutes");
    auto one_minute_ns{
        std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::minutes(1)).count()};
    s64 network_accuracy_ns{network_accuracy_m * one_minute_ns};

    res = m_time_m->SetupStandardNetworkSystemClockCore(network_clock_context, network_accuracy_ns);
    ASSERT(res == ResultSuccess);

    bool is_automatic_correction_enabled{};
    res = m_set_sys->IsUserSystemClockAutomaticCorrectionEnabled(&is_automatic_correction_enabled);
    ASSERT(res == ResultSuccess);

    Service::PSC::Time::SteadyClockTimePoint automatic_correction_time_point{};
    res = m_set_sys->GetUserSystemClockAutomaticCorrectionUpdatedTime(
        &automatic_correction_time_point);
    ASSERT(res == ResultSuccess);

    res = m_time_m->SetupStandardUserSystemClockCore(is_automatic_correction_enabled,
                                                     automatic_correction_time_point);
    ASSERT(res == ResultSuccess);

    res = m_time_m->SetupEphemeralNetworkSystemClockCore();
    ASSERT(res == ResultSuccess);

    res = SetupTimeZoneServiceCore();
    ASSERT(res == ResultSuccess);

    s64 rtc_time_s{};
    res = m_steady_clock_resource.GetRtcTimeInSeconds(rtc_time_s);
    ASSERT(res == ResultSuccess);

    // TODO system report "launch"
    //      "rtc_reset" = m_steady_clock_resource.m_rtc_reset
    //      "rtc_value" = rtc_time_s

    m_worker.StartThread();

    m_file_timestamp_worker.m_initialized = true;

    s64 system_clock_time{};
    if (m_file_timestamp_worker.m_system_clock->GetCurrentTime(&system_clock_time) ==
        ResultSuccess) {
        Service::PSC::Time::CalendarTime calendar_time{};
        Service::PSC::Time::CalendarAdditionalInfo calendar_additional{};
        if (m_file_timestamp_worker.m_time_zone->ToCalendarTimeWithMyRule(
                &calendar_time, &calendar_additional, system_clock_time) == ResultSuccess) {
            // TODO IFileSystemProxy::SetCurrentPosixTime(system_clock_time,
            // calendar_additional.ut_offset)
        }
    }
}

Result TimeManager::SetupStandardSteadyClockCore() {
    Common::UUID external_clock_source_id{};
    auto res = m_set_sys->GetExternalSteadyClockSourceId(&external_clock_source_id);
    ASSERT(res == ResultSuccess);

    s64 external_steady_clock_internal_offset_s{};
    res = m_set_sys->GetExternalSteadyClockInternalOffset(&external_steady_clock_internal_offset_s);
    ASSERT(res == ResultSuccess);

    auto one_second_ns{
        std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::seconds(1)).count()};
    s64 external_steady_clock_internal_offset_ns{external_steady_clock_internal_offset_s *
                                                 one_second_ns};

    s32 standard_steady_clock_test_offset_m{};
    m_set_sys->GetSettingsItemValueImpl<s32>(standard_steady_clock_test_offset_m, "time",
                                             "standard_steady_clock_test_offset_minutes");
    auto one_minute_ns{
        std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::minutes(1)).count()};
    s64 standard_steady_clock_test_offset_ns{standard_steady_clock_test_offset_m * one_minute_ns};

    auto reset_detected = m_steady_clock_resource.GetResetDetected();
    if (reset_detected) {
        external_clock_source_id = {};
    }

    Common::UUID clock_source_id{};
    m_steady_clock_resource.Initialize(&clock_source_id, &external_clock_source_id);

    if (clock_source_id != external_clock_source_id) {
        m_set_sys->SetExternalSteadyClockSourceId(clock_source_id);
    }

    res = m_time_m->SetupStandardSteadyClockCore(
        reset_detected, clock_source_id, m_steady_clock_resource.GetTime(),
        external_steady_clock_internal_offset_ns, standard_steady_clock_test_offset_ns);
    ASSERT(res == ResultSuccess);
    R_SUCCEED();
}

Result TimeManager::SetupTimeZoneServiceCore() {
    Service::PSC::Time::LocationName name{};
    auto res = m_set_sys->GetDeviceTimeZoneLocationName(&name);
    ASSERT(res == ResultSuccess);

    auto configured_zone = GetTimeZoneString(m_time_zone_binary, name);

    if (configured_zone != name) {
        m_set_sys->SetDeviceTimeZoneLocationName(configured_zone);
        name = configured_zone;

        std::shared_ptr<Service::PSC::Time::SystemClock> local_clock;
        m_time_sm->GetStandardLocalSystemClock(&local_clock);

        Service::PSC::Time::SystemClockContext context{};
        local_clock->GetSystemClockContext(&context);
        m_set_sys->SetDeviceTimeZoneLocationUpdatedTime(context.steady_time_point);
    }

    Service::PSC::Time::SteadyClockTimePoint time_point{};
    res = m_set_sys->GetDeviceTimeZoneLocationUpdatedTime(&time_point);
    ASSERT(res == ResultSuccess);

    auto location_count = m_time_zone_binary.GetTimeZoneCount();
    Service::PSC::Time::RuleVersion rule_version{};
    m_time_zone_binary.GetTimeZoneVersion(rule_version);

    std::span<const u8> rule_buffer{};
    size_t rule_size{};
    res = m_time_zone_binary.GetTimeZoneRule(rule_buffer, rule_size, name);
    ASSERT(res == ResultSuccess);

    res = m_time_m->SetupTimeZoneServiceCore(name, rule_version, location_count, time_point,
                                             rule_buffer);
    ASSERT(res == ResultSuccess);

    R_SUCCEED();
}

} // namespace Service::Glue::Time
