// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/demangle.h"
#include "core/arm/debug.h"
#include "core/arm/symbols.h"
#include "core/hle/kernel/k_process.h"
#include "core/hle/kernel/k_thread.h"
#include "core/memory.h"

namespace Core {

namespace {

std::optional<std::string> GetNameFromThreadType64(Core::Memory::Memory& memory,
                                                   const Kernel::KThread& thread) {
    // Read thread type from TLS
    const VAddr tls_thread_type{memory.Read64(thread.GetTlsAddress() + 0x1f8)};
    const VAddr argument_thread_type{thread.GetArgument()};

    if (argument_thread_type && tls_thread_type != argument_thread_type) {
        // Probably not created by nnsdk, no name available.
        return std::nullopt;
    }

    if (!tls_thread_type) {
        return std::nullopt;
    }

    const u16 version{memory.Read16(tls_thread_type + 0x46)};
    VAddr name_pointer{};
    if (version == 1) {
        name_pointer = memory.Read64(tls_thread_type + 0x1a0);
    } else {
        name_pointer = memory.Read64(tls_thread_type + 0x1a8);
    }

    if (!name_pointer) {
        // No name provided.
        return std::nullopt;
    }

    return memory.ReadCString(name_pointer, 256);
}

std::optional<std::string> GetNameFromThreadType32(Core::Memory::Memory& memory,
                                                   const Kernel::KThread& thread) {
    // Read thread type from TLS
    const VAddr tls_thread_type{memory.Read32(thread.GetTlsAddress() + 0x1fc)};
    const VAddr argument_thread_type{thread.GetArgument()};

    if (argument_thread_type && tls_thread_type != argument_thread_type) {
        // Probably not created by nnsdk, no name available.
        return std::nullopt;
    }

    if (!tls_thread_type) {
        return std::nullopt;
    }

    const u16 version{memory.Read16(tls_thread_type + 0x26)};
    VAddr name_pointer{};
    if (version == 1) {
        name_pointer = memory.Read32(tls_thread_type + 0xe4);
    } else {
        name_pointer = memory.Read32(tls_thread_type + 0xe8);
    }

    if (!name_pointer) {
        // No name provided.
        return std::nullopt;
    }

    return memory.ReadCString(name_pointer, 256);
}

constexpr std::array<u64, 2> SegmentBases{
    0x60000000ULL,
    0x7100000000ULL,
};

void SymbolicateBacktrace(Kernel::KProcess* process, std::vector<BacktraceEntry>& out) {
    auto modules = FindModules(process);

    const bool is_64 = process->Is64Bit();

    std::map<std::string, Symbols::Symbols> symbols;
    for (const auto& module : modules) {
        symbols.insert_or_assign(module.second,
                                 Symbols::GetSymbols(module.first, process->GetMemory(), is_64));
    }

    for (auto& entry : out) {
        VAddr base = 0;
        for (auto iter = modules.rbegin(); iter != modules.rend(); ++iter) {
            const auto& module{*iter};
            if (entry.original_address >= module.first) {
                entry.module = module.second;
                base = module.first;
                break;
            }
        }

        entry.offset = entry.original_address - base;
        entry.address = SegmentBases[is_64] + entry.offset;

        if (entry.module.empty()) {
            entry.module = "unknown";
        }

        const auto symbol_set = symbols.find(entry.module);
        if (symbol_set != symbols.end()) {
            const auto symbol = Symbols::GetSymbolName(symbol_set->second, entry.offset);
            if (symbol) {
                entry.name = Common::DemangleSymbol(*symbol);
            }
        }
    }
}

std::vector<BacktraceEntry> GetAArch64Backtrace(Kernel::KProcess* process,
                                                const Kernel::Svc::ThreadContext& ctx) {
    std::vector<BacktraceEntry> out;
    auto& memory = process->GetMemory();
    auto pc = ctx.pc, lr = ctx.lr, fp = ctx.fp;

    out.push_back({"", 0, pc, 0, ""});

    // fp (= x29) points to the previous frame record.
    // Frame records are two words long:
    // fp+0 : pointer to previous frame record
    // fp+8 : value of lr for frame
    for (size_t i = 0; i < 256; i++) {
        out.push_back({"", 0, lr, 0, ""});
        if (!fp || (fp % 4 != 0) || !memory.IsValidVirtualAddressRange(fp, 16)) {
            break;
        }
        lr = memory.Read64(fp + 8);
        fp = memory.Read64(fp);
    }

    SymbolicateBacktrace(process, out);

    return out;
}

std::vector<BacktraceEntry> GetAArch32Backtrace(Kernel::KProcess* process,
                                                const Kernel::Svc::ThreadContext& ctx) {
    std::vector<BacktraceEntry> out;
    auto& memory = process->GetMemory();
    auto pc = ctx.pc, lr = ctx.lr, fp = ctx.fp;

    out.push_back({"", 0, pc, 0, ""});

    // fp (= r11) points to the last frame record.
    // Frame records are two words long:
    // fp+0 : pointer to previous frame record
    // fp+4 : value of lr for frame
    for (size_t i = 0; i < 256; i++) {
        out.push_back({"", 0, lr, 0, ""});
        if (!fp || (fp % 4 != 0) || !memory.IsValidVirtualAddressRange(fp, 8)) {
            break;
        }
        lr = memory.Read32(fp + 4);
        fp = memory.Read32(fp);
    }

    SymbolicateBacktrace(process, out);

    return out;
}

} // namespace

std::optional<std::string> GetThreadName(const Kernel::KThread* thread) {
    auto* process = thread->GetOwnerProcess();
    if (process->Is64Bit()) {
        return GetNameFromThreadType64(process->GetMemory(), *thread);
    } else {
        return GetNameFromThreadType32(process->GetMemory(), *thread);
    }
}

std::string_view GetThreadWaitReason(const Kernel::KThread* thread) {
    switch (thread->GetWaitReasonForDebugging()) {
    case Kernel::ThreadWaitReasonForDebugging::Sleep:
        return "Sleep";
    case Kernel::ThreadWaitReasonForDebugging::IPC:
        return "IPC";
    case Kernel::ThreadWaitReasonForDebugging::Synchronization:
        return "Synchronization";
    case Kernel::ThreadWaitReasonForDebugging::ConditionVar:
        return "ConditionVar";
    case Kernel::ThreadWaitReasonForDebugging::Arbitration:
        return "Arbitration";
    case Kernel::ThreadWaitReasonForDebugging::Suspended:
        return "Suspended";
    default:
        return "Unknown";
    }
}

std::string GetThreadState(const Kernel::KThread* thread) {
    switch (thread->GetState()) {
    case Kernel::ThreadState::Initialized:
        return "Initialized";
    case Kernel::ThreadState::Waiting:
        return fmt::format("Waiting ({})", GetThreadWaitReason(thread));
    case Kernel::ThreadState::Runnable:
        return "Runnable";
    case Kernel::ThreadState::Terminated:
        return "Terminated";
    default:
        return "Unknown";
    }
}

Kernel::KProcessAddress GetModuleEnd(const Kernel::KProcess* process,
                                     Kernel::KProcessAddress base) {
    Kernel::KMemoryInfo mem_info;
    Kernel::Svc::MemoryInfo svc_mem_info;
    Kernel::Svc::PageInfo page_info;
    VAddr cur_addr{GetInteger(base)};
    auto& page_table = process->GetPageTable();

    // Expect: r-x Code (.text)
    R_ASSERT(page_table.QueryInfo(std::addressof(mem_info), std::addressof(page_info), cur_addr));
    svc_mem_info = mem_info.GetSvcMemoryInfo();
    cur_addr = svc_mem_info.base_address + svc_mem_info.size;
    if (svc_mem_info.state != Kernel::Svc::MemoryState::Code ||
        svc_mem_info.permission != Kernel::Svc::MemoryPermission::ReadExecute) {
        return cur_addr - 1;
    }

    // Expect: r-- Code (.rodata)
    R_ASSERT(page_table.QueryInfo(std::addressof(mem_info), std::addressof(page_info), cur_addr));
    svc_mem_info = mem_info.GetSvcMemoryInfo();
    cur_addr = svc_mem_info.base_address + svc_mem_info.size;
    if (svc_mem_info.state != Kernel::Svc::MemoryState::Code ||
        svc_mem_info.permission != Kernel::Svc::MemoryPermission::Read) {
        return cur_addr - 1;
    }

    // Expect: rw- CodeData (.data)
    R_ASSERT(page_table.QueryInfo(std::addressof(mem_info), std::addressof(page_info), cur_addr));
    svc_mem_info = mem_info.GetSvcMemoryInfo();
    cur_addr = svc_mem_info.base_address + svc_mem_info.size;
    return cur_addr - 1;
}

Loader::AppLoader::Modules FindModules(Kernel::KProcess* process) {
    Loader::AppLoader::Modules modules;

    auto& page_table = process->GetPageTable();
    auto& memory = process->GetMemory();
    VAddr cur_addr = 0;

    // Look for executable sections in Code or AliasCode regions.
    while (true) {
        Kernel::KMemoryInfo mem_info{};
        Kernel::Svc::PageInfo page_info{};
        R_ASSERT(
            page_table.QueryInfo(std::addressof(mem_info), std::addressof(page_info), cur_addr));
        auto svc_mem_info = mem_info.GetSvcMemoryInfo();

        if (svc_mem_info.permission == Kernel::Svc::MemoryPermission::ReadExecute &&
            (svc_mem_info.state == Kernel::Svc::MemoryState::Code ||
             svc_mem_info.state == Kernel::Svc::MemoryState::AliasCode)) {
            // Try to read the module name from its path.
            constexpr s32 PathLengthMax = 0x200;
            struct {
                u32 zero;
                s32 path_length;
                std::array<char, PathLengthMax> path;
            } module_path;

            if (memory.ReadBlock(svc_mem_info.base_address + svc_mem_info.size, &module_path,
                                 sizeof(module_path))) {
                if (module_path.zero == 0 && module_path.path_length > 0) {
                    // Truncate module name.
                    module_path.path[PathLengthMax - 1] = '\0';

                    // Ignore leading directories.
                    char* path_pointer = module_path.path.data();
                    char* path_end =
                        path_pointer + std::min(PathLengthMax, module_path.path_length);

                    for (s32 i = 0; i < std::min(PathLengthMax, module_path.path_length) &&
                                    module_path.path[i] != '\0';
                         i++) {
                        if (module_path.path[i] == '/' || module_path.path[i] == '\\') {
                            path_pointer = module_path.path.data() + i + 1;
                        }
                    }

                    // Insert output.
                    modules.emplace(svc_mem_info.base_address,
                                    std::string_view(path_pointer, path_end));
                }
            }
        }

        // Check if we're done.
        const uintptr_t next_address = svc_mem_info.base_address + svc_mem_info.size;
        if (next_address <= cur_addr) {
            break;
        }

        cur_addr = next_address;
    }

    return modules;
}

Kernel::KProcessAddress FindMainModuleEntrypoint(Kernel::KProcess* process) {
    // Do we have any loaded executable sections?
    auto modules = FindModules(process);

    if (modules.size() >= 2) {
        // If we have two or more, the first one is rtld and the second is main.
        return std::next(modules.begin())->first;
    } else if (!modules.empty()) {
        // If we only have one, this is the main module.
        return modules.begin()->first;
    }

    // As a last resort, use the start of the code region.
    return GetInteger(process->GetPageTable().GetCodeRegionStart());
}

void InvalidateInstructionCacheRange(const Kernel::KProcess* process, u64 address, u64 size) {
    for (size_t i = 0; i < Core::Hardware::NUM_CPU_CORES; i++) {
        auto* interface = process->GetArmInterface(i);
        if (interface) {
            interface->InvalidateCacheRange(address, size);
        }
    }
}

std::vector<BacktraceEntry> GetBacktraceFromContext(Kernel::KProcess* process,
                                                    const Kernel::Svc::ThreadContext& ctx) {
    if (process->Is64Bit()) {
        return GetAArch64Backtrace(process, ctx);
    } else {
        return GetAArch32Backtrace(process, ctx);
    }
}

std::vector<BacktraceEntry> GetBacktrace(const Kernel::KThread* thread) {
    Kernel::Svc::ThreadContext ctx = thread->GetContext();
    return GetBacktraceFromContext(thread->GetOwnerProcess(), ctx);
}

} // namespace Core
