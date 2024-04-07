// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <opus.h>

#include "common/common_types.h"

namespace AudioCore::ADSP::OpusDecoder {
using LibOpusDecoder = ::OpusDecoder;
static constexpr u32 DecodeObjectMagic = 0xDEADBEEF;

class OpusDecodeObject {
public:
    static u32 GetWorkBufferSize(u32 channel_count);
    static OpusDecodeObject& Initialize(u64 buffer, u64 buffer2);

    s32 InitializeDecoder(u32 sample_rate, u32 channel_count);
    s32 Shutdown();
    s32 ResetDecoder();
    s32 Decode(u32& out_sample_count, u64 output_data, u64 output_data_size, u64 input_data,
               u64 input_data_size);
    u32 GetFinalRange() const noexcept {
        return final_range;
    }

private:
    u32 magic;
    bool initialized;
    bool state_valid;
    OpusDecodeObject* self;
    u32 final_range;
    LibOpusDecoder* decoder;
};
static_assert(std::is_trivially_constructible_v<OpusDecodeObject>);

} // namespace AudioCore::ADSP::OpusDecoder
