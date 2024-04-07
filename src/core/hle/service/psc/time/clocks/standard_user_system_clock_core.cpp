// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/core.h"
#include "core/hle/service/psc/time/clocks/standard_user_system_clock_core.h"

namespace Service::PSC::Time {

StandardUserSystemClockCore::StandardUserSystemClockCore(
    Core::System& system, StandardLocalSystemClockCore& local_clock,
    StandardNetworkSystemClockCore& network_clock)
    : SystemClockCore{local_clock.GetSteadyClock()}, m_system{system},
      m_ctx{m_system, "Psc:StandardUserSystemClockCore"}, m_local_system_clock{local_clock},
      m_network_system_clock{network_clock},
      m_event{m_ctx.CreateEvent("Psc:StandardUserSystemClockCore:Event")} {}

StandardUserSystemClockCore::~StandardUserSystemClockCore() {
    m_ctx.CloseEvent(m_event);
}

Result StandardUserSystemClockCore::SetAutomaticCorrection(bool automatic_correction) {
    R_SUCCEED_IF(m_automatic_correction == automatic_correction);
    R_SUCCEED_IF(!m_network_system_clock.CheckClockSourceMatches());

    SystemClockContext context{};
    R_TRY(m_network_system_clock.GetContext(context));
    R_TRY(m_local_system_clock.SetContextAndWrite(context));

    m_automatic_correction = automatic_correction;
    R_SUCCEED();
}

Result StandardUserSystemClockCore::GetContext(SystemClockContext& out_context) const {
    if (!m_automatic_correction) {
        R_RETURN(m_local_system_clock.GetContext(out_context));
    }

    if (!m_network_system_clock.CheckClockSourceMatches()) {
        R_RETURN(m_local_system_clock.GetContext(out_context));
    }

    SystemClockContext context{};
    R_TRY(m_network_system_clock.GetContext(context));
    R_TRY(m_local_system_clock.SetContextAndWrite(context));

    R_RETURN(m_local_system_clock.GetContext(out_context));
}

Result StandardUserSystemClockCore::SetContext(const SystemClockContext& context) {
    R_RETURN(ResultNotImplemented);
}

Result StandardUserSystemClockCore::GetTimePoint(SteadyClockTimePoint& out_time_point) {
    out_time_point = m_time_point;
    R_SUCCEED();
}

void StandardUserSystemClockCore::SetTimePointAndSignal(SteadyClockTimePoint& time_point) {
    m_time_point = time_point;
    m_event->Signal();
}

} // namespace Service::PSC::Time
