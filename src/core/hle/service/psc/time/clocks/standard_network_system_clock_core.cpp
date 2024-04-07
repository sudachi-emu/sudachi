// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/hle/service/psc/time/clocks/standard_network_system_clock_core.h"

namespace Service::PSC::Time {

void StandardNetworkSystemClockCore::Initialize(const SystemClockContext& context, s64 accuracy) {
    if (SetContextAndWrite(context) != ResultSuccess) {
        LOG_ERROR(Service_Time, "Failed to SetContext");
    }
    m_sufficient_accuracy = accuracy;
    SetInitialized();
}

bool StandardNetworkSystemClockCore::IsAccuracySufficient() {
    if (!IsInitialized()) {
        return false;
    }

    SystemClockContext context{};
    SteadyClockTimePoint current_time_point{};
    if (GetCurrentTimePoint(current_time_point) != ResultSuccess ||
        GetContext(context) != ResultSuccess) {
        return false;
    }

    s64 seconds{};
    if (GetSpanBetweenTimePoints(&seconds, context.steady_time_point, current_time_point) !=
        ResultSuccess) {
        return false;
    }

    if (std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::seconds(seconds))
            .count() < m_sufficient_accuracy) {
        return true;
    }

    return false;
}

} // namespace Service::PSC::Time
