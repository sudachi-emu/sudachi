// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/scope_exit.h"
#include "core/core.h"
#include "core/core_timing.h"
#include "core/hle/service/glue/time/file_timestamp_worker.h"
#include "core/hle/service/glue/time/standard_steady_clock_resource.h"
#include "core/hle/service/glue/time/worker.h"
#include "core/hle/service/os/multi_wait_utils.h"
#include "core/hle/service/psc/time/common.h"
#include "core/hle/service/psc/time/service_manager.h"
#include "core/hle/service/psc/time/static.h"
#include "core/hle/service/psc/time/system_clock.h"
#include "core/hle/service/set/system_settings_server.h"
#include "core/hle/service/sm/sm.h"

namespace Service::Glue::Time {

TimeWorker::TimeWorker(Core::System& system, StandardSteadyClockResource& steady_clock_resource,
                       FileTimestampWorker& file_timestamp_worker)
    : m_system{system}, m_ctx{m_system, "Glue:TimeWorker"},
      m_event{m_ctx.CreateEvent("Glue:TimeWorker:Event")},
      m_steady_clock_resource{steady_clock_resource},
      m_file_timestamp_worker{file_timestamp_worker},
      m_timer_steady_clock{m_ctx.CreateEvent("Glue:TimeWorker:SteadyClockTimerEvent")},
      m_timer_file_system{m_ctx.CreateEvent("Glue:TimeWorker:FileTimeTimerEvent")},
      m_alarm_worker{m_system, m_steady_clock_resource}, m_pm_state_change_handler{m_alarm_worker} {
    m_timer_steady_clock_timing_event = Core::Timing::CreateEvent(
        "Time::SteadyClockEvent",
        [this](s64 time,
               std::chrono::nanoseconds ns_late) -> std::optional<std::chrono::nanoseconds> {
            m_timer_steady_clock->Signal();
            return std::nullopt;
        });

    m_timer_file_system_timing_event = Core::Timing::CreateEvent(
        "Time::SteadyClockEvent",
        [this](s64 time,
               std::chrono::nanoseconds ns_late) -> std::optional<std::chrono::nanoseconds> {
            m_timer_file_system->Signal();
            return std::nullopt;
        });
}

TimeWorker::~TimeWorker() {
    m_local_clock_event->Signal();
    m_network_clock_event->Signal();
    m_ephemeral_clock_event->Signal();
    std::this_thread::sleep_for(std::chrono::milliseconds(16));

    m_thread.request_stop();
    m_event->Signal();
    m_thread.join();

    m_ctx.CloseEvent(m_event);
    m_system.CoreTiming().UnscheduleEvent(m_timer_steady_clock_timing_event);
    m_ctx.CloseEvent(m_timer_steady_clock);
    m_system.CoreTiming().UnscheduleEvent(m_timer_file_system_timing_event);
    m_ctx.CloseEvent(m_timer_file_system);
}

template <typename T>
T TimeWorker::GetSettingsItemValue(const std::string& category, const std::string& name) {
    T v{};
    auto res = m_set_sys->GetSettingsItemValueImpl(v, category, name);
    ASSERT(res == ResultSuccess);
    return v;
}

void TimeWorker::Initialize(std::shared_ptr<Service::PSC::Time::StaticService> time_sm,
                            std::shared_ptr<Service::Set::ISystemSettingsServer> set_sys) {
    m_set_sys = std::move(set_sys);
    m_time_m =
        m_system.ServiceManager().GetService<Service::PSC::Time::ServiceManager>("time:m", true);
    m_time_sm = std::move(time_sm);

    m_alarm_worker.Initialize(m_time_m);

    auto steady_clock_interval_m =
        GetSettingsItemValue<s32>("time", "standard_steady_clock_rtc_update_interval_minutes");

    auto one_minute_ns{
        std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::minutes(1)).count()};
    s64 steady_clock_interval_ns{steady_clock_interval_m * one_minute_ns};

    m_system.CoreTiming().ScheduleLoopingEvent(std::chrono::nanoseconds(0),
                                               std::chrono::nanoseconds(steady_clock_interval_ns),
                                               m_timer_steady_clock_timing_event);

    auto fs_notify_time_s = GetSettingsItemValue<s32>("time", "notify_time_to_fs_interval_seconds");
    auto one_second_ns{
        std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::seconds(1)).count()};
    s64 fs_notify_time_ns{fs_notify_time_s * one_second_ns};

    m_system.CoreTiming().ScheduleLoopingEvent(std::chrono::nanoseconds(0),
                                               std::chrono::nanoseconds(fs_notify_time_ns),
                                               m_timer_file_system_timing_event);

    auto res = m_time_sm->GetStandardLocalSystemClock(&m_local_clock);
    ASSERT(res == ResultSuccess);
    res = m_time_m->GetStandardLocalClockOperationEvent(&m_local_clock_event);
    ASSERT(res == ResultSuccess);

    res = m_time_sm->GetStandardNetworkSystemClock(&m_network_clock);
    ASSERT(res == ResultSuccess);
    res = m_time_m->GetStandardNetworkClockOperationEventForServiceManager(&m_network_clock_event);
    ASSERT(res == ResultSuccess);

    res = m_time_sm->GetEphemeralNetworkSystemClock(&m_ephemeral_clock);
    ASSERT(res == ResultSuccess);
    res =
        m_time_m->GetEphemeralNetworkClockOperationEventForServiceManager(&m_ephemeral_clock_event);
    ASSERT(res == ResultSuccess);

    res = m_time_m->GetStandardUserSystemClockAutomaticCorrectionUpdatedEvent(
        &m_standard_user_auto_correct_clock_event);
    ASSERT(res == ResultSuccess);
}

