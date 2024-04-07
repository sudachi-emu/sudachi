// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "core/hle/service/psc/time/alarms.h"
#include "core/hle/service/psc/time/clocks/context_writers.h"
#include "core/hle/service/psc/time/clocks/ephemeral_network_system_clock_core.h"
#include "core/hle/service/psc/time/clocks/standard_local_system_clock_core.h"
#include "core/hle/service/psc/time/clocks/standard_network_system_clock_core.h"
#include "core/hle/service/psc/time/clocks/standard_steady_clock_core.h"
#include "core/hle/service/psc/time/clocks/standard_user_system_clock_core.h"
#include "core/hle/service/psc/time/clocks/tick_based_steady_clock_core.h"
#include "core/hle/service/psc/time/power_state_request_manager.h"
#include "core/hle/service/psc/time/shared_memory.h"
#include "core/hle/service/psc/time/time_zone.h"

namespace Core {
class System;
}

namespace Service::PSC::Time {
class TimeManager {
public:
    explicit TimeManager(Core::System& system)
        : m_system{system}, m_standard_steady_clock{system}, m_tick_based_steady_clock{m_system},
          m_standard_local_system_clock{m_standard_steady_clock},
          m_standard_network_system_clock{m_standard_steady_clock},
          m_standard_user_system_clock{m_system, m_standard_local_system_clock,
                                       m_standard_network_system_clock},
          m_ephemeral_network_clock{m_tick_based_steady_clock}, m_shared_memory{m_system},
          m_power_state_request_manager{m_system},
          m_alarms{m_system, m_standard_steady_clock, m_power_state_request_manager},
          m_local_system_clock_context_writer{m_system, m_shared_memory},
          m_network_system_clock_context_writer{m_system, m_shared_memory,
                                                m_standard_user_system_clock},
          m_ephemeral_network_clock_context_writer{m_system} {}

    Core::System& m_system;

    StandardSteadyClockCore m_standard_steady_clock;
    TickBasedSteadyClockCore m_tick_based_steady_clock;
    StandardLocalSystemClockCore m_standard_local_system_clock;
    StandardNetworkSystemClockCore m_standard_network_system_clock;
    StandardUserSystemClockCore m_standard_user_system_clock;
    EphemeralNetworkSystemClockCore m_ephemeral_network_clock;
    TimeZone m_time_zone;
    SharedMemory m_shared_memory;
    PowerStateRequestManager m_power_state_request_manager;
    Alarms m_alarms;
    LocalSystemClockContextWriter m_local_system_clock_context_writer;
    NetworkSystemClockContextWriter m_network_system_clock_context_writer;
    EphemeralNetworkSystemClockContextWriter m_ephemeral_network_clock_context_writer;
};

} // namespace Service::PSC::Time
