# SPDX-FileCopyrightText: Copyright 2023 sudachi Emulator Project
# SPDX-License-Identifier: GPL-2.0-or-later

# Raw SVC definitions from the kernel.
#
# Avoid modifying the prototypes; see below for how to customize generation
# for a given typename.
SVCS = [
     [0x01, "Result SetHeapSize(Address* out_address, Size size);"],
     [0x02, "Result SetMemoryPermission(Address address, Size size, MemoryPermission perm);"],
     [0x03, "Result SetMemoryAttribute(Address address, Size size, uint32_t mask, uint32_t attr);"],
     [0x04, "Result MapMemory(Address dst_address, Address src_address, Size size);"],
     [0x05, "Result UnmapMemory(Address dst_address, Address src_address, Size size);"],
     [0x06, "Result QueryMemory(Address out_memory_info, PageInfo* out_page_info, Address address);"],
     [0x07, "void ExitProcess();"],
     [0x08, "Result CreateThread(Handle* out_handle, ThreadFunc func, Address arg, Address stack_bottom, int32_t priority, int32_t core_id);"],
     [0x09, "Result StartThread(Handle thread_handle);"],
     [0x0A, "void ExitThread();"],
     [0x0B, "void SleepThread(int64_t ns);"],
     [0x0C, "Result GetThreadPriority(int32_t* out_priority, Handle thread_handle);"],
     [0x0D, "Result SetThreadPriority(Handle thread_handle, int32_t priority);"],
     [0x0E, "Result GetThreadCoreMask(int32_t* out_core_id, uint64_t* out_affinity_mask, Handle thread_handle);"],
     [0x0F, "Result SetThreadCoreMask(Handle thread_handle, int32_t core_id, uint64_t affinity_mask);"],
     [0x10, "int32_t GetCurrentProcessorNumber();"],
     [0x11, "Result SignalEvent(Handle event_handle);"],
     [0x12, "Result ClearEvent(Handle event_handle);"],
     [0x13, "Result MapSharedMemory(Handle shmem_handle, Address address, Size size, MemoryPermission map_perm);"],
     [0x14, "Result UnmapSharedMemory(Handle shmem_handle, Address address, Size size);"],
     [0x15, "Result CreateTransferMemory(Handle* out_handle, Address address, Size size, MemoryPermission map_perm);"],
     [0x16, "Result CloseHandle(Handle handle);"],
     [0x17, "Result ResetSignal(Handle handle);"],
     [0x18, "Result WaitSynchronization(int32_t* out_index, Address handles, int32_t num_handles, int64_t timeout_ns);"],
     [0x19, "Result CancelSynchronization(Handle handle);"],
     [0x1A, "Result ArbitrateLock(Handle thread_handle, Address address, uint32_t tag);"],
     [0x1B, "Result ArbitrateUnlock(Address address);"],
     [0x1C, "Result WaitProcessWideKeyAtomic(Address address, Address cv_key, uint32_t tag, int64_t timeout_ns);"],
     [0x1D, "void SignalProcessWideKey(Address cv_key, int32_t count);"],
     [0x1E, "int64_t GetSystemTick();"],
     [0x1F, "Result ConnectToNamedPort(Handle* out_handle, Address name);"],
     [0x20, "Result SendSyncRequestLight(Handle session_handle);"],
     [0x21, "Result SendSyncRequest(Handle session_handle);"],
     [0x22, "Result SendSyncRequestWithUserBuffer(Address message_buffer, Size message_buffer_size, Handle session_handle);"],
     [0x23, "Result SendAsyncRequestWithUserBuffer(Handle* out_event_handle, Address message_buffer, Size message_buffer_size, Handle session_handle);"],
     [0x24, "Result GetProcessId(uint64_t* out_process_id, Handle process_handle);"],
     [0x25, "Result GetThreadId(uint64_t* out_thread_id, Handle thread_handle);"],
     [0x26, "void Break(BreakReason break_reason, Address arg, Size size);"],
     [0x27, "Result OutputDebugString(Address debug_str, Size len);"],
     [0x28, "void ReturnFromException(Result result);"],
     [0x29, "Result GetInfo(uint64_t* out, InfoType info_type, Handle handle, uint64_t info_subtype);"],
     [0x2A, "void FlushEntireDataCache();"],
     [0x2B, "Result FlushDataCache(Address address, Size size);"],
     [0x2C, "Result MapPhysicalMemory(Address address, Size size);"],
     [0x2D, "Result UnmapPhysicalMemory(Address address, Size size);"],
     [0x2E, "Result GetDebugFutureThreadInfo(LastThreadContext* out_context, uint64_t* out_thread_id, Handle debug_handle, int64_t ns);"],
     [0x2F, "Result GetLastThreadInfo(LastThreadContext* out_context, Address* out_tls_address, uint32_t* out_flags);"],
     [0x30, "Result GetResourceLimitLimitValue(int64_t* out_limit_value, Handle resource_limit_handle, LimitableResource which);"],
     [0x31, "Result GetResourceLimitCurrentValue(int64_t* out_current_value, Handle resource_limit_handle, LimitableResource which);"],
     [0x32, "Result SetThreadActivity(Handle thread_handle, ThreadActivity thread_activity);"],
     [0x33, "Result GetThreadContext3(Address out_context, Handle thread_handle);"],
     [0x34, "Result WaitForAddress(Address address, ArbitrationType arb_type, int32_t value, int64_t timeout_ns);"],
     [0x35, "Result SignalToAddress(Address address, SignalType signal_type, int32_t value, int32_t count);"],
     [0x36, "void SynchronizePreemptionState();"],
     [0x37, "Result GetResourceLimitPeakValue(int64_t* out_peak_value, Handle resource_limit_handle, LimitableResource which);"],

     [0x39, "Result CreateIoPool(Handle* out_handle, IoPoolType which);"],
     [0x3A, "Result CreateIoRegion(Handle* out_handle, Handle io_pool, PhysicalAddress physical_address, Size size, MemoryMapping mapping, MemoryPermission perm);"],

     [0x3C, "void KernelDebug(KernelDebugType kern_debug_type, uint64_t arg0, uint64_t arg1, uint64_t arg2);"],
     [0x3D, "void ChangeKernelTraceState(KernelTraceState kern_trace_state);"],

     [0x40, "Result CreateSession(Handle* out_server_session_handle, Handle* out_client_session_handle, bool is_light, Address name);"],
     [0x41, "Result AcceptSession(Handle* out_handle, Handle port);"],
     [0x42, "Result ReplyAndReceiveLight(Handle handle);"],
     [0x43, "Result ReplyAndReceive(int32_t* out_index, Address handles, int32_t num_handles, Handle reply_target, int64_t timeout_ns);"],
     [0x44, "Result ReplyAndReceiveWithUserBuffer(int32_t* out_index, Address message_buffer, Size message_buffer_size, Address handles, int32_t num_handles, Handle reply_target, int64_t timeout_ns);"],
     [0x45, "Result CreateEvent(Handle* out_write_handle, Handle* out_read_handle);"],
     [0x46, "Result MapIoRegion(Handle io_region, Address address, Size size, MemoryPermission perm);"],
     [0x47, "Result UnmapIoRegion(Handle io_region, Address address, Size size);"],
     [0x48, "Result MapPhysicalMemoryUnsafe(Address address, Size size);"],
     [0x49, "Result UnmapPhysicalMemoryUnsafe(Address address, Size size);"],
     [0x4A, "Result SetUnsafeLimit(Size limit);"],
     [0x4B, "Result CreateCodeMemory(Handle* out_handle, Address address, Size size);"],
     [0x4C, "Result ControlCodeMemory(Handle code_memory_handle, CodeMemoryOperation operation, uint64_t address, uint64_t size, MemoryPermission perm);"],
     [0x4D, "void SleepSystem();"],
     [0x4E, "Result ReadWriteRegister(uint32_t* out_value, PhysicalAddress address, uint32_t mask, uint32_t value);"],
     [0x4F, "Result SetProcessActivity(Handle process_handle, ProcessActivity process_activity);"],
     [0x50, "Result CreateSharedMemory(Handle* out_handle, Size size, MemoryPermission owner_perm, MemoryPermission remote_perm);"],
     [0x51, "Result MapTransferMemory(Handle trmem_handle, Address address, Size size, MemoryPermission owner_perm);"],
     [0x52, "Result UnmapTransferMemory(Handle trmem_handle, Address address, Size size);"],
     [0x53, "Result CreateInterruptEvent(Handle* out_read_handle, int32_t interrupt_id, InterruptType interrupt_type);"],
     [0x54, "Result QueryPhysicalAddress(PhysicalMemoryInfo* out_info, Address address);"],
     [0x55, "Result QueryIoMapping(Address* out_address, Size* out_size, PhysicalAddress physical_address, Size size);"],
     [0x56, "Result CreateDeviceAddressSpace(Handle* out_handle, uint64_t das_address, uint64_t das_size);"],
     [0x57, "Result AttachDeviceAddressSpace(DeviceName device_name, Handle das_handle);"],
     [0x58, "Result DetachDeviceAddressSpace(DeviceName device_name, Handle das_handle);"],
     [0x59, "Result MapDeviceAddressSpaceByForce(Handle das_handle, Handle process_handle, uint64_t process_address, Size size, uint64_t device_address, uint32_t option);"],
     [0x5A, "Result MapDeviceAddressSpaceAligned(Handle das_handle, Handle process_handle, uint64_t process_address, Size size, uint64_t device_address, uint32_t option);"],
     [0x5C, "Result UnmapDeviceAddressSpace(Handle das_handle, Handle process_handle, uint64_t process_address, Size size, uint64_t device_address);"],
     [0x5D, "Result InvalidateProcessDataCache(Handle process_handle, uint64_t address, uint64_t size);"],
     [0x5E, "Result StoreProcessDataCache(Handle process_handle, uint64_t address, uint64_t size);"],
     [0x5F, "Result FlushProcessDataCache(Handle process_handle, uint64_t address, uint64_t size);"],
     [0x60, "Result DebugActiveProcess(Handle* out_handle, uint64_t process_id);"],
     [0x61, "Result BreakDebugProcess(Handle debug_handle);"],
     [0x62, "Result TerminateDebugProcess(Handle debug_handle);"],
     [0x63, "Result GetDebugEvent(Address out_info, Handle debug_handle);"],
     [0x64, "Result ContinueDebugEvent(Handle debug_handle, uint32_t flags, Address thread_ids, int32_t num_thread_ids);"],
     [0x65, "Result GetProcessList(int32_t* out_num_processes, Address out_process_ids, int32_t max_out_count);"],
     [0x66, "Result GetThreadList(int32_t* out_num_threads, Address out_thread_ids, int32_t max_out_count, Handle debug_handle);"],
     [0x67, "Result GetDebugThreadContext(Address out_context, Handle debug_handle, uint64_t thread_id, uint32_t context_flags);"],
     [0x68, "Result SetDebugThreadContext(Handle debug_handle, uint64_t thread_id, Address context, uint32_t context_flags);"],
     [0x69, "Result QueryDebugProcessMemory(Address out_memory_info, PageInfo* out_page_info, Handle process_handle, Address address);"],
     [0x6A, "Result ReadDebugProcessMemory(Address buffer, Handle debug_handle, Address address, Size size);"],
     [0x6B, "Result WriteDebugProcessMemory(Handle debug_handle, Address buffer, Address address, Size size);"],
     [0x6C, "Result SetHardwareBreakPoint(HardwareBreakPointRegisterName name, uint64_t flags, uint64_t value);"],
     [0x6D, "Result GetDebugThreadParam(uint64_t* out_64, uint32_t* out_32, Handle debug_handle, uint64_t thread_id, DebugThreadParam param);"],

     [0x6F, "Result GetSystemInfo(uint64_t* out, SystemInfoType info_type, Handle handle, uint64_t info_subtype);"],
     [0x70, "Result CreatePort(Handle* out_server_handle, Handle* out_client_handle, int32_t max_sessions, bool is_light, Address name);"],
     [0x71, "Result ManageNamedPort(Handle* out_server_handle, Address name, int32_t max_sessions);"],
     [0x72, "Result ConnectToPort(Handle* out_handle, Handle port);"],
     [0x73, "Result SetProcessMemoryPermission(Handle process_handle, uint64_t address, uint64_t size, MemoryPermission perm);"],
     [0x74, "Result MapProcessMemory(Address dst_address, Handle process_handle, uint64_t src_address, Size size);"],
     [0x75, "Result UnmapProcessMemory(Address dst_address, Handle process_handle, uint64_t src_address, Size size);"],
     [0x76, "Result QueryProcessMemory(Address out_memory_info, PageInfo* out_page_info, Handle process_handle, uint64_t address);"],
     [0x77, "Result MapProcessCodeMemory(Handle process_handle, uint64_t dst_address, uint64_t src_address, uint64_t size);"],
     [0x78, "Result UnmapProcessCodeMemory(Handle process_handle, uint64_t dst_address, uint64_t src_address, uint64_t size);"],
     [0x79, "Result CreateProcess(Handle* out_handle, Address parameters, Address caps, int32_t num_caps);"],
     [0x7A, "Result StartProcess(Handle process_handle, int32_t priority, int32_t core_id, uint64_t main_thread_stack_size);"],
     [0x7B, "Result TerminateProcess(Handle process_handle);"],
     [0x7C, "Result GetProcessInfo(int64_t* out_info, Handle process_handle, ProcessInfoType info_type);"],
     [0x7D, "Result CreateResourceLimit(Handle* out_handle);"],
     [0x7E, "Result SetResourceLimitLimitValue(Handle resource_limit_handle, LimitableResource which, int64_t limit_value);"],
     [0x7F, "void CallSecureMonitor(SecureMonitorArguments args);"],

     [0x90, "Result MapInsecureMemory(Address address, Size size);"],
     [0x91, "Result UnmapInsecureMemory(Address address, Size size);"],
]

