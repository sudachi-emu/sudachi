// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/core.h"
#include "core/hle/kernel/physical_core.h"
#include "core/hle/kernel/svc.h"

namespace Kernel::Svc {

void CallSecureMonitor(Core::System& system, lp64::SecureMonitorArguments* args) {
    UNIMPLEMENTED();
}

void CallSecureMonitor64(Core::System& system, lp64::SecureMonitorArguments* args) {
    CallSecureMonitor(system, args);
}

void CallSecureMonitor64From32(Core::System& system, ilp32::SecureMonitorArguments* args) {
    // CallSecureMonitor64From32 is not supported.
    UNIMPLEMENTED_MSG("CallSecureMonitor64From32");
}

// Custom ABI for CallSecureMonitor.

void SvcWrap_CallSecureMonitor64(Core::System& system, std::span<uint64_t, 8> args) {
    lp64::SecureMonitorArguments smc_args{};
    for (int i = 0; i < 8; i++) {
        smc_args.r[i] = args[i];
    }

    CallSecureMonitor64(system, std::addressof(smc_args));

    for (int i = 0; i < 8; i++) {
        args[i] = smc_args.r[i];
    }
}

void SvcWrap_CallSecureMonitor64From32(Core::System& system, std::span<uint64_t, 8> args) {
    ilp32::SecureMonitorArguments smc_args{};
    for (int i = 0; i < 8; i++) {
        smc_args.r[i] = static_cast<u32>(args[i]);
    }

    CallSecureMonitor64From32(system, std::addressof(smc_args));

    for (int i = 0; i < 8; i++) {
        args[i] = smc_args.r[i];
    }
}

} // namespace Kernel::Svc
