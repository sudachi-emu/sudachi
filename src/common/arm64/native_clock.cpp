// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#ifdef ANDROID
#include <sys/system_properties.h>
#endif
#include "common/arm64/native_clock.h"

namespace Common::Arm64 {

namespace {

NativeClock::FactorType GetFixedPointFactor(u64 num, u64 den) {
    return (static_cast<NativeClock::FactorType>(num) << 64) / den;
}

u64 MultiplyHigh(u64 m, NativeClock::FactorType factor) {
    return static_cast<u64>((m * factor) >> 64);
}

} // namespace

NativeClock::NativeClock() {
    const u64 host_cntfrq = GetHostCNTFRQ();
    ns_cntfrq_factor = GetFixedPointFactor(NsRatio::den, host_cntfrq);
    us_cntfrq_factor = GetFixedPointFactor(UsRatio::den, host_cntfrq);
    ms_cntfrq_factor = GetFixedPointFactor(MsRatio::den, host_cntfrq);
    guest_cntfrq_factor = GetFixedPointFactor(CNTFRQ, host_cntfrq);
    gputick_cntfrq_factor = GetFixedPointFactor(GPUTickFreq, host_cntfrq);
}

std::chrono::nanoseconds NativeClock::GetTimeNS() const {
    return std::chrono::nanoseconds{MultiplyHigh(GetUptime(), ns_cntfrq_factor)};
}

std::chrono::microseconds NativeClock::GetTimeUS() const {
    return std::chrono::microseconds{MultiplyHigh(GetUptime(), us_cntfrq_factor)};
}

std::chrono::milliseconds NativeClock::GetTimeMS() const {
    return std::chrono::milliseconds{MultiplyHigh(GetUptime(), ms_cntfrq_factor)};
}

s64 NativeClock::GetCNTPCT() const {
    return MultiplyHigh(GetUptime(), guest_cntfrq_factor);
}

s64 NativeClock::GetGPUTick() const {
    return MultiplyHigh(GetUptime(), gputick_cntfrq_factor);
}

s64 NativeClock::GetUptime() const {
    s64 cntvct_el0 = 0;
    asm volatile("dsb ish\n\t"
                 "mrs %[cntvct_el0], cntvct_el0\n\t"
                 "dsb ish\n\t"
                 : [cntvct_el0] "=r"(cntvct_el0));
    return cntvct_el0;
}

bool NativeClock::IsNative() const {
    return true;
}

s64 NativeClock::GetHostCNTFRQ() {
    u64 cntfrq_el0 = 0;
    std::string_view board{""};
#ifdef ANDROID
    char buffer[PROP_VALUE_MAX];
    int len{__system_property_get("ro.product.board", buffer)};
    board = std::string_view(buffer, static_cast<size_t>(len));
#endif
    if (board == "s5e9925") { // Exynos 2200
        cntfrq_el0 = 25600000;
    } else if (board == "exynos2100") { // Exynos 2100
        cntfrq_el0 = 26000000;
    } else if (board == "exynos9810") { // Exynos 9810
        cntfrq_el0 = 26000000;
    } else if (board == "s5e8825") { // Exynos 1280
        cntfrq_el0 = 26000000;
    } else {
        asm("mrs %[cntfrq_el0], cntfrq_el0" : [cntfrq_el0] "=r"(cntfrq_el0));
    }
    return cntfrq_el0;
}

} // namespace Common::Arm64
