// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/core.h"
#include "core/core_timing.h"
#include "core/hle/kernel/svc.h"
#include "core/hle/service/glue/time/alarm_worker.h"
#include "core/hle/service/psc/time/service_manager.h"
#include "core/hle/service/sm/sm.h"

namespace Service::Glue::Time {

AlarmWorker::AlarmWorker(Core::System& system, StandardSteadyClockResource& steady_clock_resource)
    : m_system{system}, m_ctx{system, "Glue:AlarmWorker"},
      m_steady_clock_resource{steady_clock_resource} {}

AlarmWorker::~AlarmWorker() {
    m_system.CoreTiming().UnscheduleEvent(m_timer_timing_event);

    m_ctx.CloseEvent(m_timer_event);
}

void AlarmWorker::Initialize(std::shared_ptr<Service::PSC::Time::ServiceManager> time_m) {
    m_time_m = std::move(time_m);

    m_timer_event = m_ctx.CreateEvent("Glue:AlarmWorker:TimerEvent");
    m_timer_timing_event = Core::Timing::CreateEvent(
        "Glue:AlarmWorker::AlarmTimer",
        [this](s64 time,
               std::chrono::nanoseconds ns_late) -> std::optional<std::chrono::nanoseconds> {
            m_timer_event->Signal();
            return std::nullopt;
        });

    AttachToClosestAlarmEvent();
}

bool AlarmWorker::GetClosestAlarmInfo(Service::PSC::Time::AlarmInfo& out_alarm_info,
                                      s64& out_time) {
    bool is_valid{};
    Service::PSC::Time::AlarmInfo alarm_info{};
    s64 closest_time{};

    auto res = m_time_m->GetClosestAlarmInfo(&is_valid, &alarm_info, &closest_time);
    ASSERT(res == ResultSuccess);

    if (is_valid) {
        out_alarm_info = alarm_info;
        out_time = closest_time;
    }

    return is_valid;
}

void AlarmWorker::OnPowerStateChanged() {
    Service::PSC::Time::AlarmInfo closest_alarm_info{};
    s64 closest_time{};
    if (!GetClosestAlarmInfo(closest_alarm_info, closest_time)) {
        m_system.CoreTiming().UnscheduleEvent(m_timer_timing_event);
        m_timer_event->Clear();
        return;
    }

    if (closest_alarm_info.alert_time <= closest_time) {
        m_time_m->CheckAndSignalAlarms();
    } else {
        auto next_time{closest_alarm_info.alert_time - closest_time};

        m_system.CoreTiming().UnscheduleEvent(m_timer_timing_event);
        m_timer_event->Clear();

        m_system.CoreTiming().ScheduleEvent(std::chrono::nanoseconds(next_time),
                                            m_timer_timing_event);
    }
}

Result AlarmWorker::AttachToClosestAlarmEvent() {
    m_time_m->GetClosestAlarmUpdatedEvent(&m_event);

    R_SUCCEED();
}

} // namespace Service::Glue::Time
