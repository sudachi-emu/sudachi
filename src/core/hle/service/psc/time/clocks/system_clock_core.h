// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "core/hle/result.h"
#include "core/hle/service/psc/time/clocks/steady_clock_core.h"
#include "core/hle/service/psc/time/common.h"

namespace Service::PSC::Time {
class ContextWriter;

class SystemClockCore {
public:
    explicit SystemClockCore(SteadyClockCore& steady_clock) : m_steady_clock{steady_clock} {}
    virtual ~SystemClockCore() = default;

    SteadyClockCore& GetSteadyClock() {
        return m_steady_clock;
    }

    bool IsInitialized() const {
        return m_initialized;
    }

    void SetInitialized() {
        m_initialized = true;
    }

    void SetContextWriter(ContextWriter& context_writer) {
        m_context_writer = &context_writer;
    }

    bool CheckClockSourceMatches();

    Result GetCurrentTime(s64* out_time) const;
    Result SetCurrentTime(s64 time);

    Result GetCurrentTimePoint(SteadyClockTimePoint& out_time_point) {
        R_RETURN(m_steady_clock.GetCurrentTimePoint(out_time_point));
    }

    virtual Result GetContext(SystemClockContext& out_context) const;
    virtual Result SetContext(const SystemClockContext& context);
    Result SetContextAndWrite(const SystemClockContext& context);

    void LinkOperationEvent(OperationEvent& operation_event);

private:
    bool m_initialized{};
    ContextWriter* m_context_writer{};
    SteadyClockCore& m_steady_clock;
    SystemClockContext m_context{};
};
} // namespace Service::PSC::Time
