// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/core.h"
#include "core/hle/kernel/k_memory_layout.h"
#include "core/hle/kernel/k_process.h"
#include "core/hle/kernel/svc.h"

namespace Kernel::Svc {

/// Attempts to locks a mutex
Result ArbitrateLock(Core::System& system, Handle thread_handle, u64 address, u32 tag) {
    LOG_TRACE(Kernel_SVC, "called thread_handle=0x{:08X}, address=0x{:X}, tag=0x{:08X}",
              thread_handle, address, tag);

    // Validate the input address.
    R_UNLESS(!IsKernelAddress(address), ResultInvalidCurrentMemory);
    R_UNLESS(Common::IsAligned(address, sizeof(u32)), ResultInvalidAddress);

    R_RETURN(KConditionVariable::WaitForAddress(system.Kernel(), thread_handle, address, tag));
}

/// Unlock a mutex
Result ArbitrateUnlock(Core::System& system, u64 address) {
    LOG_TRACE(Kernel_SVC, "called address=0x{:X}", address);

    // Validate the input address.
    R_UNLESS(!IsKernelAddress(address), ResultInvalidCurrentMemory);
    R_UNLESS(Common::IsAligned(address, sizeof(u32)), ResultInvalidAddress);

    R_RETURN(KConditionVariable::SignalToAddress(system.Kernel(), address));
}

Result ArbitrateLock64(Core::System& system, Handle thread_handle, uint64_t address, uint32_t tag) {
    R_RETURN(ArbitrateLock(system, thread_handle, address, tag));
}

Result ArbitrateUnlock64(Core::System& system, uint64_t address) {
    R_RETURN(ArbitrateUnlock(system, address));
}

Result ArbitrateLock64From32(Core::System& system, Handle thread_handle, uint32_t address,
                             uint32_t tag) {
    R_RETURN(ArbitrateLock(system, thread_handle, address, tag));
}

Result ArbitrateUnlock64From32(Core::System& system, uint32_t address) {
    R_RETURN(ArbitrateUnlock(system, address));
}

} // namespace Kernel::Svc
