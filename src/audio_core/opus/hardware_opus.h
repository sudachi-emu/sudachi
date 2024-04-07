// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <mutex>
#include <opus.h>

#include "audio_core/adsp/apps/opus/opus_decoder.h"
#include "audio_core/adsp/apps/opus/shared_memory.h"
#include "audio_core/adsp/mailbox.h"
#include "core/hle/service/audio/errors.h"

namespace AudioCore::OpusDecoder {
class HardwareOpus {
public:
    HardwareOpus(Core::System& system);

    u32 GetWorkBufferSize(u32 channel);
    u32 GetWorkBufferSizeForMultiStream(u32 total_stream_count, u32 stereo_stream_count);

    Result InitializeDecodeObject(u32 sample_rate, u32 channel_count, void* buffer,
                                  u64 buffer_size);
    Result InitializeMultiStreamDecodeObject(u32 sample_rate, u32 channel_count,
                                             u32 totaL_stream_count, u32 stereo_stream_count,
                                             const void* mappings, void* buffer, u64 buffer_size);
    Result ShutdownDecodeObject(void* buffer, u64 buffer_size);
    Result ShutdownMultiStreamDecodeObject(void* buffer, u64 buffer_size);
    Result DecodeInterleaved(u32& out_sample_count, void* output_data, u64 output_data_size,
                             u32 channel_count, void* input_data, u64 input_data_size, void* buffer,
                             u64& out_time_taken, bool reset);
    Result DecodeInterleavedForMultiStream(u32& out_sample_count, void* output_data,
                                           u64 output_data_size, u32 channel_count,
                                           void* input_data, u64 input_data_size, void* buffer,
                                           u64& out_time_taken, bool reset);
    Result MapMemory(void* buffer, u64 buffer_size);
    Result UnmapMemory(void* buffer, u64 buffer_size);

private:
    Core::System& system;
    std::mutex mutex;
    ADSP::OpusDecoder::OpusDecoder& opus_decoder;
    ADSP::OpusDecoder::SharedMemory shared_memory;
};
} // namespace AudioCore::OpusDecoder
