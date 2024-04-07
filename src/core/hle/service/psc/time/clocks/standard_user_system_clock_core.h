// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "core/hle/kernel/k_event.h"
#include "core/hle/result.h"
#include "core/hle/service/kernel_helpers.h"
#include "core/hle/service/psc/time/clocks/context_writers.h"
#include "core/hle/service/psc/time/clocks/standard_local_system_clock_core.h"
#include "core/hle/service/psc/time/clocks/standard_network_system_clock_core.h"
#include "core/hle/service/psc/time/clocks/steady_clock_core.h"
#include "core/hle/service/psc/time/clocks/system_clock_core.h"
#include "core/hle/service/psc/time/common.h"

namespace Core {
class System;
}

namespace Service::PSC::Time {

class StandardUserSystemClockCore : public SystemClockCore {
public:
    explicit StandardUserSystemClockCore(Core::System& system,
                                         StandardLocalSystemClockCore& local_clock,
                                         StandardNetworkSystemClockCore& network_clock);
    ~StandardUserSystemClockCore() override;

    Kernel::KEvent& GetEvent() {
        return *m_event;
    }

    bool GetAutomaticCorrection() const {
        return m_automatic_correction;
    }
    Result SetAutomaticCorrection(bool automatic_correction);

    Result GetContext(SystemClockContext& out_context) const override;
    Result SetContext(const SystemClockContext& context) override;

    Result GetTimePoint(SteadyClockTimePoint& out_time_point);
    void SetTimePointAndSignal(SteadyClockTimePoint& time_point);

private:
    Core::System& m_system;
    KernelHelpers::ServiceContext m_ctx;

    bool m_automatic_correction{};
    StandardLocalSystemClockCore& m_local_system_clock;
    StandardNetworkSystemClockCore& m_network_system_clock;
    SteadyClockTimePoint m_time_point{};
    Kernel::KEvent* m_event{};
};

} // namespace Service::PSC::Time