# These use a custom ABI, and therefore require custom wrappers
SKIP_WRAPPERS = {
    0x20: "SendSyncRequestLight",
    0x42: "ReplyAndReceiveLight",
    0x7F: "CallSecureMonitor",
}

BIT_32 = 0
BIT_64 = 1

REG_SIZES = [4, 8]
SUFFIX_NAMES = ["64From32", "64"]
TYPE_SIZES = {
    # SVC types
    "ArbitrationType": 4,
    "BreakReason": 4,
    "CodeMemoryOperation": 4,
    "DebugThreadParam": 4,
    "DeviceName": 4,
    "HardwareBreakPointRegisterName": 4,
    "Handle": 4,
    "InfoType": 4,
    "InterruptType": 4,
    "IoPoolType": 4,
    "KernelDebugType": 4,
    "KernelTraceState": 4,
    "LimitableResource": 4,
    "MemoryMapping": 4,
    "MemoryPermission": 4,
    "PageInfo": 4,
    "ProcessActivity": 4,
    "ProcessInfoType": 4,
    "Result": 4,
    "SignalType": 4,
    "SystemInfoType": 4,
    "ThreadActivity": 4,

    # Arch-specific types
    "ilp32::LastThreadContext": 16,
    "ilp32::PhysicalMemoryInfo": 16,
    "ilp32::SecureMonitorArguments": 32,
    "lp64::LastThreadContext": 32,
    "lp64::PhysicalMemoryInfo": 24,
    "lp64::SecureMonitorArguments": 64,

    # Generic types
    "bool": 1,
    "int32_t": 4,
    "int64_t": 8,
    "uint32_t": 4,
    "uint64_t": 8,
    "void": 0,
}

