// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "common/common_funcs.h"
#include "common/common_types.h"

namespace AudioCore::ADSP::OpusDecoder {

struct SharedMemory {
    std::array<u8, 0x100> channel_mapping{};
    std::array<u64, 16> host_send_data{};
    std::array<u64, 16> dsp_return_data{};
};

} // namespace AudioCore::ADSP::OpusDecoder
