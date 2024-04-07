// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <thread>

#include "common/steady_clock.h"
#include "common/uint128.h"
#include "common/x64/rdtsc.h"

namespace Common::X64 {

template <u64 Nearest>
static u64 RoundToNearest(u64 value) {
    const auto mod = value % Nearest;
    return mod >= (Nearest / 2) ? (value - mod + Nearest) : (value - mod);
}

u64 EstimateRDTSCFrequency() {
    // Discard the first result measuring the rdtsc.
    FencedRDTSC();
    std::this_thread::sleep_for(std::chrono::milliseconds{1});
    FencedRDTSC();

    // Get the current time.
    const auto start_time = RealTimeClock::Now();
    const u64 tsc_start = FencedRDTSC();
    // Wait for 100 milliseconds.
    std::this_thread::sleep_for(std::chrono::milliseconds{100});
    const auto end_time = RealTimeClock::Now();
    const u64 tsc_end = FencedRDTSC();
    // Calculate differences.
    const u64 timer_diff = static_cast<u64>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - start_time).count());
    const u64 tsc_diff = tsc_end - tsc_start;
    const u64 tsc_freq = MultiplyAndDivide64(tsc_diff, 1000000000ULL, timer_diff);
    return RoundToNearest<100'000>(tsc_freq);
}

} // namespace Common::X64