TYPE_REPLACEMENTS = {
    "Address": ["uint32_t", "uint64_t"],
    "LastThreadContext": ["ilp32::LastThreadContext", "lp64::LastThreadContext"],
    "PhysicalAddress": ["uint64_t", "uint64_t"],
    "PhysicalMemoryInfo": ["ilp32::PhysicalMemoryInfo", "lp64::PhysicalMemoryInfo"],
    "SecureMonitorArguments": ["ilp32::SecureMonitorArguments", "lp64::SecureMonitorArguments"],
    "Size": ["uint32_t", "uint64_t"],
    "ThreadFunc": ["uint32_t", "uint64_t"],
}

# Statically verify that the hardcoded sizes match the intended
# sizes in C++.
def emit_size_check():
    lines = []

    for type, size in TYPE_SIZES.items():
        if type != "void":
            lines.append(f"static_assert(sizeof({type}) == {size});")

    return "\n".join(lines)


# Replaces a type with an arch-specific one, if it exists.
def substitute_type(name, bitness):
    if name in TYPE_REPLACEMENTS:
        return TYPE_REPLACEMENTS[name][bitness]
    else:
        return name


class Argument:
    def __init__(self, type_name, var_name, is_output, is_outptr, is_address):
        self.type_name = type_name
        self.var_name = var_name
        self.is_output = is_output
        self.is_outptr = is_outptr
        self.is_address = is_address


