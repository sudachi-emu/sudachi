// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/hle/kernel/svc.h"
#include "core/hle/kernel/svc_results.h"

namespace Kernel::Svc {

Result GetDebugFutureThreadInfo(Core::System& system, lp64::LastThreadContext* out_context,
                                uint64_t* out_thread_id, Handle debug_handle, int64_t ns) {
    UNIMPLEMENTED();
    R_THROW(ResultNotImplemented);
}

Result GetLastThreadInfo(Core::System& system, lp64::LastThreadContext* out_context,
                         uint64_t* out_tls_address, uint32_t* out_flags) {
    UNIMPLEMENTED();
    R_THROW(ResultNotImplemented);
}

Result GetDebugFutureThreadInfo64(Core::System& system, lp64::LastThreadContext* out_context,
                                  uint64_t* out_thread_id, Handle debug_handle, int64_t ns) {
    R_RETURN(GetDebugFutureThreadInfo(system, out_context, out_thread_id, debug_handle, ns));
}

Result GetLastThreadInfo64(Core::System& system, lp64::LastThreadContext* out_context,
                           uint64_t* out_tls_address, uint32_t* out_flags) {
    R_RETURN(GetLastThreadInfo(system, out_context, out_tls_address, out_flags));
}

Result GetDebugFutureThreadInfo64From32(Core::System& system, ilp32::LastThreadContext* out_context,
                                        uint64_t* out_thread_id, Handle debug_handle, int64_t ns) {
    lp64::LastThreadContext context{};
    R_TRY(
        GetDebugFutureThreadInfo(system, std::addressof(context), out_thread_id, debug_handle, ns));

    *out_context = {
        .fp = static_cast<u32>(context.fp),
        .sp = static_cast<u32>(context.sp),
        .lr = static_cast<u32>(context.lr),
        .pc = static_cast<u32>(context.pc),
    };
    R_SUCCEED();
}

Result GetLastThreadInfo64From32(Core::System& system, ilp32::LastThreadContext* out_context,
                                 uint64_t* out_tls_address, uint32_t* out_flags) {
    lp64::LastThreadContext context{};
    R_TRY(GetLastThreadInfo(system, std::addressof(context), out_tls_address, out_flags));

    *out_context = {
        .fp = static_cast<u32>(context.fp),
        .sp = static_cast<u32>(context.sp),
        .lr = static_cast<u32>(context.lr),
        .pc = static_cast<u32>(context.pc),
    };
    R_SUCCEED();
}

} // namespace Kernel::Svc
