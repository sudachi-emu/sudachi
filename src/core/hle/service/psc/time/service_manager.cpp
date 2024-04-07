// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/core.h"
#include "core/core_timing.h"
#include "core/hle/service/cmif_serialization.h"
#include "core/hle/service/psc/time/power_state_service.h"
#include "core/hle/service/psc/time/service_manager.h"
#include "core/hle/service/psc/time/static.h"

namespace Service::PSC::Time {

ServiceManager::ServiceManager(Core::System& system_, std::shared_ptr<TimeManager> time,
                               ServerManager* server_manager)
    : ServiceFramework{system_, "time:m"}, m_system{system}, m_time{std::move(time)},
      m_server_manager{*server_manager},
      m_local_system_clock{m_time->m_standard_local_system_clock},
      m_user_system_clock{m_time->m_standard_user_system_clock},
      m_network_system_clock{m_time->m_standard_network_system_clock},
      m_steady_clock{m_time->m_standard_steady_clock}, m_time_zone{m_time->m_time_zone},
      m_ephemeral_network_clock{m_time->m_ephemeral_network_clock},
      m_shared_memory{m_time->m_shared_memory}, m_alarms{m_time->m_alarms},
      m_local_system_context_writer{m_time->m_local_system_clock_context_writer},
      m_network_system_context_writer{m_time->m_network_system_clock_context_writer},
      m_ephemeral_system_context_writer{m_time->m_ephemeral_network_clock_context_writer},
      m_local_operation{m_system}, m_network_operation{m_system}, m_ephemeral_operation{m_system} {
    // clang-format off
    static const FunctionInfo functions[] = {
        {0,   D<&ServiceManager::GetStaticServiceAsUser>, "GetStaticServiceAsUser"},
        {5,   D<&ServiceManager::GetStaticServiceAsAdmin>, "GetStaticServiceAsAdmin"},
        {6,   D<&ServiceManager::GetStaticServiceAsRepair>, "GetStaticServiceAsRepair"},
        {9,   D<&ServiceManager::GetStaticServiceAsServiceManager>, "GetStaticServiceAsServiceManager"},
        {10,  D<&ServiceManager::SetupStandardSteadyClockCore>, "SetupStandardSteadyClockCore"},
        {11,  D<&ServiceManager::SetupStandardLocalSystemClockCore>, "SetupStandardLocalSystemClockCore"},
        {12,  D<&ServiceManager::SetupStandardNetworkSystemClockCore>, "SetupStandardNetworkSystemClockCore"},
        {13,  D<&ServiceManager::SetupStandardUserSystemClockCore>, "SetupStandardUserSystemClockCore"},
        {14,  D<&ServiceManager::SetupTimeZoneServiceCore>, "SetupTimeZoneServiceCore"},
        {15,  D<&ServiceManager::SetupEphemeralNetworkSystemClockCore>, "SetupEphemeralNetworkSystemClockCore"},
        {50,  D<&ServiceManager::GetStandardLocalClockOperationEvent>, "GetStandardLocalClockOperationEvent"},
        {51,  D<&ServiceManager::GetStandardNetworkClockOperationEventForServiceManager>, "GetStandardNetworkClockOperationEventForServiceManager"},
        {52,  D<&ServiceManager::GetEphemeralNetworkClockOperationEventForServiceManager>, "GetEphemeralNetworkClockOperationEventForServiceManager"},
        {60,  D<&ServiceManager::GetStandardUserSystemClockAutomaticCorrectionUpdatedEvent>, "GetStandardUserSystemClockAutomaticCorrectionUpdatedEvent"},
        {100, D<&ServiceManager::SetStandardSteadyClockBaseTime>, "SetStandardSteadyClockBaseTime"},
        {200, D<&ServiceManager::GetClosestAlarmUpdatedEvent>, "GetClosestAlarmUpdatedEvent"},
        {201, D<&ServiceManager::CheckAndSignalAlarms>, "CheckAndSignalAlarms"},
        {202, D<&ServiceManager::GetClosestAlarmInfo>, "GetClosestAlarmInfo "},
    };
    // clang-format on
    RegisterHandlers(functions);

    m_local_system_context_writer.Link(m_local_operation);
    m_network_system_context_writer.Link(m_network_operation);
    m_ephemeral_system_context_writer.Link(m_ephemeral_operation);
}

Result ServiceManager::GetStaticServiceAsUser(OutInterface<StaticService> out_service) {
    LOG_DEBUG(Service_Time, "called.");

    R_RETURN(GetStaticService(out_service, StaticServiceSetupInfo{0, 0, 0, 0, 0, 0}, "time:u"));
}

Result ServiceManager::GetStaticServiceAsAdmin(OutInterface<StaticService> out_service) {
    LOG_DEBUG(Service_Time, "called.");

    R_RETURN(GetStaticService(out_service, StaticServiceSetupInfo{1, 1, 0, 1, 0, 0}, "time:a"));
}

Result ServiceManager::GetStaticServiceAsRepair(OutInterface<StaticService> out_service) {
    LOG_DEBUG(Service_Time, "called.");

    R_RETURN(GetStaticService(out_service, StaticServiceSetupInfo{0, 0, 0, 0, 1, 0}, "time:r"));
}

Result ServiceManager::GetStaticServiceAsServiceManager(OutInterface<StaticService> out_service) {
    LOG_DEBUG(Service_Time, "called.");

    R_RETURN(GetStaticService(out_service, StaticServiceSetupInfo{1, 1, 1, 1, 1, 0}, "time:sm"));
}

Result ServiceManager::SetupStandardSteadyClockCore(bool is_rtc_reset_detected,
                                                    const Common::UUID& clock_source_id,
                                                    s64 rtc_offset, s64 internal_offset,
                                                    s64 test_offset) {
    LOG_DEBUG(Service_Time,
              "called. is_rtc_reset_detected={} clock_source_id={} rtc_offset={} "
              "internal_offset={} test_offset={}",
              is_rtc_reset_detected, clock_source_id.RawString(), rtc_offset, internal_offset,
              test_offset);

    m_steady_clock.Initialize(clock_source_id, rtc_offset, internal_offset, test_offset,
                              is_rtc_reset_detected);
    auto time = m_steady_clock.GetRawTime();
    auto ticks = m_system.CoreTiming().GetClockTicks();
    auto boot_time = time - ConvertToTimeSpan(ticks).count();
    m_shared_memory.SetSteadyClockTimePoint(clock_source_id, boot_time);
    m_steady_clock.SetContinuousAdjustment(clock_source_id, boot_time);

    ContinuousAdjustmentTimePoint time_point{};
    m_steady_clock.GetContinuousAdjustment(time_point);
    m_shared_memory.SetContinuousAdjustment(time_point);

    CheckAndSetupServicesSAndP();
    R_SUCCEED();
}

Result ServiceManager::SetupStandardLocalSystemClockCore(const SystemClockContext& context,
                                                         s64 time) {
    LOG_DEBUG(Service_Time,
              "called. context={} context.steady_time_point.clock_source_id={} time={}", context,
              context.steady_time_point.clock_source_id.RawString(), time);

    m_local_system_clock.SetContextWriter(m_local_system_context_writer);
    m_local_system_clock.Initialize(context, time);

    CheckAndSetupServicesSAndP();
    R_SUCCEED();
}

Result ServiceManager::SetupStandardNetworkSystemClockCore(SystemClockContext context,
                                                           s64 accuracy) {
    LOG_DEBUG(Service_Time, "called. context={} steady_time_point.clock_source_id={} accuracy={}",
              context, context.steady_time_point.clock_source_id.RawString(), accuracy);

    // TODO this is a hack! The network clock should be updated independently, from the ntc service
    // and maybe elsewhere. We do not do that, so fix the clock to the local clock.
    m_local_system_clock.GetContext(context);

    m_network_system_clock.SetContextWriter(m_network_system_context_writer);
    m_network_system_clock.Initialize(context, accuracy);

    CheckAndSetupServicesSAndP();
    R_SUCCEED();
}

Result ServiceManager::SetupStandardUserSystemClockCore(bool automatic_correction,
                                                        SteadyClockTimePoint time_point) {
    LOG_DEBUG(Service_Time, "called. automatic_correction={} time_point={} clock_source_id={}",
              automatic_correction, time_point, time_point.clock_source_id.RawString());

    m_user_system_clock.SetAutomaticCorrection(automatic_correction);
    m_user_system_clock.SetTimePointAndSignal(time_point);
    m_user_system_clock.SetInitialized();
    m_shared_memory.SetAutomaticCorrection(automatic_correction);

    CheckAndSetupServicesSAndP();
    R_SUCCEED();
}

Result ServiceManager::SetupTimeZoneServiceCore(const LocationName& name,
                                                const RuleVersion& rule_version, u32 location_count,
                                                const SteadyClockTimePoint& time_point,
                                                InBuffer<BufferAttr_HipcAutoSelect> rule_buffer) {
    LOG_DEBUG(Service_Time,
              "called. name={} rule_version={} location_count={} time_point={} "
              "clock_source_id={}",
              name, rule_version, location_count, time_point,
              time_point.clock_source_id.RawString());

    if (m_time_zone.ParseBinary(name, rule_buffer) != ResultSuccess) {
        LOG_ERROR(Service_Time, "Failed to parse time zone binary!");
    }

    m_time_zone.SetTimePoint(time_point);
    m_time_zone.SetTotalLocationNameCount(location_count);
    m_time_zone.SetRuleVersion(rule_version);
    m_time_zone.SetInitialized();

    CheckAndSetupServicesSAndP();
    R_SUCCEED();
}

Result ServiceManager::SetupEphemeralNetworkSystemClockCore() {
    LOG_DEBUG(Service_Time, "called.");

    m_ephemeral_network_clock.SetContextWriter(m_ephemeral_system_context_writer);
    m_ephemeral_network_clock.SetInitialized();

    CheckAndSetupServicesSAndP();
    R_SUCCEED();
}

Result ServiceManager::GetStandardLocalClockOperationEvent(
    OutCopyHandle<Kernel::KReadableEvent> out_event) {
    LOG_DEBUG(Service_Time, "called.");

    *out_event = &m_local_operation.m_event->GetReadableEvent();
    R_SUCCEED();
}

Result ServiceManager::GetStandardNetworkClockOperationEventForServiceManager(
    OutCopyHandle<Kernel::KReadableEvent> out_event) {
    LOG_DEBUG(Service_Time, "called.");

    *out_event = &m_network_operation.m_event->GetReadableEvent();
    R_SUCCEED();
}

Result ServiceManager::GetEphemeralNetworkClockOperationEventForServiceManager(
    OutCopyHandle<Kernel::KReadableEvent> out_event) {
    LOG_DEBUG(Service_Time, "called.");

    *out_event = &m_ephemeral_operation.m_event->GetReadableEvent();
    R_SUCCEED();
}

Result ServiceManager::GetStandardUserSystemClockAutomaticCorrectionUpdatedEvent(
    OutCopyHandle<Kernel::KReadableEvent> out_event) {
    LOG_DEBUG(Service_Time, "called.");

    *out_event = &m_user_system_clock.GetEvent().GetReadableEvent();
    R_SUCCEED();
}

Result ServiceManager::SetStandardSteadyClockBaseTime(s64 base_time) {
    LOG_DEBUG(Service_Time, "called. base_time={}", base_time);

    m_steady_clock.SetRtcOffset(base_time);
    auto time = m_steady_clock.GetRawTime();
    auto ticks = m_system.CoreTiming().GetClockTicks();
    auto diff = time - ConvertToTimeSpan(ticks).count();
    m_shared_memory.UpdateBaseTime(diff);
    m_steady_clock.UpdateContinuousAdjustmentTime(diff);

    ContinuousAdjustmentTimePoint time_point{};
    m_steady_clock.GetContinuousAdjustment(time_point);
    m_shared_memory.SetContinuousAdjustment(time_point);
    R_SUCCEED();
}

Result ServiceManager::GetClosestAlarmUpdatedEvent(
    OutCopyHandle<Kernel::KReadableEvent> out_event) {
    LOG_DEBUG(Service_Time, "called.");

    *out_event = &m_alarms.GetEvent().GetReadableEvent();
    R_SUCCEED();
}

Result ServiceManager::CheckAndSignalAlarms() {
    LOG_DEBUG(Service_Time, "called.");

    m_alarms.CheckAndSignal();
    R_SUCCEED();
}

Result ServiceManager::GetClosestAlarmInfo(Out<bool> out_is_valid, Out<AlarmInfo> out_info,
                                           Out<s64> out_time) {
    Alarm* alarm{nullptr};
    *out_is_valid = m_alarms.GetClosestAlarm(&alarm);
    if (*out_is_valid) {
        *out_info = {
            .alert_time = alarm->GetAlertTime(),
            .priority = alarm->GetPriority(),
        };
        *out_time = m_alarms.GetRawTime();
    }

    LOG_DEBUG(Service_Time,
              "called. out_is_valid={} out_info.alert_time={} out_info.priority={}, out_time={}",
              *out_is_valid, out_info->alert_time, out_info->priority, *out_time);

    R_SUCCEED();
}

void ServiceManager::CheckAndSetupServicesSAndP() {
    if (m_local_system_clock.IsInitialized() && m_user_system_clock.IsInitialized() &&
        m_network_system_clock.IsInitialized() && m_steady_clock.IsInitialized() &&
        m_time_zone.IsInitialized() && m_ephemeral_network_clock.IsInitialized()) {
        SetupSAndP();
    }
}

void ServiceManager::SetupSAndP() {
    if (!m_is_s_and_p_setup) {
        m_is_s_and_p_setup = true;
        m_server_manager.RegisterNamedService(
            "time:s", std::make_shared<StaticService>(
                          m_system, StaticServiceSetupInfo{0, 0, 1, 0, 0, 0}, m_time, "time:s"));
        m_server_manager.RegisterNamedService("time:p",
                                              std::make_shared<IPowerStateRequestHandler>(
                                                  m_system, m_time->m_power_state_request_manager));
    }
}

Result ServiceManager::GetStaticService(OutInterface<StaticService> out_service,
                                        StaticServiceSetupInfo setup_info, const char* name) {
    *out_service = std::make_shared<StaticService>(m_system, setup_info, m_time, name);
    R_SUCCEED();
}

} // namespace Service::PSC::Time