# Parses C-style string declarations for SVCs.
def parse_declaration(declaration, bitness):
    return_type, rest = declaration.split(" ", 1)
    func_name, rest = rest.split("(", 1)
    arg_names, rest = rest.split(")", 1)
    argument_types = []

    return_type = substitute_type(return_type, bitness)
    assert return_type in TYPE_SIZES, f"Unknown type '{return_type}'"

    if arg_names:
        for arg_name in arg_names.split(", "):
            type_name, var_name = arg_name.replace("*", "").split(" ", 1)

            # All outputs must contain out_ in the name.
            is_output = var_name == "out" or var_name.find("out_") != -1

            # User-pointer outputs are not written to registers.
            is_outptr = is_output and arg_name.find("*") == -1

            # Special handling is performed for output addresses to avoid awkwardness
            # in conversion for the 32-bit equivalents.
            is_address = is_output and not is_outptr and \
                type_name in ["Address", "Size"]
            type_name = substitute_type(type_name, bitness)

            assert type_name in TYPE_SIZES, f"Unknown type '{type_name}'"

            argument_types.append(
                Argument(type_name, var_name, is_output, is_outptr, is_address))

    return (return_type, func_name, argument_types)


class RegisterAllocator:
    def __init__(self, num_regs, byte_size, parameter_count):
        self.registers = {}
        self.num_regs = num_regs
        self.byte_size = byte_size
        self.parameter_count = parameter_count

    # Mark the given register as allocated, for use in layout
    # calculation if the NGRN exceeds the ABI parameter count.
    def allocate(self, i):
        assert i not in self.registers, f"Register R{i} already allocated"
        self.registers[i] = True
        return i

    # Calculate the next available location for a register;
    # the NGRN has exceeded the ABI parameter count.
    def allocate_first_free(self):
        for i in range(0, self.num_regs):
            if i in self.registers:
                continue

            self.allocate(i)
            return i

        assert False, "No registers available"

    # Add a single register at the given NGRN.
    # If the index exceeds the ABI parameter count, try to find a
    # location to add it. Returns the output location and increment.
    def add_single(self, ngrn):
        if ngrn >= self.parameter_count:
            return (self.allocate_first_free(), 0)
        else:
            return (self.allocate(ngrn), 1)

    # Add registers at the given NGRN for a data type of
    # the given size. Returns the output locations and increment.
    def add(self, ngrn, data_size, align=True):
        if data_size <= self.byte_size:
            r, i = self.add_single(ngrn)
            return ([r], i)

        regs = []
        inc = ngrn % 2 if align else 0
        remaining_size = data_size
        while remaining_size > 0:
            r, i = self.add_single(ngrn + inc)
            regs.append(r)
            inc += i
            remaining_size -= self.byte_size

        return (regs, inc)