void TimeWorker::StartThread() {
    m_thread = std::jthread(std::bind_front(&TimeWorker::ThreadFunc, this));
}

void TimeWorker::ThreadFunc(std::stop_token stop_token) {
    Common::SetCurrentThreadName("TimeWorker");
    Common::SetCurrentThreadPriority(Common::ThreadPriority::Low);

    while (!stop_token.stop_requested()) {
        enum class EventType : s32 {
            Exit = 0,
            PowerStateChange = 1,
            SignalAlarms = 2,
            UpdateLocalSystemClock = 3,
            UpdateNetworkSystemClock = 4,
            UpdateEphemeralSystemClock = 5,
            UpdateSteadyClock = 6,
            UpdateFileTimestamp = 7,
            AutoCorrect = 8,
        };

        s32 index{};

        if (m_pm_state_change_handler.m_priority != 0) {
            // TODO: gIPmModuleService::GetEvent() 1
            index = WaitAny(m_system.Kernel(),
                            &m_event->GetReadableEvent(), // 0
                            &m_alarm_worker.GetEvent()    // 1
            );
        } else {
            // TODO: gIPmModuleService::GetEvent() 1
            index = WaitAny(m_system.Kernel(),
                            &m_event->GetReadableEvent(),                       // 0
                            &m_alarm_worker.GetEvent(),                         // 1
                            &m_alarm_worker.GetTimerEvent().GetReadableEvent(), // 2
                            m_local_clock_event,                                // 3
                            m_network_clock_event,                              // 4
                            m_ephemeral_clock_event,                            // 5
                            &m_timer_steady_clock->GetReadableEvent(),          // 6
                            &m_timer_file_system->GetReadableEvent(),           // 7
                            m_standard_user_auto_correct_clock_event            // 8
            );
        }

        switch (static_cast<EventType>(index)) {
        case EventType::Exit:
            return;

        case EventType::PowerStateChange:
            m_alarm_worker.GetEvent().Clear();
            if (m_pm_state_change_handler.m_priority <= 1) {
                m_alarm_worker.OnPowerStateChanged();
            }
            break;

        case EventType::SignalAlarms:
            m_alarm_worker.GetTimerEvent().Clear();
            m_time_m->CheckAndSignalAlarms();
            break;

        case EventType::UpdateLocalSystemClock: {
            m_local_clock_event->Clear();

            Service::PSC::Time::SystemClockContext context{};
            R_ASSERT(m_local_clock->GetSystemClockContext(&context));

            m_set_sys->SetUserSystemClockContext(context);
            m_file_timestamp_worker.SetFilesystemPosixTime();
            break;
        }

        case EventType::UpdateNetworkSystemClock: {
            m_network_clock_event->Clear();

            Service::PSC::Time::SystemClockContext context{};
            R_ASSERT(m_network_clock->GetSystemClockContext(&context));

            m_set_sys->SetNetworkSystemClockContext(context);

            s64 time{};
            if (m_network_clock->GetCurrentTime(&time) != ResultSuccess) {
                break;
            }

            [[maybe_unused]] auto offset_before{
                m_ig_report_network_clock_context_set ? m_report_network_clock_context.offset : 0};
            // TODO system report "standard_netclock_operation"
            //              "clock_time" = time
            //              "context_offset_before" = offset_before
            //              "context_offset_after"  = context.offset
            m_report_network_clock_context = context;
            if (!m_ig_report_network_clock_context_set) {
                m_ig_report_network_clock_context_set = true;
            }

            m_file_timestamp_worker.SetFilesystemPosixTime();
            break;
        }

        case EventType::UpdateEphemeralSystemClock: {
            m_ephemeral_clock_event->Clear();

            Service::PSC::Time::SystemClockContext context{};
            auto res = m_ephemeral_clock->GetSystemClockContext(&context);
            if (res != ResultSuccess) {
                break;
            }

            s64 time{};
            res = m_ephemeral_clock->GetCurrentTime(&time);
            if (res != ResultSuccess) {
                break;
            }

            [[maybe_unused]] auto offset_before{m_ig_report_ephemeral_clock_context_set
                                                    ? m_report_ephemeral_clock_context.offset
                                                    : 0};
            // TODO system report "ephemeral_netclock_operation"
            //              "clock_time" = time
            //              "context_offset_before" = offset_before
            //              "context_offset_after"  = context.offset
            m_report_ephemeral_clock_context = context;
            if (!m_ig_report_ephemeral_clock_context_set) {
                m_ig_report_ephemeral_clock_context_set = true;
            }
            break;
        }

        case EventType::UpdateSteadyClock:
            m_timer_steady_clock->Clear();

            m_steady_clock_resource.UpdateTime();
            m_time_m->SetStandardSteadyClockBaseTime(m_steady_clock_resource.GetTime());
            break;

        case EventType::UpdateFileTimestamp:
            m_timer_file_system->Clear();

            m_file_timestamp_worker.SetFilesystemPosixTime();
            break;

        case EventType::AutoCorrect: {
            m_standard_user_auto_correct_clock_event->Clear();

            bool automatic_correction{};
            R_ASSERT(m_time_sm->IsStandardUserSystemClockAutomaticCorrectionEnabled(
                &automatic_correction));

            Service::PSC::Time::SteadyClockTimePoint time_point{};
            R_ASSERT(
                m_time_sm->GetStandardUserSystemClockAutomaticCorrectionUpdatedTime(&time_point));

            m_set_sys->SetUserSystemClockAutomaticCorrectionEnabled(automatic_correction);
            m_set_sys->SetUserSystemClockAutomaticCorrectionUpdatedTime(time_point);
            break;
        }

        default:
            UNREACHABLE();
        }
    }
}

} // namespace Service::Glue::Time
