// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/hle/kernel/svc.h"
#include "core/hle/kernel/svc_results.h"

namespace Kernel::Svc {

void SleepSystem(Core::System& system) {
    UNIMPLEMENTED();
}

void SleepSystem64(Core::System& system) {
    return SleepSystem(system);
}

void SleepSystem64From32(Core::System& system) {
    return SleepSystem(system);
}

} // namespace Kernel::Svc