def reg_alloc(bitness):
    if bitness == 0:
        # aapcs32: 4 4-byte registers
        return RegisterAllocator(8, 4, 4)
    elif bitness == 1:
        # aapcs64: 8 8-byte registers
        return RegisterAllocator(8, 8, 8)


# Converts a parsed SVC declaration into register lists for
# the return value, outputs, and inputs.
def get_registers(parse_result, bitness):
    output_alloc = reg_alloc(bitness)
    input_alloc = reg_alloc(bitness)
    return_type, _, arguments = parse_result

    return_write = []
    output_writes = []
    input_reads = []

    input_ngrn = 0
    output_ngrn = 0

    # Run the input calculation.
    for arg in arguments:
        if arg.is_output and not arg.is_outptr:
            input_ngrn += 1
            continue

        regs, increment = input_alloc.add(
            input_ngrn, TYPE_SIZES[arg.type_name], align=True)
        input_reads.append([arg.type_name, arg.var_name, regs])
        input_ngrn += increment

    # Include the return value if this SVC returns a value.
    if return_type != "void":
        regs, increment = output_alloc.add(
            output_ngrn, TYPE_SIZES[return_type], align=False)
        return_write.append([return_type, regs])
        output_ngrn += increment

    # Run the output calculation.
    for arg in arguments:
        if not arg.is_output or arg.is_outptr:
            continue

        regs, increment = output_alloc.add(
            output_ngrn, TYPE_SIZES[arg.type_name], align=False)
        output_writes.append(
            [arg.type_name, arg.var_name, regs, arg.is_address])
        output_ngrn += increment

    return (return_write, output_writes, input_reads)


