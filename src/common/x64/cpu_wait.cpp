// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <thread>

#ifdef _MSC_VER
#include <intrin.h>
#endif

#include "common/x64/cpu_detect.h"
#include "common/x64/cpu_wait.h"
#include "common/x64/rdtsc.h"

namespace Common::X64 {

namespace {

// 100,000 cycles is a reasonable amount of time to wait to save on CPU resources.
// For reference:
// At 1 GHz, 100K cycles is 100us
// At 2 GHz, 100K cycles is 50us
// At 4 GHz, 100K cycles is 25us
constexpr auto PauseCycles = 100'000U;

} // Anonymous namespace

#ifdef _MSC_VER
__forceinline static void TPAUSE() {
    static constexpr auto RequestC02State = 0U;
    _tpause(RequestC02State, FencedRDTSC() + PauseCycles);
}

__forceinline static void MWAITX() {
    static constexpr auto EnableWaitTimeFlag = 1U << 1;
    static constexpr auto RequestC1State = 0U;

    // monitor_var should be aligned to a cache line.
    alignas(64) u64 monitor_var{};
    _mm_monitorx(&monitor_var, 0, 0);
    _mm_mwaitx(EnableWaitTimeFlag, RequestC1State, PauseCycles);
}
#else
static void TPAUSE() {
    static constexpr auto RequestC02State = 0U;
    const auto tsc = FencedRDTSC() + PauseCycles;
    const auto eax = static_cast<u32>(tsc & 0xFFFFFFFF);
    const auto edx = static_cast<u32>(tsc >> 32);
    asm volatile("tpause %0" : : "r"(RequestC02State), "d"(edx), "a"(eax));
}

static void MWAITX() {
    static constexpr auto EnableWaitTimeFlag = 1U << 1;
    static constexpr auto RequestC1State = 0U;

    // monitor_var should be aligned to a cache line.
    alignas(64) u64 monitor_var{};
    asm volatile("monitorx" : : "a"(&monitor_var), "c"(0), "d"(0));
    asm volatile("mwaitx" : : "a"(RequestC1State), "b"(PauseCycles), "c"(EnableWaitTimeFlag));
}
#endif

void MicroSleep() {
    static const bool has_waitpkg = GetCPUCaps().waitpkg;
    static const bool has_monitorx = GetCPUCaps().monitorx;

    if (has_waitpkg) {
        TPAUSE();
    } else if (has_monitorx) {
        MWAITX();
    } else {
        std::this_thread::yield();
    }
}

} // namespace Common::X64
