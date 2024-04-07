// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <dynarmic/interface/halt_reason.h>

#include "core/arm/arm_interface.h"

namespace Core {

constexpr Dynarmic::HaltReason StepThread = Dynarmic::HaltReason::Step;
constexpr Dynarmic::HaltReason DataAbort = Dynarmic::HaltReason::MemoryAbort;
constexpr Dynarmic::HaltReason BreakLoop = Dynarmic::HaltReason::UserDefined2;
constexpr Dynarmic::HaltReason SupervisorCall = Dynarmic::HaltReason::UserDefined3;
constexpr Dynarmic::HaltReason InstructionBreakpoint = Dynarmic::HaltReason::UserDefined4;
constexpr Dynarmic::HaltReason PrefetchAbort = Dynarmic::HaltReason::UserDefined6;

constexpr HaltReason TranslateHaltReason(Dynarmic::HaltReason hr) {
    static_assert(static_cast<u64>(HaltReason::StepThread) == static_cast<u64>(StepThread));
    static_assert(static_cast<u64>(HaltReason::DataAbort) == static_cast<u64>(DataAbort));
    static_assert(static_cast<u64>(HaltReason::BreakLoop) == static_cast<u64>(BreakLoop));
    static_assert(static_cast<u64>(HaltReason::SupervisorCall) == static_cast<u64>(SupervisorCall));
    static_assert(static_cast<u64>(HaltReason::InstructionBreakpoint) ==
                  static_cast<u64>(InstructionBreakpoint));
    static_assert(static_cast<u64>(HaltReason::PrefetchAbort) == static_cast<u64>(PrefetchAbort));

    return static_cast<HaltReason>(hr);
}

#ifdef __linux__

class ScopedJitExecution {
public:
    explicit ScopedJitExecution(Kernel::KProcess* process);
    ~ScopedJitExecution();
    static void RegisterHandler();
};

#else

class ScopedJitExecution {
public:
    explicit ScopedJitExecution(Kernel::KProcess* process) {}
    ~ScopedJitExecution() {}
    static void RegisterHandler() {}
};

#endif

} // namespace Core
