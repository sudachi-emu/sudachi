// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <mutex>

#include "core/hle/service/psc/time/clocks/steady_clock_core.h"

namespace Core {
class System;
}

namespace Service::PSC::Time {
class StandardSteadyClockCore : public SteadyClockCore {
public:
    explicit StandardSteadyClockCore(Core::System& system) : m_system{system} {}
    ~StandardSteadyClockCore() override = default;

    void Initialize(ClockSourceId clock_source_id, s64 rtc_offset, s64 internal_offset,
                    s64 test_offset, bool is_rtc_reset_detected);

    void SetRtcOffset(s64 offset);
    void SetContinuousAdjustment(ClockSourceId clock_source_id, s64 time);
    void GetContinuousAdjustment(ContinuousAdjustmentTimePoint& out_time_point) const;
    void UpdateContinuousAdjustmentTime(s64 time);

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

    std::mutex m_mutex;
    s64 m_test_offset{};
    s64 m_internal_offset{};
    ClockSourceId m_clock_source_id{};
    s64 m_rtc_offset{};
    s64 m_cached_time_point{};
    ContinuousAdjustmentTimePoint m_continuous_adjustment_time_point{};
};
} // namespace Service::PSC::Time
