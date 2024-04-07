// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <mutex>

#include "common/common_types.h"
#include "core/hle/result.h"
#include "core/hle/service/psc/time/common.h"

namespace Core {
class System;
}

namespace Service::Glue::Time {
class StandardSteadyClockResource {
public:
    StandardSteadyClockResource(Core::System& system);

    void Initialize(Common::UUID* out_source_id, Common::UUID* external_source_id);

    s64 GetTime() const {
        return m_time;
    }

    bool GetResetDetected();
    Result SetCurrentTime();
    Result GetRtcTimeInSeconds(s64& out_time);
    void UpdateTime();

private:
    Core::System& m_system;

    std::mutex m_mutex;
    Service::PSC::Time::ClockSourceId m_clock_source_id{};
    s64 m_time{};
    Result m_set_time_result;
    bool m_rtc_reset;
};
} // namespace Service::Glue::Time
