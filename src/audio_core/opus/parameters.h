// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "common/common_funcs.h"
#include "common/common_types.h"

namespace AudioCore::OpusDecoder {
constexpr size_t OpusStreamCountMax = 255;
constexpr size_t MaxChannels = 2;

struct OpusParameters {
    /* 0x00 */ u32 sample_rate;
    /* 0x04 */ u32 channel_count;
}; // size = 0x8
static_assert(sizeof(OpusParameters) == 0x8, "OpusParameters has the wrong size!");

struct OpusParametersEx {
    /* 0x00 */ u32 sample_rate;
    /* 0x04 */ u32 channel_count;
    /* 0x08 */ bool use_large_frame_size;
    /* 0x09 */ INSERT_PADDING_BYTES_NOINIT(7);
}; // size = 0x10
static_assert(sizeof(OpusParametersEx) == 0x10, "OpusParametersEx has the wrong size!");

struct OpusMultiStreamParameters {
    /* 0x00 */ u32 sample_rate;
    /* 0x04 */ u32 channel_count;
    /* 0x08 */ u32 total_stream_count;
    /* 0x0C */ u32 stereo_stream_count;
    /* 0x10 */ std::array<u8, OpusStreamCountMax + 1> mappings;
}; // size = 0x110
static_assert(sizeof(OpusMultiStreamParameters) == 0x110,
              "OpusMultiStreamParameters has the wrong size!");

struct OpusMultiStreamParametersEx {
    /* 0x00 */ u32 sample_rate;
    /* 0x04 */ u32 channel_count;
    /* 0x08 */ u32 total_stream_count;
    /* 0x0C */ u32 stereo_stream_count;
    /* 0x10 */ bool use_large_frame_size;
    /* 0x11 */ INSERT_PADDING_BYTES_NOINIT(7);
    /* 0x18 */ std::array<u8, OpusStreamCountMax + 1> mappings;
}; // size = 0x118
static_assert(sizeof(OpusMultiStreamParametersEx) == 0x118,
              "OpusMultiStreamParametersEx has the wrong size!");

struct OpusPacketHeader {
    /* 0x00 */ u32 size;
    /* 0x04 */ u32 final_range;
}; // size = 0x8
static_assert(sizeof(OpusPacketHeader) == 0x8, "OpusPacketHeader has the wrong size!");
} // namespace AudioCore::OpusDecoder
