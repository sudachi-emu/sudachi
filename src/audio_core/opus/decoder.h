// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <span>

#include "audio_core/opus/parameters.h"
#include "common/common_types.h"
#include "core/hle/kernel/k_transfer_memory.h"
#include "core/hle/service/audio/errors.h"

namespace Core {
class System;
}

namespace AudioCore::OpusDecoder {
class HardwareOpus;

class OpusDecoder {
public:
    explicit OpusDecoder(Core::System& system, HardwareOpus& hardware_opus_);
    ~OpusDecoder();

    Result Initialize(const OpusParametersEx& params, Kernel::KTransferMemory* transfer_memory,
                      u64 transfer_memory_size);
    Result Initialize(const OpusMultiStreamParametersEx& params,
                      Kernel::KTransferMemory* transfer_memory, u64 transfer_memory_size);
    Result DecodeInterleaved(u32* out_data_size, u64* out_time_taken, u32* out_sample_count,
                             std::span<const u8> input_data, std::span<u8> output_data, bool reset);
    Result SetContext([[maybe_unused]] std::span<const u8> context);
    Result DecodeInterleavedForMultiStream(u32* out_data_size, u64* out_time_taken,
                                           u32* out_sample_count, std::span<const u8> input_data,
                                           std::span<u8> output_data, bool reset);

private:
    Core::System& system;
    HardwareOpus& hardware_opus;
    std::unique_ptr<u8[]> shared_buffer{};
    u64 shared_buffer_size;
    std::span<u8> in_data{};
    std::span<u8> out_data{};
    u64 buffer_size{};
    s32 sample_rate{};
    s32 channel_count{};
    bool use_large_frame_size{false};
    s32 total_stream_count{};
    s32 stereo_stream_count{};
    bool shared_memory_mapped{false};
    bool decode_object_initialized{false};
};

} // namespace AudioCore::OpusDecoder
