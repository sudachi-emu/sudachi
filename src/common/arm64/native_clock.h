// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "common/wall_clock.h"

namespace Common::Arm64 {

class NativeClock final : public WallClock {
public:
    explicit NativeClock();

    std::chrono::nanoseconds GetTimeNS() const override;

    std::chrono::microseconds GetTimeUS() const override;

    std::chrono::milliseconds GetTimeMS() const override;

    s64 GetCNTPCT() const override;

    s64 GetGPUTick() const override;

    s64 GetUptime() const override;

    bool IsNative() const override;

    static s64 GetHostCNTFRQ();

public:
    using FactorType = unsigned __int128;

    FactorType GetGuestCNTFRQFactor() const {
        return guest_cntfrq_factor;
    }

private:
    FactorType ns_cntfrq_factor;
    FactorType us_cntfrq_factor;
    FactorType ms_cntfrq_factor;
    FactorType guest_cntfrq_factor;
    FactorType gputick_cntfrq_factor;
};

} // namespace Common::Arm64