# Collects possibly multiple source registers into the named C++ value.
def emit_gather(sources, name, type_name, reg_size):
    get_fn = f"GetArg{reg_size*8}"

    if len(sources) == 1:
        s, = sources
        line = f"{name} = Convert<{type_name}>({get_fn}(args, {s}));"
        return [line]

    var_type = f"std::array<uint{reg_size*8}_t, {len(sources)}>"
    lines = [
        f"{var_type} {name}_gather{{}};"
    ]
    for i in range(0, len(sources)):
        lines.append(
            f"{name}_gather[{i}] = {get_fn}(args, {sources[i]});")

    lines.append(f"{name} = Convert<{type_name}>({name}_gather);")
    return lines


# Produces one or more statements which assign the named C++ value
# into possibly multiple registers.
def emit_scatter(destinations, name, reg_size):
    set_fn = f"SetArg{reg_size*8}"
    reg_type = f"uint{reg_size*8}_t"

    if len(destinations) == 1:
        d, = destinations
        line = f"{set_fn}(args, {d}, Convert<{reg_type}>({name}));"
        return [line]

    var_type = f"std::array<{reg_type}, {len(destinations)}>"
    lines = [
        f"auto {name}_scatter = Convert<{var_type}>({name});"
    ]

    for i in range(0, len(destinations)):
        lines.append(
            f"{set_fn}(args, {destinations[i]}, {name}_scatter[{i}]);")

    return lines


def emit_lines(lines, indent='    '):
    output_lines = []
    first = True
    for line in lines:
        if line and not first:
            output_lines.append(indent + line)
        else:
            output_lines.append(line)
        first = False

    return "\n".join(output_lines)


# Emit a C++ function to wrap a guest SVC.
def emit_wrapper(wrapped_fn, suffix, register_info, arguments, byte_size):
    return_write, output_writes, input_reads = register_info
    lines = [
        f"static void SvcWrap_{wrapped_fn}{suffix}(Core::System& system, std::span<uint64_t, 8> args) {{"
    ]

    # Get everything ready.
    for return_type, _ in return_write:
        lines.append(f"{return_type} ret{{}};")
    if return_write:
        lines.append("")

    for output_type, var_name, _, is_address in output_writes:
        output_type = "uint64_t" if is_address else output_type
        lines.append(f"{output_type} {var_name}{{}};")
    for input_type, var_name, _ in input_reads:
        lines.append(f"{input_type} {var_name}{{}};")

    if output_writes or input_reads:
        lines.append("")

    for input_type, var_name, sources in input_reads:
        lines += emit_gather(sources, var_name, input_type, byte_size)
    if input_reads:
        lines.append("")

    # Build the call.
    call_arguments = ["system"]
    for arg in arguments:
        if arg.is_output and not arg.is_outptr:
            call_arguments.append(f"std::addressof({arg.var_name})")
        else:
            call_arguments.append(arg.var_name)

    line = ""
    if return_write:
        line += "ret = "

    line += f"{wrapped_fn}{suffix}({', '.join(call_arguments)});"
    lines.append(line)

    if return_write or output_writes:
        lines.append("")

    # Write back the return value and outputs.
    for _, destinations in return_write:
        lines += emit_scatter(destinations, "ret", byte_size)
    for _, var_name, destinations, _ in output_writes:
        lines += emit_scatter(destinations, var_name, byte_size)

    # Finish.
    return emit_lines(lines) + "\n}"


