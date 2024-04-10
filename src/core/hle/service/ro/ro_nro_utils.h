// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <random>

#include "common/common_types.h"

namespace Kernel {
class KProcess;
}

union Result;

namespace Service::RO {

Result MapNro(u64* out_base_address, Kernel::KProcess* process, u64 nro_heap_address,
              u64 nro_heap_size, u64 bss_heap_address, u64 bss_heap_size,
              std::mt19937_64& generate_random);
Result SetNroPerms(Kernel::KProcess* process, u64 base_address, u64 rx_size, u64 ro_size,
                   u64 rw_size);
Result UnmapNro(Kernel::KProcess* process, u64 base_address, u64 nro_heap_address,
                u64 nro_heap_size, u64 bss_heap_address, u64 bss_heap_size);

} // namespace Service::RO
