// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/core.h"
#include "core/hle/kernel/k_process.h"
#include "core/hle/kernel/svc.h"

namespace Kernel::Svc {

/// Exits the current process
void ExitProcess(Core::System& system) {
    auto* current_process = GetCurrentProcessPointer(system.Kernel());

    LOG_INFO(Kernel_SVC, "Process {} exiting", current_process->GetProcessId());
    ASSERT_MSG(current_process->GetState() == KProcess::State::Running,
               "Process has already exited");

    system.Exit();
}

/// Gets the ID of the specified process or a specified thread's owning process.
Result GetProcessId(Core::System& system, u64* out_process_id, Handle handle) {
    LOG_DEBUG(Kernel_SVC, "called handle=0x{:08X}", handle);

    // Get the object from the handle table.
    KScopedAutoObject obj = GetCurrentProcess(system.Kernel())
                                .GetHandleTable()
                                .GetObject<KAutoObject>(static_cast<Handle>(handle));
    R_UNLESS(obj.IsNotNull(), ResultInvalidHandle);

    // Get the process from the object.
    KProcess* process = nullptr;
    if (KProcess* p = obj->DynamicCast<KProcess*>(); p != nullptr) {
        // The object is a process, so we can use it directly.
        process = p;
    } else if (KThread* t = obj->DynamicCast<KThread*>(); t != nullptr) {
        // The object is a thread, so we want to use its parent.
        process = reinterpret_cast<KThread*>(obj.GetPointerUnsafe())->GetOwnerProcess();
    } else {
        // TODO(bunnei): This should also handle debug objects before returning.
        UNIMPLEMENTED_MSG("Debug objects not implemented");
    }

    // Make sure the target process exists.
    R_UNLESS(process != nullptr, ResultInvalidHandle);

    // Get the process id.
    *out_process_id = process->GetId();

    R_SUCCEED();
}

Result GetProcessList(Core::System& system, s32* out_num_processes, u64 out_process_ids,
                      int32_t out_process_ids_size) {
    LOG_DEBUG(Kernel_SVC, "called. out_process_ids=0x{:016X}, out_process_ids_size={}",
              out_process_ids, out_process_ids_size);

    // If the supplied size is negative or greater than INT32_MAX / sizeof(u64), bail.
    if ((out_process_ids_size & 0xF0000000) != 0) {
        LOG_ERROR(Kernel_SVC,
                  "Supplied size outside [0, 0x0FFFFFFF] range. out_process_ids_size={}",
                  out_process_ids_size);
        R_THROW(ResultOutOfRange);
    }

    auto& kernel = system.Kernel();
    const auto total_copy_size = out_process_ids_size * sizeof(u64);

    if (out_process_ids_size > 0 &&
        !GetCurrentProcess(kernel).GetPageTable().Contains(out_process_ids, total_copy_size)) {
        LOG_ERROR(Kernel_SVC, "Address range outside address space. begin=0x{:016X}, end=0x{:016X}",
                  out_process_ids, out_process_ids + total_copy_size);
        R_THROW(ResultInvalidCurrentMemory);
    }

    auto& memory = GetCurrentMemory(kernel);
    auto process_list = kernel.GetProcessList();
    auto it = process_list.begin();

    const auto num_processes = process_list.size();
    const auto copy_amount =
        std::min(static_cast<std::size_t>(out_process_ids_size), num_processes);

    for (std::size_t i = 0; i < copy_amount && it != process_list.end(); ++i, ++it) {
        memory.Write64(out_process_ids, (*it)->GetProcessId());
        out_process_ids += sizeof(u64);
    }

    *out_num_processes = static_cast<u32>(num_processes);
    R_SUCCEED();
}

Result GetProcessInfo(Core::System& system, s64* out, Handle process_handle,
                      ProcessInfoType info_type) {
    LOG_DEBUG(Kernel_SVC, "called, handle=0x{:08X}, type=0x{:X}", process_handle, info_type);

    const auto& handle_table = GetCurrentProcess(system.Kernel()).GetHandleTable();
    KScopedAutoObject process = handle_table.GetObject<KProcess>(process_handle);
    if (process.IsNull()) {
        LOG_ERROR(Kernel_SVC, "Process handle does not exist, process_handle=0x{:08X}",
                  process_handle);
        R_THROW(ResultInvalidHandle);
    }

    if (info_type != ProcessInfoType::ProcessState) {
        LOG_ERROR(Kernel_SVC, "Expected info_type to be ProcessState but got {} instead",
                  info_type);
        R_THROW(ResultInvalidEnumValue);
    }

    *out = static_cast<s64>(process->GetState());
    R_SUCCEED();
}

Result CreateProcess(Core::System& system, Handle* out_handle, uint64_t parameters, uint64_t caps,
                     int32_t num_caps) {
    UNIMPLEMENTED();
    R_THROW(ResultNotImplemented);
}

Result StartProcess(Core::System& system, Handle process_handle, int32_t priority, int32_t core_id,
                    uint64_t main_thread_stack_size) {
    UNIMPLEMENTED();
    R_THROW(ResultNotImplemented);
}

Result TerminateProcess(Core::System& system, Handle process_handle) {
    UNIMPLEMENTED();
    R_THROW(ResultNotImplemented);
}

void ExitProcess64(Core::System& system) {
    ExitProcess(system);
}

Result GetProcessId64(Core::System& system, uint64_t* out_process_id, Handle process_handle) {
    R_RETURN(GetProcessId(system, out_process_id, process_handle));
}

Result GetProcessList64(Core::System& system, int32_t* out_num_processes, uint64_t out_process_ids,
                        int32_t max_out_count) {
    R_RETURN(GetProcessList(system, out_num_processes, out_process_ids, max_out_count));
}

Result CreateProcess64(Core::System& system, Handle* out_handle, uint64_t parameters, uint64_t caps,
                       int32_t num_caps) {
    R_RETURN(CreateProcess(system, out_handle, parameters, caps, num_caps));
}

Result StartProcess64(Core::System& system, Handle process_handle, int32_t priority,
                      int32_t core_id, uint64_t main_thread_stack_size) {
    R_RETURN(StartProcess(system, process_handle, priority, core_id, main_thread_stack_size));
}

Result TerminateProcess64(Core::System& system, Handle process_handle) {
    R_RETURN(TerminateProcess(system, process_handle));
}

Result GetProcessInfo64(Core::System& system, int64_t* out_info, Handle process_handle,
                        ProcessInfoType info_type) {
    R_RETURN(GetProcessInfo(system, out_info, process_handle, info_type));
}

void ExitProcess64From32(Core::System& system) {
    ExitProcess(system);
}

Result GetProcessId64From32(Core::System& system, uint64_t* out_process_id, Handle process_handle) {
    R_RETURN(GetProcessId(system, out_process_id, process_handle));
}

Result GetProcessList64From32(Core::System& system, int32_t* out_num_processes,
                              uint32_t out_process_ids, int32_t max_out_count) {
    R_RETURN(GetProcessList(system, out_num_processes, out_process_ids, max_out_count));
}

Result CreateProcess64From32(Core::System& system, Handle* out_handle, uint32_t parameters,
                             uint32_t caps, int32_t num_caps) {
    R_RETURN(CreateProcess(system, out_handle, parameters, caps, num_caps));
}

Result StartProcess64From32(Core::System& system, Handle process_handle, int32_t priority,
                            int32_t core_id, uint64_t main_thread_stack_size) {
    R_RETURN(StartProcess(system, process_handle, priority, core_id, main_thread_stack_size));
}

Result TerminateProcess64From32(Core::System& system, Handle process_handle) {
    R_RETURN(TerminateProcess(system, process_handle));
}

Result GetProcessInfo64From32(Core::System& system, int64_t* out_info, Handle process_handle,
                              ProcessInfoType info_type) {
    R_RETURN(GetProcessInfo(system, out_info, process_handle, info_type));
}

} // namespace Kernel::Svc
