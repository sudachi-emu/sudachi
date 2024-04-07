// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "core/hle/service/cmif_types.h"
#include "core/hle/service/ipc_helpers.h"
#include "core/hle/service/psc/time/common.h"
#include "core/hle/service/psc/time/manager.h"
#include "core/hle/service/server_manager.h"
#include "core/hle/service/service.h"

namespace Core {
class System;
}

namespace Service::PSC::Time {

class SteadyClock final : public ServiceFramework<SteadyClock> {
public:
    explicit SteadyClock(Core::System& system, std::shared_ptr<TimeManager> manager,
                         bool can_write_steady_clock, bool can_write_uninitialized_clock);

    ~SteadyClock() override = default;

    Result GetCurrentTimePoint(Out<SteadyClockTimePoint> out_time_point);
    Result GetTestOffset(Out<s64> out_test_offset);
    Result SetTestOffset(s64 test_offset);
    Result GetRtcValue(Out<s64> out_rtc_value);
    Result IsRtcResetDetected(Out<bool> out_is_detected);
    Result GetSetupResultValue(Out<Result> out_result);
    Result GetInternalOffset(Out<s64> out_internal_offset);

private:
    Core::System& m_system;

    StandardSteadyClockCore& m_clock_core;
    bool m_can_write_steady_clock;
    bool m_can_write_uninitialized_clock;
};

} // namespace Service::PSC::Time