COPYRIGHT = """\
// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

// This file is automatically generated using svc_generator.py.
"""

PROLOGUE_H = """
#pragma once

namespace Core {
class System;
}

#include <span>

#include "common/common_types.h"
#include "core/hle/kernel/svc_types.h"
#include "core/hle/result.h"

namespace Kernel::Svc {

// clang-format off
"""

EPILOGUE_H = """
// clang-format on

// Custom ABI.
Result ReplyAndReceiveLight(Core::System& system, Handle handle, uint32_t* args);
Result ReplyAndReceiveLight64From32(Core::System& system, Handle handle, uint32_t* args);
Result ReplyAndReceiveLight64(Core::System& system, Handle handle, uint32_t* args);

Result SendSyncRequestLight(Core::System& system, Handle session_handle, uint32_t* args);
Result SendSyncRequestLight64From32(Core::System& system, Handle session_handle, uint32_t* args);
Result SendSyncRequestLight64(Core::System& system, Handle session_handle, uint32_t* args);

void CallSecureMonitor(Core::System& system, lp64::SecureMonitorArguments* args);
void CallSecureMonitor64From32(Core::System& system, ilp32::SecureMonitorArguments* args);
void CallSecureMonitor64(Core::System& system, lp64::SecureMonitorArguments* args);

// Defined in svc_light_ipc.cpp.
void SvcWrap_ReplyAndReceiveLight64From32(Core::System& system, std::span<uint64_t, 8> args);
void SvcWrap_ReplyAndReceiveLight64(Core::System& system, std::span<uint64_t, 8> args);

void SvcWrap_SendSyncRequestLight64From32(Core::System& system, std::span<uint64_t, 8> args);
void SvcWrap_SendSyncRequestLight64(Core::System& system, std::span<uint64_t, 8> args);

// Defined in svc_secure_monitor_call.cpp.
void SvcWrap_CallSecureMonitor64From32(Core::System& system, std::span<uint64_t, 8> args);
void SvcWrap_CallSecureMonitor64(Core::System& system, std::span<uint64_t, 8> args);

// Perform a supervisor call by index.
void Call(Core::System& system, u32 imm);

} // namespace Kernel::Svc
"""

PROLOGUE_CPP = """
#include <type_traits>

#include "core/arm/arm_interface.h"
#include "core/core.h"
#include "core/hle/kernel/k_process.h"
#include "core/hle/kernel/svc.h"

namespace Kernel::Svc {

static uint32_t GetArg32(std::span<uint64_t, 8> args, int n) {
    return static_cast<uint32_t>(args[n]);
}

static void SetArg32(std::span<uint64_t, 8> args, int n, uint32_t result) {
    args[n] = result;
}

static uint64_t GetArg64(std::span<uint64_t, 8> args, int n) {
    return args[n];
}

static void SetArg64(std::span<uint64_t, 8> args, int n, uint64_t result) {
    args[n] = result;
}

// Like bit_cast, but handles the case when the source and dest
// are differently-sized.
template <typename To, typename From>
    requires(std::is_trivial_v<To> && std::is_trivially_copyable_v<From>)
static To Convert(const From& from) {
    To to{};

    if constexpr (sizeof(To) >= sizeof(From)) {
        std::memcpy(std::addressof(to), std::addressof(from), sizeof(From));
    } else {
        std::memcpy(std::addressof(to), std::addressof(from), sizeof(To));
    }

    return to;
}

// clang-format off
"""

