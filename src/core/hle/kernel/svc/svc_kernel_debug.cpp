// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/hle/kernel/svc.h"

namespace Kernel::Svc {

void KernelDebug(Core::System& system, KernelDebugType kernel_debug_type, u64 arg0, u64 arg1,
                 u64 arg2) {
    // Intentionally do nothing, as this does nothing in released kernel binaries.
}

void ChangeKernelTraceState(Core::System& system, KernelTraceState trace_state) {
    // Intentionally do nothing, as this does nothing in released kernel binaries.
}

void KernelDebug64(Core::System& system, KernelDebugType kern_debug_type, uint64_t arg0,
                   uint64_t arg1, uint64_t arg2) {
    KernelDebug(system, kern_debug_type, arg0, arg1, arg2);
}

void ChangeKernelTraceState64(Core::System& system, KernelTraceState kern_trace_state) {
    ChangeKernelTraceState(system, kern_trace_state);
}

void KernelDebug64From32(Core::System& system, KernelDebugType kern_debug_type, uint64_t arg0,
                         uint64_t arg1, uint64_t arg2) {
    KernelDebug(system, kern_debug_type, arg0, arg1, arg2);
}

void ChangeKernelTraceState64From32(Core::System& system, KernelTraceState kern_trace_state) {
    ChangeKernelTraceState(system, kern_trace_state);
}

} // namespace Kernel::Svc
