// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/hle/service/psc/time/clocks/context_writers.h"
#include "core/hle/service/psc/time/clocks/system_clock_core.h"

namespace Service::PSC::Time {

bool SystemClockCore::CheckClockSourceMatches() {
    SystemClockContext context{};
    if (GetContext(context) != ResultSuccess) {
        return false;
    }

    SteadyClockTimePoint time_point{};
    if (m_steady_clock.GetCurrentTimePoint(time_point) != ResultSuccess) {
        return false;
    }

    return context.steady_time_point.IdMatches(time_point);
}

Result SystemClockCore::GetCurrentTime(s64* out_time) const {
    R_UNLESS(out_time != nullptr, ResultInvalidArgument);

    SystemClockContext context{};
    SteadyClockTimePoint time_point{};

    R_TRY(m_steady_clock.GetCurrentTimePoint(time_point));
    R_TRY(GetContext(context));

    R_UNLESS(context.steady_time_point.IdMatches(time_point), ResultClockMismatch);

    *out_time = context.offset + time_point.time_point;
    R_SUCCEED();
}

Result SystemClockCore::SetCurrentTime(s64 time) {
    SteadyClockTimePoint time_point{};
    R_TRY(m_steady_clock.GetCurrentTimePoint(time_point));

    SystemClockContext context{
        .offset = time - time_point.time_point,
        .steady_time_point = time_point,
    };
    R_RETURN(SetContextAndWrite(context));
}

Result SystemClockCore::GetContext(SystemClockContext& out_context) const {
    out_context = m_context;
    R_SUCCEED();
}

Result SystemClockCore::SetContext(const SystemClockContext& context) {
    m_context = context;
    R_SUCCEED();
}

Result SystemClockCore::SetContextAndWrite(const SystemClockContext& context) {
    R_TRY(SetContext(context));

    if (m_context_writer) {
        R_RETURN(m_context_writer->Write(context));
    }

    R_SUCCEED();
}

void SystemClockCore::LinkOperationEvent(OperationEvent& operation_event) {
    if (m_context_writer) {
        m_context_writer->Link(operation_event);
    }
}

} // namespace Service::PSC::Time
