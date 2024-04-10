// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/scope_exit.h"
#include "common/scratch_buffer.h"
#include "core/core.h"
#include "core/hle/kernel/k_hardware_timer.h"
#include "core/hle/kernel/k_process.h"
#include "core/hle/kernel/k_readable_event.h"
#include "core/hle/kernel/svc.h"
#include "core/hle/kernel/svc_results.h"

namespace Kernel::Svc {

/// Close a handle
Result CloseHandle(Core::System& system, Handle handle) {
    LOG_TRACE(Kernel_SVC, "Closing handle 0x{:08X}", handle);

    // Remove the handle.
    R_UNLESS(GetCurrentProcess(system.Kernel()).GetHandleTable().Remove(handle),
             ResultInvalidHandle);

    R_SUCCEED();
}

/// Clears the signaled state of an event or process.
Result ResetSignal(Core::System& system, Handle handle) {
    LOG_DEBUG(Kernel_SVC, "called handle 0x{:08X}", handle);

    // Get the current handle table.
    const auto& handle_table = GetCurrentProcess(system.Kernel()).GetHandleTable();

    // Try to reset as readable event.
    {
        KScopedAutoObject readable_event = handle_table.GetObject<KReadableEvent>(handle);
        if (readable_event.IsNotNull()) {
            R_RETURN(readable_event->Reset());
        }
    }

    // Try to reset as process.
    {
        KScopedAutoObject process = handle_table.GetObject<KProcess>(handle);
        if (process.IsNotNull()) {
            R_RETURN(process->Reset());
        }
    }

    R_THROW(ResultInvalidHandle);
}

/// Wait for the given handles to synchronize, timeout after the specified nanoseconds
Result WaitSynchronization(Core::System& system, int32_t* out_index, u64 user_handles,
                           int32_t num_handles, int64_t timeout_ns) {
    LOG_TRACE(Kernel_SVC, "called user_handles={:#x}, num_handles={}, timeout_ns={}", user_handles,
              num_handles, timeout_ns);

    // Ensure number of handles is valid.
    R_UNLESS(0 <= num_handles && num_handles <= Svc::ArgumentHandleCountMax, ResultOutOfRange);

    // Get the synchronization context.
    auto& kernel = system.Kernel();
    auto& handle_table = GetCurrentProcess(kernel).GetHandleTable();
    auto objs = GetCurrentThread(kernel).GetSynchronizationObjectBuffer();
    auto handles = GetCurrentThread(kernel).GetHandleBuffer();

    // Copy user handles.
    if (num_handles > 0) {
        // Get the handles.
        R_UNLESS(GetCurrentMemory(kernel).ReadBlock(user_handles, handles.data(),
                                                    sizeof(Handle) * num_handles),
                 ResultInvalidPointer);

        // Convert the handles to objects.
        R_UNLESS(handle_table.GetMultipleObjects<KSynchronizationObject>(
                     objs.data(), handles.data(), num_handles),
                 ResultInvalidHandle);
    }

    // Ensure handles are closed when we're done.
    SCOPE_EXIT {
        for (auto i = 0; i < num_handles; ++i) {
            objs[i]->Close();
        }
    };

    // Convert the timeout from nanoseconds to ticks.
    s64 timeout;
    if (timeout_ns > 0) {
        u64 ticks = kernel.HardwareTimer().GetTick();
        ticks += timeout_ns;
        ticks += 2;

        timeout = ticks;
    } else {
        timeout = timeout_ns;
    }

    // Wait on the objects.
    Result res = KSynchronizationObject::Wait(kernel, out_index, objs.data(), num_handles, timeout);

    R_SUCCEED_IF(res == ResultSessionClosed);
    R_RETURN(res);
}

/// Resumes a thread waiting on WaitSynchronization
Result CancelSynchronization(Core::System& system, Handle handle) {
    LOG_TRACE(Kernel_SVC, "called handle=0x{:X}", handle);

    // Get the thread from its handle.
    KScopedAutoObject thread =
        GetCurrentProcess(system.Kernel()).GetHandleTable().GetObject<KThread>(handle);
    R_UNLESS(thread.IsNotNull(), ResultInvalidHandle);

    // Cancel the thread's wait.
    thread->WaitCancel();
    R_SUCCEED();
}

void SynchronizePreemptionState(Core::System& system) {
    auto& kernel = system.Kernel();

    // Lock the scheduler.
    KScopedSchedulerLock sl{kernel};

    // If the current thread is pinned, unpin it.
    KProcess* cur_process = GetCurrentProcessPointer(kernel);
    const auto core_id = GetCurrentCoreId(kernel);

    if (cur_process->GetPinnedThread(core_id) == GetCurrentThreadPointer(kernel)) {
        // Clear the current thread's interrupt flag.
        GetCurrentThread(kernel).ClearInterruptFlag();

        // Unpin the current thread.
        cur_process->UnpinCurrentThread();
    }
}

Result CloseHandle64(Core::System& system, Handle handle) {
    R_RETURN(CloseHandle(system, handle));
}

Result ResetSignal64(Core::System& system, Handle handle) {
    R_RETURN(ResetSignal(system, handle));
}

Result WaitSynchronization64(Core::System& system, int32_t* out_index, uint64_t handles,
                             int32_t num_handles, int64_t timeout_ns) {
    R_RETURN(WaitSynchronization(system, out_index, handles, num_handles, timeout_ns));
}

Result CancelSynchronization64(Core::System& system, Handle handle) {
    R_RETURN(CancelSynchronization(system, handle));
}

void SynchronizePreemptionState64(Core::System& system) {
    SynchronizePreemptionState(system);
}

Result CloseHandle64From32(Core::System& system, Handle handle) {
    R_RETURN(CloseHandle(system, handle));
}

Result ResetSignal64From32(Core::System& system, Handle handle) {
    R_RETURN(ResetSignal(system, handle));
}

Result WaitSynchronization64From32(Core::System& system, int32_t* out_index, uint32_t handles,
                                   int32_t num_handles, int64_t timeout_ns) {
    R_RETURN(WaitSynchronization(system, out_index, handles, num_handles, timeout_ns));
}

Result CancelSynchronization64From32(Core::System& system, Handle handle) {
    R_RETURN(CancelSynchronization(system, handle));
}

void SynchronizePreemptionState64From32(Core::System& system) {
    SynchronizePreemptionState(system);
}

} // namespace Kernel::Svc
