// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/core.h"
#include "core/hle/kernel/k_process.h"
#include "core/hle/kernel/svc.h"

namespace Kernel::Svc {

Result QueryMemory(Core::System& system, uint64_t out_memory_info, PageInfo* out_page_info,
                   u64 query_address) {
    LOG_TRACE(Kernel_SVC,
              "called, out_memory_info=0x{:016X}, "
              "query_address=0x{:016X}",
              out_memory_info, query_address);

    // Query memory is just QueryProcessMemory on the current process.
    R_RETURN(
        QueryProcessMemory(system, out_memory_info, out_page_info, CurrentProcess, query_address));
}

Result QueryProcessMemory(Core::System& system, uint64_t out_memory_info, PageInfo* out_page_info,
                          Handle process_handle, uint64_t address) {
    LOG_TRACE(Kernel_SVC, "called process=0x{:08X} address={:X}", process_handle, address);
    const auto& handle_table = GetCurrentProcess(system.Kernel()).GetHandleTable();
    KScopedAutoObject process = handle_table.GetObject<KProcess>(process_handle);
    if (process.IsNull()) {
        LOG_ERROR(Kernel_SVC, "Process handle does not exist, process_handle=0x{:08X}",
                  process_handle);
        R_THROW(ResultInvalidHandle);
    }

    auto& current_memory{GetCurrentMemory(system.Kernel())};

    KMemoryInfo mem_info;
    R_TRY(process->GetPageTable().QueryInfo(std::addressof(mem_info), out_page_info, address));

    const auto svc_mem_info = mem_info.GetSvcMemoryInfo();
    current_memory.WriteBlock(out_memory_info, std::addressof(svc_mem_info), sizeof(svc_mem_info));

    R_SUCCEED();
}

Result QueryMemory64(Core::System& system, uint64_t out_memory_info, PageInfo* out_page_info,
                     uint64_t address) {
    R_RETURN(QueryMemory(system, out_memory_info, out_page_info, address));
}

Result QueryProcessMemory64(Core::System& system, uint64_t out_memory_info, PageInfo* out_page_info,
                            Handle process_handle, uint64_t address) {
    R_RETURN(QueryProcessMemory(system, out_memory_info, out_page_info, process_handle, address));
}

Result QueryMemory64From32(Core::System& system, uint32_t out_memory_info, PageInfo* out_page_info,
                           uint32_t address) {
    R_RETURN(QueryMemory(system, out_memory_info, out_page_info, address));
}

Result QueryProcessMemory64From32(Core::System& system, uint32_t out_memory_info,
                                  PageInfo* out_page_info, Handle process_handle,
                                  uint64_t address) {
    R_RETURN(QueryProcessMemory(system, out_memory_info, out_page_info, process_handle, address));
}

} // namespace Kernel::Svc
