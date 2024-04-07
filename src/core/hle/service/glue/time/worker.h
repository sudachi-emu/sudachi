// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "common/common_types.h"
#include "core/hle/kernel/k_event.h"
#include "core/hle/service/glue/time/alarm_worker.h"
#include "core/hle/service/glue/time/pm_state_change_handler.h"
#include "core/hle/service/kernel_helpers.h"

namespace Service::Set {
class ISystemSettingsServer;
}

namespace Service::PSC::Time {
class StaticService;
class SystemClock;
} // namespace Service::PSC::Time

namespace Service::Glue::Time {
class FileTimestampWorker;
class StandardSteadyClockResource;

class TimeWorker {
public:
    explicit TimeWorker(Core::System& system, StandardSteadyClockResource& steady_clock_resource,
                        FileTimestampWorker& file_timestamp_worker);
    ~TimeWorker();

    void Initialize(std::shared_ptr<Service::PSC::Time::StaticService> time_sm,
                    std::shared_ptr<Service::Set::ISystemSettingsServer> set_sys);

    void StartThread();

private:
    template <typename T>
    T GetSettingsItemValue(const std::string& category, const std::string& name);

    void ThreadFunc(std::stop_token stop_token);

    Core::System& m_system;
    KernelHelpers::ServiceContext m_ctx;
    std::shared_ptr<Service::Set::ISystemSettingsServer> m_set_sys;

    std::jthread m_thread;
    Kernel::KEvent* m_event{};
    std::shared_ptr<Service::PSC::Time::ServiceManager> m_time_m;
    std::shared_ptr<Service::PSC::Time::StaticService> m_time_sm;
    std::shared_ptr<Service::PSC::Time::SystemClock> m_network_clock;
    std::shared_ptr<Service::PSC::Time::SystemClock> m_local_clock;
    std::shared_ptr<Service::PSC::Time::SystemClock> m_ephemeral_clock;
    StandardSteadyClockResource& m_steady_clock_resource;
    FileTimestampWorker& m_file_timestamp_worker;
    Kernel::KReadableEvent* m_local_clock_event{};
    Kernel::KReadableEvent* m_network_clock_event{};
    Kernel::KReadableEvent* m_ephemeral_clock_event{};
    Kernel::KReadableEvent* m_standard_user_auto_correct_clock_event{};
    Kernel::KEvent* m_timer_steady_clock{};
    std::shared_ptr<Core::Timing::EventType> m_timer_steady_clock_timing_event;
    Kernel::KEvent* m_timer_file_system{};
    std::shared_ptr<Core::Timing::EventType> m_timer_file_system_timing_event;
    AlarmWorker m_alarm_worker;
    PmStateChangeHandler m_pm_state_change_handler;

    bool m_ig_report_network_clock_context_set{};
    Service::PSC::Time::SystemClockContext m_report_network_clock_context{};
    bool m_ig_report_ephemeral_clock_context_set{};
    Service::PSC::Time::SystemClockContext m_report_ephemeral_clock_context{};
};

} // namespace Service::Glue::Time
