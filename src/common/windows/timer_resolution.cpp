// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <windows.h>

#include "common/windows/timer_resolution.h"

extern "C" {
// http://undocumented.ntinternals.net/index.html?page=UserMode%2FUndocumented%20Functions%2FTime%2FNtQueryTimerResolution.html
NTSYSAPI LONG NTAPI NtQueryTimerResolution(PULONG MinimumResolution, PULONG MaximumResolution,
                                           PULONG CurrentResolution);

// http://undocumented.ntinternals.net/index.html?page=UserMode%2FUndocumented%20Functions%2FTime%2FNtSetTimerResolution.html
NTSYSAPI LONG NTAPI NtSetTimerResolution(ULONG DesiredResolution, BOOLEAN SetResolution,
                                         PULONG CurrentResolution);

// http://undocumented.ntinternals.net/index.html?page=UserMode%2FUndocumented%20Functions%2FNT%20Objects%2FThread%2FNtDelayExecution.html
NTSYSAPI LONG NTAPI NtDelayExecution(BOOLEAN Alertable, PLARGE_INTEGER DelayInterval);
}

// Defines for compatibility with older Windows 10 SDKs.

#ifndef PROCESS_POWER_THROTTLING_EXECUTION_SPEED
#define PROCESS_POWER_THROTTLING_EXECUTION_SPEED 0x1
#endif
#ifndef PROCESS_POWER_THROTTLING_IGNORE_TIMER_RESOLUTION
#define PROCESS_POWER_THROTTLING_IGNORE_TIMER_RESOLUTION 0x4
#endif

namespace Common::Windows {

namespace {

using namespace std::chrono;

constexpr nanoseconds ToNS(ULONG hundred_ns) {
    return nanoseconds{hundred_ns * 100};
}

constexpr ULONG ToHundredNS(nanoseconds ns) {
    return static_cast<ULONG>(ns.count()) / 100;
}

struct TimerResolution {
    std::chrono::nanoseconds minimum;
    std::chrono::nanoseconds maximum;
    std::chrono::nanoseconds current;
};

TimerResolution GetTimerResolution() {
    ULONG MinimumTimerResolution;
    ULONG MaximumTimerResolution;
    ULONG CurrentTimerResolution;
    NtQueryTimerResolution(&MinimumTimerResolution, &MaximumTimerResolution,
                           &CurrentTimerResolution);
    return {
        .minimum{ToNS(MinimumTimerResolution)},
        .maximum{ToNS(MaximumTimerResolution)},
        .current{ToNS(CurrentTimerResolution)},
    };
}

void SetHighQoS() {
    // https://learn.microsoft.com/en-us/windows/win32/procthread/quality-of-service
    PROCESS_POWER_THROTTLING_STATE PowerThrottling{
        .Version{PROCESS_POWER_THROTTLING_CURRENT_VERSION},
        .ControlMask{PROCESS_POWER_THROTTLING_EXECUTION_SPEED |
                     PROCESS_POWER_THROTTLING_IGNORE_TIMER_RESOLUTION},
        .StateMask{},
    };
    SetProcessInformation(GetCurrentProcess(), ProcessPowerThrottling, &PowerThrottling,
                          sizeof(PROCESS_POWER_THROTTLING_STATE));
}

} // Anonymous namespace

nanoseconds GetMinimumTimerResolution() {
    return GetTimerResolution().minimum;
}

nanoseconds GetMaximumTimerResolution() {
    return GetTimerResolution().maximum;
}

nanoseconds GetCurrentTimerResolution() {
    return GetTimerResolution().current;
}

nanoseconds SetCurrentTimerResolution(nanoseconds timer_resolution) {
    // Set the timer resolution, and return the current timer resolution.
    const auto DesiredTimerResolution = ToHundredNS(timer_resolution);
    ULONG CurrentTimerResolution;
    NtSetTimerResolution(DesiredTimerResolution, TRUE, &CurrentTimerResolution);
    return ToNS(CurrentTimerResolution);
}

nanoseconds SetCurrentTimerResolutionToMaximum() {
    SetHighQoS();
    return SetCurrentTimerResolution(GetMaximumTimerResolution());
}

void SleepForOneTick() {
    LARGE_INTEGER DelayInterval{
        .QuadPart{-1},
    };
    NtDelayExecution(FALSE, &DelayInterval);
}

} // namespace Common::Windows
