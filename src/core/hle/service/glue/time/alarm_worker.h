// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "common/common_types.h"
#include "core/hle/kernel/k_event.h"
#include "core/hle/service/kernel_helpers.h"
#include "core/hle/service/psc/time/common.h"

namespace Core {
class System;
}

namespace Service::PSC::Time {
class ServiceManager;
}

namespace Service::Glue::Time {
class StandardSteadyClockResource;

class AlarmWorker {
public:
    explicit AlarmWorker(Core::System& system, StandardSteadyClockResource& steady_clock_resource);
    ~AlarmWorker();

    void Initialize(std::shared_ptr<Service::PSC::Time::ServiceManager> time_m);

    Kernel::KReadableEvent& GetEvent() {
        return *m_event;
    }

    Kernel::KEvent& GetTimerEvent() {
        return *m_timer_event;
    }

    void OnPowerStateChanged();

private:
    bool GetClosestAlarmInfo(Service::PSC::Time::AlarmInfo& out_alarm_info, s64& out_time);
    Result AttachToClosestAlarmEvent();

    Core::System& m_system;
    KernelHelpers::ServiceContext m_ctx;
    std::shared_ptr<Service::PSC::Time::ServiceManager> m_time_m;

    Kernel::KReadableEvent* m_event{};
    Kernel::KEvent* m_timer_event{};
    std::shared_ptr<Core::Timing::EventType> m_timer_timing_event;
    StandardSteadyClockResource& m_steady_clock_resource;
};

} // namespace Service::Glue::Time
