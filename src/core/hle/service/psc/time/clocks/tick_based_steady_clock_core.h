// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <mutex>

#include "common/uuid.h"
#include "core/hle/service/psc/time/clocks/steady_clock_core.h"

namespace Core {
class System;
}

namespace Service::PSC::Time {
class TickBasedSteadyClockCore : public SteadyClockCore {
public:
    explicit TickBasedSteadyClockCore(Core::System& system) : m_system{system} {}
    ~TickBasedSteadyClockCore() override = default;

    Result GetCurrentTimePointImpl(SteadyClockTimePoint& out_time_point) override;
    s64 GetCurrentRawTimePointImpl() override;
    s64 GetTestOffsetImpl() const override;
    void SetTestOffsetImpl(s64 offset) override;
    s64 GetInternalOffsetImpl() const override;
    void SetInternalOffsetImpl(s64 offset) override;

    Result GetRtcValueImpl(s64& out_value) override {
        R_RETURN(ResultNotImplemented);
    }

    Result GetSetupResultValueImpl() override {
        R_SUCCEED();
    }

private:
    Core::System& m_system;

    ClockSourceId m_clock_source_id{Common::UUID::MakeRandom()};
};
} // namespace Service::PSC::Time
