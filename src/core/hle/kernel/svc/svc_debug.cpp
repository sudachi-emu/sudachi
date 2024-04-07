// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/hle/kernel/svc.h"
#include "core/hle/kernel/svc_results.h"

namespace Kernel::Svc {

Result DebugActiveProcess(Core::System& system, Handle* out_handle, uint64_t process_id) {
    UNIMPLEMENTED();
    R_THROW(ResultNotImplemented);
}

Result BreakDebugProcess(Core::System& system, Handle debug_handle) {
    UNIMPLEMENTED();
    R_THROW(ResultNotImplemented);
}

Result TerminateDebugProcess(Core::System& system, Handle debug_handle) {
    UNIMPLEMENTED();
    R_THROW(ResultNotImplemented);
}

Result GetDebugEvent(Core::System& system, uint64_t out_info, Handle debug_handle) {
    UNIMPLEMENTED();
    R_THROW(ResultNotImplemented);
}

Result ContinueDebugEvent(Core::System& system, Handle debug_handle, uint32_t flags,
                          uint64_t user_thread_ids, int32_t num_thread_ids) {
    UNIMPLEMENTED();
    R_THROW(ResultNotImplemented);
}

Result GetDebugThreadContext(Core::System& system, uint64_t out_context, Handle debug_handle,
                             uint64_t thread_id, uint32_t context_flags) {
    UNIMPLEMENTED();
    R_THROW(ResultNotImplemented);
}

Result SetDebugThreadContext(Core::System& system, Handle debug_handle, uint64_t thread_id,
                             uint64_t user_context, uint32_t context_flags) {
    UNIMPLEMENTED();
    R_THROW(ResultNotImplemented);
}

Result QueryDebugProcessMemory(Core::System& system, uint64_t out_memory_info,
                               PageInfo* out_page_info, Handle process_handle, uint64_t address) {
    UNIMPLEMENTED();
    R_THROW(ResultNotImplemented);
}

Result ReadDebugProcessMemory(Core::System& system, uint64_t buffer, Handle debug_handle,
                              uint64_t address, uint64_t size) {
    UNIMPLEMENTED();
    R_THROW(ResultNotImplemented);
}

Result WriteDebugProcessMemory(Core::System& system, Handle debug_handle, uint64_t buffer,
                               uint64_t address, uint64_t size) {
    UNIMPLEMENTED();
    R_THROW(ResultNotImplemented);
}

Result SetHardwareBreakPoint(Core::System& system, HardwareBreakPointRegisterName name,
                             uint64_t flags, uint64_t value) {
    UNIMPLEMENTED();
    R_THROW(ResultNotImplemented);
}

Result GetDebugThreadParam(Core::System& system, uint64_t* out_64, uint32_t* out_32,
                           Handle debug_handle, uint64_t thread_id, DebugThreadParam param) {
    UNIMPLEMENTED();
    R_THROW(ResultNotImplemented);
}

Result DebugActiveProcess64(Core::System& system, Handle* out_handle, uint64_t process_id) {
    R_RETURN(DebugActiveProcess(system, out_handle, process_id));
}

Result BreakDebugProcess64(Core::System& system, Handle debug_handle) {
    R_RETURN(BreakDebugProcess(system, debug_handle));
}

Result TerminateDebugProcess64(Core::System& system, Handle debug_handle) {
    R_RETURN(TerminateDebugProcess(system, debug_handle));
}

Result GetDebugEvent64(Core::System& system, uint64_t out_info, Handle debug_handle) {
    R_RETURN(GetDebugEvent(system, out_info, debug_handle));
}

Result ContinueDebugEvent64(Core::System& system, Handle debug_handle, uint32_t flags,
                            uint64_t thread_ids, int32_t num_thread_ids) {
    R_RETURN(ContinueDebugEvent(system, debug_handle, flags, thread_ids, num_thread_ids));
}

Result GetDebugThreadContext64(Core::System& system, uint64_t out_context, Handle debug_handle,
                               uint64_t thread_id, uint32_t context_flags) {
    R_RETURN(GetDebugThreadContext(system, out_context, debug_handle, thread_id, context_flags));
}

Result SetDebugThreadContext64(Core::System& system, Handle debug_handle, uint64_t thread_id,
                               uint64_t context, uint32_t context_flags) {
    R_RETURN(SetDebugThreadContext(system, debug_handle, thread_id, context, context_flags));
}

Result QueryDebugProcessMemory64(Core::System& system, uint64_t out_memory_info,
                                 PageInfo* out_page_info, Handle debug_handle, uint64_t address) {
    R_RETURN(
        QueryDebugProcessMemory(system, out_memory_info, out_page_info, debug_handle, address));
}

Result ReadDebugProcessMemory64(Core::System& system, uint64_t buffer, Handle debug_handle,
                                uint64_t address, uint64_t size) {
    R_RETURN(ReadDebugProcessMemory(system, buffer, debug_handle, address, size));
}

Result WriteDebugProcessMemory64(Core::System& system, Handle debug_handle, uint64_t buffer,
                                 uint64_t address, uint64_t size) {
    R_RETURN(WriteDebugProcessMemory(system, debug_handle, buffer, address, size));
}

Result SetHardwareBreakPoint64(Core::System& system, HardwareBreakPointRegisterName name,
                               uint64_t flags, uint64_t value) {
    R_RETURN(SetHardwareBreakPoint(system, name, flags, value));
}

Result GetDebugThreadParam64(Core::System& system, uint64_t* out_64, uint32_t* out_32,
                             Handle debug_handle, uint64_t thread_id, DebugThreadParam param) {
    R_RETURN(GetDebugThreadParam(system, out_64, out_32, debug_handle, thread_id, param));
}

Result DebugActiveProcess64From32(Core::System& system, Handle* out_handle, uint64_t process_id) {
    R_RETURN(DebugActiveProcess(system, out_handle, process_id));
}

Result BreakDebugProcess64From32(Core::System& system, Handle debug_handle) {
    R_RETURN(BreakDebugProcess(system, debug_handle));
}

Result TerminateDebugProcess64From32(Core::System& system, Handle debug_handle) {
    R_RETURN(TerminateDebugProcess(system, debug_handle));
}

Result GetDebugEvent64From32(Core::System& system, uint32_t out_info, Handle debug_handle) {
    R_RETURN(GetDebugEvent(system, out_info, debug_handle));
}

Result ContinueDebugEvent64From32(Core::System& system, Handle debug_handle, uint32_t flags,
                                  uint32_t thread_ids, int32_t num_thread_ids) {
    R_RETURN(ContinueDebugEvent(system, debug_handle, flags, thread_ids, num_thread_ids));
}

Result GetDebugThreadContext64From32(Core::System& system, uint32_t out_context,
                                     Handle debug_handle, uint64_t thread_id,
                                     uint32_t context_flags) {
    R_RETURN(GetDebugThreadContext(system, out_context, debug_handle, thread_id, context_flags));
}

Result SetDebugThreadContext64From32(Core::System& system, Handle debug_handle, uint64_t thread_id,
                                     uint32_t context, uint32_t context_flags) {
    R_RETURN(SetDebugThreadContext(system, debug_handle, thread_id, context, context_flags));
}

Result QueryDebugProcessMemory64From32(Core::System& system, uint32_t out_memory_info,
                                       PageInfo* out_page_info, Handle debug_handle,
                                       uint32_t address) {
    R_RETURN(
        QueryDebugProcessMemory(system, out_memory_info, out_page_info, debug_handle, address));
}

Result ReadDebugProcessMemory64From32(Core::System& system, uint32_t buffer, Handle debug_handle,
                                      uint32_t address, uint32_t size) {
    R_RETURN(ReadDebugProcessMemory(system, buffer, debug_handle, address, size));
}

Result WriteDebugProcessMemory64From32(Core::System& system, Handle debug_handle, uint32_t buffer,
                                       uint32_t address, uint32_t size) {
    R_RETURN(WriteDebugProcessMemory(system, debug_handle, buffer, address, size));
}

Result SetHardwareBreakPoint64From32(Core::System& system, HardwareBreakPointRegisterName name,
                                     uint64_t flags, uint64_t value) {
    R_RETURN(SetHardwareBreakPoint(system, name, flags, value));
}

Result GetDebugThreadParam64From32(Core::System& system, uint64_t* out_64, uint32_t* out_32,
                                   Handle debug_handle, uint64_t thread_id,
                                   DebugThreadParam param) {
    R_RETURN(GetDebugThreadParam(system, out_64, out_32, debug_handle, thread_id, param));
}

} // namespace Kernel::Svc
