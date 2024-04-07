// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/scope_exit.h"
#include "core/core.h"
#include "core/core_timing.h"
#include "core/hle/kernel/k_hardware_timer.h"
#include "core/hle/kernel/k_process.h"
#include "core/hle/kernel/k_scoped_resource_reservation.h"
#include "core/hle/kernel/k_thread.h"
#include "core/hle/kernel/svc.h"

namespace Kernel::Svc {
namespace {

constexpr bool IsValidVirtualCoreId(int32_t core_id) {
    return (0 <= core_id && core_id < static_cast<int32_t>(Core::Hardware::NUM_CPU_CORES));
}

} // Anonymous namespace

/// Creates a new thread
Result CreateThread(Core::System& system, Handle* out_handle, u64 entry_point, u64 arg,
                    u64 stack_bottom, s32 priority, s32 core_id) {
    LOG_DEBUG(Kernel_SVC,
              "called entry_point=0x{:08X}, arg=0x{:08X}, stack_bottom=0x{:08X}, "
              "priority=0x{:08X}, core_id=0x{:08X}",
              entry_point, arg, stack_bottom, priority, core_id);

    // Adjust core id, if it's the default magic.
    auto& kernel = system.Kernel();
    auto& process = GetCurrentProcess(kernel);
    if (core_id == IdealCoreUseProcessValue) {
        core_id = process.GetIdealCoreId();
    }

    // Validate arguments.
    R_UNLESS(IsValidVirtualCoreId(core_id), ResultInvalidCoreId);
    R_UNLESS(((1ull << core_id) & process.GetCoreMask()) != 0, ResultInvalidCoreId);

    R_UNLESS(HighestThreadPriority <= priority && priority <= LowestThreadPriority,
             ResultInvalidPriority);
    R_UNLESS(process.CheckThreadPriority(priority), ResultInvalidPriority);

    // Reserve a new thread from the process resource limit (waiting up to 100ms).
    KScopedResourceReservation thread_reservation(std::addressof(process),
                                                  LimitableResource::ThreadCountMax, 1,
                                                  kernel.HardwareTimer().GetTick() + 100000000);
    R_UNLESS(thread_reservation.Succeeded(), ResultLimitReached);

    // Create the thread.
    KThread* thread = KThread::Create(kernel);
    R_UNLESS(thread != nullptr, ResultOutOfResource)
    SCOPE_EXIT {
        thread->Close();
    };

    // Initialize the thread.
    {
        KScopedLightLock lk{process.GetStateLock()};
        R_TRY(KThread::InitializeUserThread(system, thread, entry_point, arg, stack_bottom,
                                            priority, core_id, std::addressof(process)));
    }

    // Commit the thread reservation.
    thread_reservation.Commit();

    // Clone the current fpu status to the new thread.
    thread->CloneFpuStatus();

    // Register the new thread.
    KThread::Register(kernel, thread);

    // Add the thread to the handle table.
    R_RETURN(process.GetHandleTable().Add(out_handle, thread));
}

/// Starts the thread for the provided handle
Result StartThread(Core::System& system, Handle thread_handle) {
    LOG_DEBUG(Kernel_SVC, "called thread=0x{:08X}", thread_handle);

    // Get the thread from its handle.
    KScopedAutoObject thread =
        GetCurrentProcess(system.Kernel()).GetHandleTable().GetObject<KThread>(thread_handle);
    R_UNLESS(thread.IsNotNull(), ResultInvalidHandle);

    // Try to start the thread.
    R_TRY(thread->Run());

    R_SUCCEED();
}

/// Called when a thread exits
void ExitThread(Core::System& system) {
    auto* const current_thread = GetCurrentThreadPointer(system.Kernel());
    system.GlobalSchedulerContext().RemoveThread(current_thread);
    current_thread->Exit();
}

/// Sleep the current thread
void SleepThread(Core::System& system, s64 ns) {
    auto& kernel = system.Kernel();
    const auto yield_type = static_cast<Svc::YieldType>(ns);

    LOG_TRACE(Kernel_SVC, "called nanoseconds={}", ns);

    // When the input tick is positive, sleep.
    if (ns > 0) {
        // Convert the timeout from nanoseconds to ticks.
        // NOTE: Nintendo does not use this conversion logic in WaitSynchronization...
        s64 timeout;

        const s64 offset_tick(ns);
        if (offset_tick > 0) {
            timeout = kernel.HardwareTimer().GetTick() + offset_tick + 2;
            if (timeout <= 0) {
                timeout = std::numeric_limits<s64>::max();
            }
        } else {
            timeout = std::numeric_limits<s64>::max();
        }

        // Sleep.
        // NOTE: Nintendo does not check the result of this sleep.
        static_cast<void>(GetCurrentThread(kernel).Sleep(timeout));
    } else if (yield_type == Svc::YieldType::WithoutCoreMigration) {
        KScheduler::YieldWithoutCoreMigration(kernel);
    } else if (yield_type == Svc::YieldType::WithCoreMigration) {
        KScheduler::YieldWithCoreMigration(kernel);
    } else if (yield_type == Svc::YieldType::ToAnyThread) {
        KScheduler::YieldToAnyThread(kernel);
    } else {
        // Nintendo does nothing at all if an otherwise invalid value is passed.
    }
}

/// Gets the thread context
Result GetThreadContext3(Core::System& system, u64 out_context, Handle thread_handle) {
    LOG_DEBUG(Kernel_SVC, "called, out_context=0x{:08X}, thread_handle=0x{:X}", out_context,
              thread_handle);

    auto& kernel = system.Kernel();

    // Get the thread from its handle.
    KScopedAutoObject thread =
        GetCurrentProcess(kernel).GetHandleTable().GetObject<KThread>(thread_handle);
    R_UNLESS(thread.IsNotNull(), ResultInvalidHandle);

    // Require the handle be to a non-current thread in the current process.
    R_UNLESS(thread->GetOwnerProcess() == GetCurrentProcessPointer(kernel), ResultInvalidHandle);
    R_UNLESS(thread.GetPointerUnsafe() != GetCurrentThreadPointer(kernel), ResultBusy);

    // Get the thread context.
    Svc::ThreadContext context{};
    R_TRY(thread->GetThreadContext3(std::addressof(context)));

    // Copy the thread context to user space.
    R_UNLESS(
        GetCurrentMemory(kernel).WriteBlock(out_context, std::addressof(context), sizeof(context)),
        ResultInvalidPointer);

    R_SUCCEED();
}

/// Gets the priority for the specified thread
Result GetThreadPriority(Core::System& system, s32* out_priority, Handle handle) {
    LOG_TRACE(Kernel_SVC, "called");

    // Get the thread from its handle.
    KScopedAutoObject thread =
        GetCurrentProcess(system.Kernel()).GetHandleTable().GetObject<KThread>(handle);
    R_UNLESS(thread.IsNotNull(), ResultInvalidHandle);

    // Get the thread's priority.
    *out_priority = thread->GetPriority();
    R_SUCCEED();
}

/// Sets the priority for the specified thread
Result SetThreadPriority(Core::System& system, Handle thread_handle, s32 priority) {
    // Get the current process.
    KProcess& process = GetCurrentProcess(system.Kernel());

    // Validate the priority.
    R_UNLESS(HighestThreadPriority <= priority && priority <= LowestThreadPriority,
             ResultInvalidPriority);
    R_UNLESS(process.CheckThreadPriority(priority), ResultInvalidPriority);

    // Get the thread from its handle.
    KScopedAutoObject thread = process.GetHandleTable().GetObject<KThread>(thread_handle);
    R_UNLESS(thread.IsNotNull(), ResultInvalidHandle);

    // Set the thread priority.
    thread->SetBasePriority(priority);
    R_SUCCEED();
}

Result GetThreadList(Core::System& system, s32* out_num_threads, u64 out_thread_ids,
                     s32 out_thread_ids_size, Handle debug_handle) {
    // TODO: Handle this case when debug events are supported.
    UNIMPLEMENTED_IF(debug_handle != InvalidHandle);

    LOG_DEBUG(Kernel_SVC, "called. out_thread_ids=0x{:016X}, out_thread_ids_size={}",
              out_thread_ids, out_thread_ids_size);

    // If the size is negative or larger than INT32_MAX / sizeof(u64)
    if ((out_thread_ids_size & 0xF0000000) != 0) {
        LOG_ERROR(Kernel_SVC, "Supplied size outside [0, 0x0FFFFFFF] range. size={}",
                  out_thread_ids_size);
        R_THROW(ResultOutOfRange);
    }

    auto* const current_process = GetCurrentProcessPointer(system.Kernel());
    const auto total_copy_size = out_thread_ids_size * sizeof(u64);

    if (out_thread_ids_size > 0 &&
        !current_process->GetPageTable().Contains(out_thread_ids, total_copy_size)) {
        LOG_ERROR(Kernel_SVC, "Address range outside address space. begin=0x{:016X}, end=0x{:016X}",
                  out_thread_ids, out_thread_ids + total_copy_size);
        R_THROW(ResultInvalidCurrentMemory);
    }

    auto& memory = GetCurrentMemory(system.Kernel());
    const auto& thread_list = current_process->GetThreadList();
    const auto num_threads = thread_list.size();
    const auto copy_amount = std::min(static_cast<std::size_t>(out_thread_ids_size), num_threads);

    auto list_iter = thread_list.cbegin();
    for (std::size_t i = 0; i < copy_amount; ++i, ++list_iter) {
        memory.Write64(out_thread_ids, list_iter->GetThreadId());
        out_thread_ids += sizeof(u64);
    }

    *out_num_threads = static_cast<u32>(num_threads);
    R_SUCCEED();
}

Result GetThreadCoreMask(Core::System& system, s32* out_core_id, u64* out_affinity_mask,
                         Handle thread_handle) {
    LOG_TRACE(Kernel_SVC, "called, handle=0x{:08X}", thread_handle);

    // Get the thread from its handle.
    KScopedAutoObject thread =
        GetCurrentProcess(system.Kernel()).GetHandleTable().GetObject<KThread>(thread_handle);
    R_UNLESS(thread.IsNotNull(), ResultInvalidHandle);

    // Get the core mask.
    R_RETURN(thread->GetCoreMask(out_core_id, out_affinity_mask));
}

Result SetThreadCoreMask(Core::System& system, Handle thread_handle, s32 core_id,
                         u64 affinity_mask) {
    // Determine the core id/affinity mask.
    if (core_id == IdealCoreUseProcessValue) {
        core_id = GetCurrentProcess(system.Kernel()).GetIdealCoreId();
        affinity_mask = (1ULL << core_id);
    } else {
        // Validate the affinity mask.
        const u64 process_core_mask = GetCurrentProcess(system.Kernel()).GetCoreMask();
        R_UNLESS((affinity_mask | process_core_mask) == process_core_mask, ResultInvalidCoreId);
        R_UNLESS(affinity_mask != 0, ResultInvalidCombination);

        // Validate the core id.
        if (IsValidVirtualCoreId(core_id)) {
            R_UNLESS(((1ULL << core_id) & affinity_mask) != 0, ResultInvalidCombination);
        } else {
            R_UNLESS(core_id == IdealCoreNoUpdate || core_id == IdealCoreDontCare,
                     ResultInvalidCoreId);
        }
    }

    // Get the thread from its handle.
    KScopedAutoObject thread =
        GetCurrentProcess(system.Kernel()).GetHandleTable().GetObject<KThread>(thread_handle);
    R_UNLESS(thread.IsNotNull(), ResultInvalidHandle);

    // Set the core mask.
    R_RETURN(thread->SetCoreMask(core_id, affinity_mask));
}

/// Get the ID for the specified thread.
Result GetThreadId(Core::System& system, u64* out_thread_id, Handle thread_handle) {
    // Get the thread from its handle.
    KScopedAutoObject thread =
        GetCurrentProcess(system.Kernel()).GetHandleTable().GetObject<KThread>(thread_handle);
    R_UNLESS(thread.IsNotNull(), ResultInvalidHandle);

    // Get the thread's id.
    *out_thread_id = thread->GetId();
    R_SUCCEED();
}

Result CreateThread64(Core::System& system, Handle* out_handle, uint64_t func, uint64_t arg,
                      uint64_t stack_bottom, int32_t priority, int32_t core_id) {
    R_RETURN(CreateThread(system, out_handle, func, arg, stack_bottom, priority, core_id));
}

Result StartThread64(Core::System& system, Handle thread_handle) {
    R_RETURN(StartThread(system, thread_handle));
}

void ExitThread64(Core::System& system) {
    return ExitThread(system);
}

void SleepThread64(Core::System& system, int64_t ns) {
    return SleepThread(system, ns);
}

Result GetThreadPriority64(Core::System& system, int32_t* out_priority, Handle thread_handle) {
    R_RETURN(GetThreadPriority(system, out_priority, thread_handle));
}

Result SetThreadPriority64(Core::System& system, Handle thread_handle, int32_t priority) {
    R_RETURN(SetThreadPriority(system, thread_handle, priority));
}

Result GetThreadCoreMask64(Core::System& system, int32_t* out_core_id, uint64_t* out_affinity_mask,
                           Handle thread_handle) {
    R_RETURN(GetThreadCoreMask(system, out_core_id, out_affinity_mask, thread_handle));
}

Result SetThreadCoreMask64(Core::System& system, Handle thread_handle, int32_t core_id,
                           uint64_t affinity_mask) {
    R_RETURN(SetThreadCoreMask(system, thread_handle, core_id, affinity_mask));
}

Result GetThreadId64(Core::System& system, uint64_t* out_thread_id, Handle thread_handle) {
    R_RETURN(GetThreadId(system, out_thread_id, thread_handle));
}

Result GetThreadContext364(Core::System& system, uint64_t out_context, Handle thread_handle) {
    R_RETURN(GetThreadContext3(system, out_context, thread_handle));
}

Result GetThreadList64(Core::System& system, int32_t* out_num_threads, uint64_t out_thread_ids,
                       int32_t max_out_count, Handle debug_handle) {
    R_RETURN(GetThreadList(system, out_num_threads, out_thread_ids, max_out_count, debug_handle));
}

Result CreateThread64From32(Core::System& system, Handle* out_handle, uint32_t func, uint32_t arg,
                            uint32_t stack_bottom, int32_t priority, int32_t core_id) {
    R_RETURN(CreateThread(system, out_handle, func, arg, stack_bottom, priority, core_id));
}

Result StartThread64From32(Core::System& system, Handle thread_handle) {
    R_RETURN(StartThread(system, thread_handle));
}

void ExitThread64From32(Core::System& system) {
    return ExitThread(system);
}

void SleepThread64From32(Core::System& system, int64_t ns) {
    return SleepThread(system, ns);
}

Result GetThreadPriority64From32(Core::System& system, int32_t* out_priority,
                                 Handle thread_handle) {
    R_RETURN(GetThreadPriority(system, out_priority, thread_handle));
}

Result SetThreadPriority64From32(Core::System& system, Handle thread_handle, int32_t priority) {
    R_RETURN(SetThreadPriority(system, thread_handle, priority));
}

Result GetThreadCoreMask64From32(Core::System& system, int32_t* out_core_id,
                                 uint64_t* out_affinity_mask, Handle thread_handle) {
    R_RETURN(GetThreadCoreMask(system, out_core_id, out_affinity_mask, thread_handle));
}

Result SetThreadCoreMask64From32(Core::System& system, Handle thread_handle, int32_t core_id,
                                 uint64_t affinity_mask) {
    R_RETURN(SetThreadCoreMask(system, thread_handle, core_id, affinity_mask));
}

Result GetThreadId64From32(Core::System& system, uint64_t* out_thread_id, Handle thread_handle) {
    R_RETURN(GetThreadId(system, out_thread_id, thread_handle));
}

Result GetThreadContext364From32(Core::System& system, uint32_t out_context, Handle thread_handle) {
    R_RETURN(GetThreadContext3(system, out_context, thread_handle));
}

Result GetThreadList64From32(Core::System& system, int32_t* out_num_threads,
                             uint32_t out_thread_ids, int32_t max_out_count, Handle debug_handle) {
    R_RETURN(GetThreadList(system, out_num_threads, out_thread_ids, max_out_count, debug_handle));
}

} // namespace Kernel::Svc