EPILOGUE_CPP = """
// clang-format on

void Call(Core::System& system, u32 imm) {
    auto& kernel = system.Kernel();
    auto& process = GetCurrentProcess(kernel);

    std::array<uint64_t, 8> args;
    kernel.CurrentPhysicalCore().SaveSvcArguments(process, args);
    kernel.EnterSVCProfile();

    if (process.Is64Bit()) {
        Call64(system, imm, args);
    } else {
        Call32(system, imm, args);
    }

    kernel.ExitSVCProfile();
    kernel.CurrentPhysicalCore().LoadSvcArguments(process, args);
}

} // namespace Kernel::Svc
"""


def emit_call(bitness, names, suffix):
    bit_size = REG_SIZES[bitness]*8
    indent = "    "
    lines = [
        f"static void Call{bit_size}(Core::System& system, u32 imm, std::span<uint64_t, 8> args) {{",
        f"{indent}switch (static_cast<SvcId>(imm)) {{"
    ]

    for _, name in names:
        lines.append(f"{indent}case SvcId::{name}:")
        lines.append(f"{indent*2}return SvcWrap_{name}{suffix}(system, args);")

    lines.append(f"{indent}default:")
    lines.append(
        f"{indent*2}LOG_CRITICAL(Kernel_SVC, \"Unknown SVC {{:x}}!\", imm);")
    lines.append(f"{indent*2}break;")
    lines.append(f"{indent}}}")
    lines.append("}")

    return "\n".join(lines)


def build_fn_declaration(return_type, name, arguments):
    arg_list = ["Core::System& system"]
    for arg in arguments:
        type_name = "uint64_t" if arg.is_address else arg.type_name
        pointer = "*" if arg.is_output and not arg.is_outptr else ""
        arg_list.append(f"{type_name}{pointer} {arg.var_name}")

    return f"{return_type} {name}({', '.join(arg_list)});"


def build_enum_declarations():
    lines = ["enum class SvcId : u32 {"]
    indent = "    "

    for imm, decl in SVCS:
        _, name, _ = parse_declaration(decl, BIT_64)
        lines.append(f"{indent}{name} = {hex(imm)},")

    lines.append("};")
    return "\n".join(lines)


def main():
    arch_fw_declarations = [[], []]
    svc_fw_declarations = []
    wrapper_fns = []
    names = []

    for imm, decl in SVCS:
        return_type, name, arguments = parse_declaration(decl, BIT_64)

        if imm not in SKIP_WRAPPERS:
            svc_fw_declarations.append(
                build_fn_declaration(return_type, name, arguments))

        names.append([imm, name])

    for bitness in range(2):
        byte_size = REG_SIZES[bitness]
        suffix = SUFFIX_NAMES[bitness]

        for imm, decl in SVCS:
            if imm in SKIP_WRAPPERS:
                continue

            parse_result = parse_declaration(decl, bitness)
            return_type, name, arguments = parse_result

            register_info = get_registers(parse_result, bitness)
            wrapper_fns.append(
                emit_wrapper(name, suffix, register_info, arguments, byte_size))
            arch_fw_declarations[bitness].append(
                build_fn_declaration(return_type, name + suffix, arguments))

    call_32 = emit_call(BIT_32, names, SUFFIX_NAMES[BIT_32])
    call_64 = emit_call(BIT_64, names, SUFFIX_NAMES[BIT_64])
    enum_decls = build_enum_declarations()

    with open("svc.h", "w") as f:
        f.write(COPYRIGHT)
        f.write(PROLOGUE_H)
        f.write("\n".join(svc_fw_declarations))
        f.write("\n\n")
        f.write("\n".join(arch_fw_declarations[BIT_32]))
        f.write("\n\n")
        f.write("\n".join(arch_fw_declarations[BIT_64]))
        f.write("\n\n")
        f.write(enum_decls)
        f.write(EPILOGUE_H)

    with open("svc.cpp", "w") as f:
        f.write(COPYRIGHT)
        f.write(PROLOGUE_CPP)
        f.write(emit_size_check())
        f.write("\n\n")
        f.write("\n\n".join(wrapper_fns))
        f.write("\n\n")
        f.write(call_32)
        f.write("\n\n")
        f.write(call_64)
        f.write(EPILOGUE_CPP)

    print(f"Done (emitted {len(names)} definitions)")


if __name__ == "__main__":
    main()
