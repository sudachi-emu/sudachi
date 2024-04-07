// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/core.h"
#include "core/hle/kernel/k_process.h"
#include "core/hle/kernel/k_thread.h"
#include "core/hle/kernel/svc.h"
#include "core/hle/kernel/svc_results.h"

namespace Kernel::Svc {

/// Sets the thread activity
Result SetThreadActivity(Core::System& system, Handle thread_handle,
                         ThreadActivity thread_activity) {
    LOG_DEBUG(Kernel_SVC, "called, handle=0x{:08X}, activity=0x{:08X}", thread_handle,
              thread_activity);

    // Validate the activity.
    static constexpr auto IsValidThreadActivity = [](ThreadActivity activity) {
        return activity == ThreadActivity::Runnable || activity == ThreadActivity::Paused;
    };
    R_UNLESS(IsValidThreadActivity(thread_activity), ResultInvalidEnumValue);

    // Get the thread from its handle.
    KScopedAutoObject thread =
        GetCurrentProcess(system.Kernel()).GetHandleTable().GetObject<KThread>(thread_handle);
    R_UNLESS(thread.IsNotNull(), ResultInvalidHandle);

    // Check that the activity is being set on a non-current thread for the current process.
    R_UNLESS(thread->GetOwnerProcess() == GetCurrentProcessPointer(system.Kernel()),
             ResultInvalidHandle);
    R_UNLESS(thread.GetPointerUnsafe() != GetCurrentThreadPointer(system.Kernel()), ResultBusy);

    // Set the activity.
    R_TRY(thread->SetActivity(thread_activity));

    return ResultSuccess;
}

Result SetProcessActivity(Core::System& system, Handle process_handle,
                          ProcessActivity process_activity) {
    UNIMPLEMENTED();
    R_THROW(ResultNotImplemented);
}

Result SetThreadActivity64(Core::System& system, Handle thread_handle,
                           ThreadActivity thread_activity) {
    return SetThreadActivity(system, thread_handle, thread_activity);
}

Result SetProcessActivity64(Core::System& system, Handle process_handle,
                            ProcessActivity process_activity) {
    return SetProcessActivity(system, process_handle, process_activity);
}

Result SetThreadActivity64From32(Core::System& system, Handle thread_handle,
                                 ThreadActivity thread_activity) {
    return SetThreadActivity(system, thread_handle, thread_activity);
}

Result SetProcessActivity64From32(Core::System& system, Handle process_handle,
                                  ProcessActivity process_activity) {
    return SetProcessActivity(system, process_handle, process_activity);
}

} // namespace Kernel::Svc
