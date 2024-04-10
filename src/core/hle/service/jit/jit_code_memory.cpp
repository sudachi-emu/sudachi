// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/hle/service/jit/jit_code_memory.h"

namespace Service::JIT {

Result CodeMemory::Initialize(Kernel::KProcess& process, Kernel::KCodeMemory& code_memory,
                              size_t size, Kernel::Svc::MemoryPermission perm,
                              std::mt19937_64& generate_random) {
    auto& page_table = process.GetPageTable();
    const u64 alias_code_start =
        GetInteger(page_table.GetAliasCodeRegionStart()) / Kernel::PageSize;
    const u64 alias_code_size = page_table.GetAliasCodeRegionSize() / Kernel::PageSize;

    // NOTE: This will retry indefinitely until mapping the code memory succeeds.
    while (true) {
        // Generate a new trial address.
        const u64 mapped_address =
            (alias_code_start + (generate_random() % alias_code_size)) * Kernel::PageSize;

        // Try to map the address
        R_TRY_CATCH(code_memory.MapToOwner(mapped_address, size, perm)) {
            R_CATCH(Kernel::ResultInvalidMemoryRegion) {
                // If we could not map here, retry.
                continue;
            }
        }
        R_END_TRY_CATCH;

        // Set members.
        m_code_memory = std::addressof(code_memory);
        m_size = size;
        m_address = mapped_address;
        m_perm = perm;

        // Open a new reference to the code memory.
        m_code_memory->Open();

        // We succeeded.
        R_SUCCEED();
    }
}

void CodeMemory::Finalize() {
    if (m_code_memory) {
        R_ASSERT(m_code_memory->UnmapFromOwner(m_address, m_size));
        m_code_memory->Close();
    }

    m_code_memory = nullptr;
}

} // namespace Service::JIT
