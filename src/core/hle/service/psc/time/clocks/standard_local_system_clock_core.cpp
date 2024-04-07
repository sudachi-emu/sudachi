// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/hle/service/psc/time/clocks/standard_local_system_clock_core.h"

namespace Service::PSC::Time {

void StandardLocalSystemClockCore::Initialize(const SystemClockContext& context, s64 time) {
    SteadyClockTimePoint time_point{};
    if (GetCurrentTimePoint(time_point) == ResultSuccess &&
        context.steady_time_point.IdMatches(time_point)) {
        SetContextAndWrite(context);
    } else if (SetCurrentTime(time) != ResultSuccess) {
        LOG_ERROR(Service_Time, "Failed to SetCurrentTime");
    }

    SetInitialized();
}

} // namespace Service::PSC::Time
