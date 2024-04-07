// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#if defined(_WIN32)
#include <windows.h>
#else
#include <time.h>
#endif

#include "common/steady_clock.h"

namespace Common {

#ifdef _WIN32
static s64 WindowsQueryPerformanceFrequency() {
    LARGE_INTEGER frequency;
    QueryPerformanceFrequency(&frequency);
    return frequency.QuadPart;
}

static s64 WindowsQueryPerformanceCounter() {
    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    return counter.QuadPart;
}

static s64 GetSystemTimeNS() {
    // GetSystemTimePreciseAsFileTime returns the file time in 100ns units.
    static constexpr s64 Multiplier = 100;
    // Convert Windows epoch to Unix epoch.
    static constexpr s64 WindowsEpochToUnixEpoch = 0x19DB1DED53E8000LL;

    FILETIME filetime;
    GetSystemTimePreciseAsFileTime(&filetime);
    return Multiplier * ((static_cast<s64>(filetime.dwHighDateTime) << 32) +
                         static_cast<s64>(filetime.dwLowDateTime) - WindowsEpochToUnixEpoch);
}
#endif

SteadyClock::time_point SteadyClock::Now() noexcept {
#if defined(_WIN32)
    static const auto freq = WindowsQueryPerformanceFrequency();
    const auto counter = WindowsQueryPerformanceCounter();

    // 10 MHz is a very common QPC frequency on modern PCs.
    // Optimizing for this specific frequency can double the performance of
    // this function by avoiding the expensive frequency conversion path.
    static constexpr s64 TenMHz = 10'000'000;

    if (freq == TenMHz) [[likely]] {
        static_assert(period::den % TenMHz == 0);
        static constexpr s64 Multiplier = period::den / TenMHz;
        return time_point{duration{counter * Multiplier}};
    }

    const auto whole = (counter / freq) * period::den;
    const auto part = (counter % freq) * period::den / freq;
    return time_point{duration{whole + part}};
#elif defined(__APPLE__)
    return time_point{duration{clock_gettime_nsec_np(CLOCK_MONOTONIC_RAW)}};
#else
    timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return time_point{std::chrono::seconds{ts.tv_sec} + std::chrono::nanoseconds{ts.tv_nsec}};
#endif
}

RealTimeClock::time_point RealTimeClock::Now() noexcept {
#if defined(_WIN32)
    return time_point{duration{GetSystemTimeNS()}};
#elif defined(__APPLE__)
    return time_point{duration{clock_gettime_nsec_np(CLOCK_REALTIME)}};
#else
    timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return time_point{std::chrono::seconds{ts.tv_sec} + std::chrono::nanoseconds{ts.tv_nsec}};
#endif
}

}; // namespace Common
