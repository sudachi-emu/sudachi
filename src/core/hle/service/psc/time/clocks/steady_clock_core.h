// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <chrono>

#include "core/hle/result.h"
#include "core/hle/service/psc/time/common.h"

namespace Service::PSC::Time {

class SteadyClockCore {
public:
    SteadyClockCore() = default;
    virtual ~SteadyClockCore() = default;

    void SetInitialized() {
        m_initialized = true;
    }

    bool IsInitialized() const {
        return m_initialized;
    }

    void SetResetDetected() {
        m_reset_detected = true;
    }

    bool IsResetDetected() const {
        return m_reset_detected;
    }

    Result GetCurrentTimePoint(SteadyClockTimePoint& out_time_point) {
        R_TRY(GetCurrentTimePointImpl(out_time_point));

        auto one_second_ns{
            std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::seconds(1)).count()};
        out_time_point.time_point += GetTestOffsetImpl() / one_second_ns;
        out_time_point.time_point += GetInternalOffsetImpl() / one_second_ns;
        R_SUCCEED();
    }

    s64 GetTestOffset() const {
        return GetTestOffsetImpl();
    }

    void SetTestOffset(s64 offset) {
        SetTestOffsetImpl(offset);
    }

    s64 GetInternalOffset() const {
        return GetInternalOffsetImpl();
    }

    s64 GetRawTime() {
        return GetCurrentRawTimePointImpl() + GetTestOffsetImpl() + GetInternalOffsetImpl();
    }

    Result GetRtcValue(s64& out_value) {
        R_RETURN(GetRtcValueImpl(out_value));
    }

    Result GetSetupResultValue() {
        R_RETURN(GetSetupResultValueImpl());
    }

private:
    virtual Result GetCurrentTimePointImpl(SteadyClockTimePoint& out_time_point) = 0;
    virtual s64 GetCurrentRawTimePointImpl() = 0;
    virtual s64 GetTestOffsetImpl() const = 0;
    virtual void SetTestOffsetImpl(s64 offset) = 0;
    virtual s64 GetInternalOffsetImpl() const = 0;
    virtual void SetInternalOffsetImpl(s64 offset) = 0;
    virtual Result GetRtcValueImpl(s64& out_value) = 0;
    virtual Result GetSetupResultValueImpl() = 0;

    bool m_initialized{};
    bool m_reset_detected{};
};
} // namespace Service::PSC::Time
