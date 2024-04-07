// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/core.h"
#include "core/hle/kernel/k_hardware_timer.h"
#include "core/hle/kernel/k_memory_layout.h"
#include "core/hle/kernel/k_process.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/svc.h"
#include "core/hle/kernel/svc_results.h"
#include "core/hle/kernel/svc_types.h"

namespace Kernel::Svc {
namespace {

constexpr bool IsValidSignalType(Svc::SignalType type) {
    switch (type) {
    case Svc::SignalType::Signal:
    case Svc::SignalType::SignalAndIncrementIfEqual:
    case Svc::SignalType::SignalAndModifyByWaitingCountIfEqual:
        return true;
    default:
        return false;
    }
}

constexpr bool IsValidArbitrationType(Svc::ArbitrationType type) {
    switch (type) {
    case Svc::ArbitrationType::WaitIfLessThan:
    case Svc::ArbitrationType::DecrementAndWaitIfLessThan:
    case Svc::ArbitrationType::WaitIfEqual:
        return true;
    default:
        return false;
    }
}

} // namespace

// Wait for an address (via Address Arbiter)
Result WaitForAddress(Core::System& system, u64 address, ArbitrationType arb_type, s32 value,
                      s64 timeout_ns) {
    LOG_TRACE(Kernel_SVC, "called, address=0x{:X}, arb_type=0x{:X}, value=0x{:X}, timeout_ns={}",
              address, arb_type, value, timeout_ns);

    // Validate input.
    R_UNLESS(!IsKernelAddress(address), ResultInvalidCurrentMemory);
    R_UNLESS(Common::IsAligned(address, sizeof(s32)), ResultInvalidAddress);
    R_UNLESS(IsValidArbitrationType(arb_type), ResultInvalidEnumValue);

    // Convert timeout from nanoseconds to ticks.
    s64 timeout{};
    if (timeout_ns > 0) {
        const s64 offset_tick(timeout_ns);
        if (offset_tick > 0) {
            timeout = system.Kernel().HardwareTimer().GetTick() + offset_tick + 2;
            if (timeout <= 0) {
                timeout = std::numeric_limits<s64>::max();
            }
        } else {
            timeout = std::numeric_limits<s64>::max();
        }
    } else {
        timeout = timeout_ns;
    }

    R_RETURN(
        GetCurrentProcess(system.Kernel()).WaitAddressArbiter(address, arb_type, value, timeout));
}

// Signals to an address (via Address Arbiter)
Result SignalToAddress(Core::System& system, u64 address, SignalType signal_type, s32 value,
                       s32 count) {
    LOG_TRACE(Kernel_SVC, "called, address=0x{:X}, signal_type=0x{:X}, value=0x{:X}, count=0x{:X}",
              address, signal_type, value, count);

    // Validate input.
    R_UNLESS(!IsKernelAddress(address), ResultInvalidCurrentMemory);
    R_UNLESS(Common::IsAligned(address, sizeof(s32)), ResultInvalidAddress);
    R_UNLESS(IsValidSignalType(signal_type), ResultInvalidEnumValue);

    R_RETURN(GetCurrentProcess(system.Kernel())
                 .SignalAddressArbiter(address, signal_type, value, count));
}

Result WaitForAddress64(Core::System& system, u64 address, ArbitrationType arb_type, s32 value,
                        s64 timeout_ns) {
    R_RETURN(WaitForAddress(system, address, arb_type, value, timeout_ns));
}

Result SignalToAddress64(Core::System& system, u64 address, SignalType signal_type, s32 value,
                         s32 count) {
    R_RETURN(SignalToAddress(system, address, signal_type, value, count));
}

Result WaitForAddress64From32(Core::System& system, u32 address, ArbitrationType arb_type,
                              s32 value, s64 timeout_ns) {
    R_RETURN(WaitForAddress(system, address, arb_type, value, timeout_ns));
}

Result SignalToAddress64From32(Core::System& system, u32 address, SignalType signal_type, s32 value,
                               s32 count) {
    R_RETURN(SignalToAddress(system, address, signal_type, value, count));
}

} // namespace Kernel::Svc
