// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/core.h"
#include "core/core_timing.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/svc.h"

namespace Kernel::Svc {

/// This returns the total CPU ticks elapsed since the CPU was powered-on
int64_t GetSystemTick(Core::System& system) {
    LOG_TRACE(Kernel_SVC, "called");

    // Returns the value of cntpct_el0 (https://switchbrew.org/wiki/SVC#svcGetSystemTick)
    return static_cast<int64_t>(system.CoreTiming().GetClockTicks());
}

int64_t GetSystemTick64(Core::System& system) {
    return GetSystemTick(system);
}

int64_t GetSystemTick64From32(Core::System& system) {
    return GetSystemTick(system);
}

} // namespace Kernel::Svc
