// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <chrono>

#include "core/hle/result.h"
#include "core/hle/service/psc/time/clocks/context_writers.h"
#include "core/hle/service/psc/time/clocks/steady_clock_core.h"
#include "core/hle/service/psc/time/clocks/system_clock_core.h"
#include "core/hle/service/psc/time/common.h"

namespace Service::PSC::Time {

class StandardNetworkSystemClockCore : public SystemClockCore {
public:
    explicit StandardNetworkSystemClockCore(SteadyClockCore& steady_clock)
        : SystemClockCore{steady_clock} {}
    ~StandardNetworkSystemClockCore() override = default;

    void Initialize(const SystemClockContext& context, s64 accuracy);
    bool IsAccuracySufficient();

private:
    s64 m_sufficient_accuracy{
        std::chrono ::duration_cast<std::chrono::nanoseconds>(std::chrono::days(10)).count()};
};

} // namespace Service::PSC::Time
