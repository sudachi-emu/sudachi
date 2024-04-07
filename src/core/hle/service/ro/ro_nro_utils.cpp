// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/hle/kernel/k_process.h"
#include "core/hle/service/ro/ro_nro_utils.h"
#include "core/hle/service/ro/ro_results.h"

namespace Service::RO {

namespace {

struct ProcessMemoryRegion {
    u64 address;
    u64 size;
};

size_t GetTotalProcessMemoryRegionSize(const ProcessMemoryRegion* regions, size_t num_regions) {
    size_t total = 0;

    for (size_t i = 0; i < num_regions; ++i) {
        total += regions[i].size;
    }

    return total;
}

size_t SetupNroProcessMemoryRegions(ProcessMemoryRegion* regions, u64 nro_heap_address,
                                    u64 nro_heap_size, u64 bss_heap_address, u64 bss_heap_size) {
    // Reset region count.
    size_t num_regions = 0;

    // We always want a region for the nro.
    regions[num_regions++] = {nro_heap_address, nro_heap_size};

    // If we have bss, create a region for bss.
    if (bss_heap_size > 0) {
        regions[num_regions++] = {bss_heap_address, bss_heap_size};
    }

    return num_regions;
}

Result SetProcessMemoryPermission(Kernel::KProcess* process, u64 address, u64 size,
                                  Kernel::Svc::MemoryPermission permission) {
    auto& page_table = process->GetPageTable();

    // Set permission.
    R_RETURN(page_table.SetProcessMemoryPermission(address, size, permission));
}

Result UnmapProcessCodeMemory(Kernel::KProcess* process, u64 process_code_address,
                              const ProcessMemoryRegion* regions, size_t num_regions) {
    // Get the total process memory region size.
    const size_t total_size = GetTotalProcessMemoryRegionSize(regions, num_regions);

    auto& page_table = process->GetPageTable();

    // Unmap each region in order.
    size_t cur_offset = total_size;
    for (size_t i = 0; i < num_regions; ++i) {
        // We want to unmap in reverse order.
        const auto& cur_region = regions[num_regions - 1 - i];

        // Subtract to update the current offset.
        cur_offset -= cur_region.size;

        // Unmap.
        R_TRY(page_table.UnmapCodeMemory(process_code_address + cur_offset, cur_region.address,
                                         cur_region.size));
    }

    R_SUCCEED();
}

Result EnsureGuardPages(Kernel::KProcessPageTable& page_table, u64 map_address, u64 map_size) {
    Kernel::KMemoryInfo memory_info;
    Kernel::Svc::PageInfo page_info;

    // Ensure page before mapping is unmapped.
    R_TRY(page_table.QueryInfo(std::addressof(memory_info), std::addressof(page_info),
                               map_address - 1));
    R_UNLESS(memory_info.GetSvcState() == Kernel::Svc::MemoryState::Free,
             Kernel::ResultInvalidState);

    // Ensure page after mapping is unmapped.
    R_TRY(page_table.QueryInfo(std::addressof(memory_info), std::addressof(page_info),
                               map_address + map_size));
    R_UNLESS(memory_info.GetSvcState() == Kernel::Svc::MemoryState::Free,
             Kernel::ResultInvalidState);

    // Successfully verified guard pages.
    R_SUCCEED();
}

Result MapProcessCodeMemory(u64* out, Kernel::KProcess* process, const ProcessMemoryRegion* regions,
                            size_t num_regions, std::mt19937_64& generate_random) {
    auto& page_table = process->GetPageTable();
    const u64 alias_code_start =
        GetInteger(page_table.GetAliasCodeRegionStart()) / Kernel::PageSize;
    const u64 alias_code_size = page_table.GetAliasCodeRegionSize() / Kernel::PageSize;

    for (size_t trial = 0; trial < 64; trial++) {
        // Generate a new trial address.
        const u64 mapped_address =
            (alias_code_start + (generate_random() % alias_code_size)) * Kernel::PageSize;

        const auto MapRegions = [&] {
            // Map the regions in order.
            u64 mapped_size = 0;
            for (size_t i = 0; i < num_regions; ++i) {
                // If we fail, unmap up to where we've mapped.
                ON_RESULT_FAILURE {
                    R_ASSERT(UnmapProcessCodeMemory(process, mapped_address, regions, i));
                };

                // Map the current region.
                R_TRY(page_table.MapCodeMemory(mapped_address + mapped_size, regions[i].address,
                                               regions[i].size));

                mapped_size += regions[i].size;
            }

            // If we fail, unmap all mapped regions.
            ON_RESULT_FAILURE {
                R_ASSERT(UnmapProcessCodeMemory(process, mapped_address, regions, num_regions));
            };

            // Ensure guard pages.
            R_RETURN(EnsureGuardPages(page_table, mapped_address, mapped_size));
        };

        if (R_SUCCEEDED(MapRegions())) {
            // Set the output address.
            *out = mapped_address;
            R_SUCCEED();
        }
    }

    // We failed to map anything.
    R_THROW(RO::ResultOutOfAddressSpace);
}

} // namespace

Result MapNro(u64* out_base_address, Kernel::KProcess* process, u64 nro_heap_address,
              u64 nro_heap_size, u64 bss_heap_address, u64 bss_heap_size,
              std::mt19937_64& generate_random) {
    // Set up the process memory regions.
    std::array<ProcessMemoryRegion, 2> regions{};
    const size_t num_regions = SetupNroProcessMemoryRegions(
        regions.data(), nro_heap_address, nro_heap_size, bss_heap_address, bss_heap_size);

    // Re-map the nro/bss as code memory in the destination process.
    R_RETURN(MapProcessCodeMemory(out_base_address, process, regions.data(), num_regions,
                                  generate_random));
}

Result SetNroPerms(Kernel::KProcess* process, u64 base_address, u64 rx_size, u64 ro_size,
                   u64 rw_size) {
    const u64 rx_offset = 0;
    const u64 ro_offset = rx_offset + rx_size;
    const u64 rw_offset = ro_offset + ro_size;

    R_TRY(SetProcessMemoryPermission(process, base_address + rx_offset, rx_size,
                                     Kernel::Svc::MemoryPermission::ReadExecute));
    R_TRY(SetProcessMemoryPermission(process, base_address + ro_offset, ro_size,
                                     Kernel::Svc::MemoryPermission::Read));
    R_TRY(SetProcessMemoryPermission(process, base_address + rw_offset, rw_size,
                                     Kernel::Svc::MemoryPermission::ReadWrite));

    R_SUCCEED();
}

Result UnmapNro(Kernel::KProcess* process, u64 base_address, u64 nro_heap_address,
                u64 nro_heap_size, u64 bss_heap_address, u64 bss_heap_size) {
    // Set up the process memory regions.
    std::array<ProcessMemoryRegion, 2> regions{};
    const size_t num_regions = SetupNroProcessMemoryRegions(
        regions.data(), nro_heap_address, nro_heap_size, bss_heap_address, bss_heap_size);

    // Unmap the nro/bss.
    R_RETURN(UnmapProcessCodeMemory(process, base_address, regions.data(), num_regions));
}

} // namespace Service::RO
