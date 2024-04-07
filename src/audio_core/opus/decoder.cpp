// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "audio_core/opus/decoder.h"
#include "audio_core/opus/hardware_opus.h"
#include "audio_core/opus/parameters.h"
#include "common/alignment.h"
#include "common/swap.h"
#include "core/core.h"

namespace AudioCore::OpusDecoder {
using namespace Service::Audio;
namespace {
OpusPacketHeader ReverseHeader(OpusPacketHeader header) {
    OpusPacketHeader out;
    out.size = Common::swap32(header.size);
    out.final_range = Common::swap32(header.final_range);
    return out;
}
} // namespace

OpusDecoder::OpusDecoder(Core::System& system_, HardwareOpus& hardware_opus_)
    : system{system_}, hardware_opus{hardware_opus_} {}

OpusDecoder::~OpusDecoder() {
    if (decode_object_initialized) {
        hardware_opus.ShutdownDecodeObject(shared_buffer.get(), shared_buffer_size);
    }
}

Result OpusDecoder::Initialize(const OpusParametersEx& params,
                               Kernel::KTransferMemory* transfer_memory, u64 transfer_memory_size) {
    auto frame_size{params.use_large_frame_size ? 5760 : 1920};
    shared_buffer_size = transfer_memory_size;
    shared_buffer = std::make_unique<u8[]>(shared_buffer_size);
    shared_memory_mapped = true;

    buffer_size =
        Common::AlignUp((frame_size * params.channel_count) / (48'000 / params.sample_rate), 16);

    out_data = {shared_buffer.get() + shared_buffer_size - buffer_size, buffer_size};
    size_t in_data_size{0x600u};
    in_data = {out_data.data() - in_data_size, in_data_size};

    ON_RESULT_FAILURE {
        if (shared_memory_mapped) {
            shared_memory_mapped = false;
            ASSERT(R_SUCCEEDED(hardware_opus.UnmapMemory(shared_buffer.get(), shared_buffer_size)));
        }
    };

    R_TRY(hardware_opus.InitializeDecodeObject(params.sample_rate, params.channel_count,
                                               shared_buffer.get(), shared_buffer_size));

    sample_rate = params.sample_rate;
    channel_count = params.channel_count;
    use_large_frame_size = params.use_large_frame_size;
    decode_object_initialized = true;
    R_SUCCEED();
}

Result OpusDecoder::Initialize(const OpusMultiStreamParametersEx& params,
                               Kernel::KTransferMemory* transfer_memory, u64 transfer_memory_size) {
    auto frame_size{params.use_large_frame_size ? 5760 : 1920};
    shared_buffer_size = transfer_memory_size;
    shared_buffer = std::make_unique<u8[]>(shared_buffer_size);
    shared_memory_mapped = true;

    buffer_size =
        Common::AlignUp((frame_size * params.channel_count) / (48'000 / params.sample_rate), 16);

    out_data = {shared_buffer.get() + shared_buffer_size - buffer_size, buffer_size};
    size_t in_data_size{Common::AlignUp(1500ull * params.total_stream_count, 64u)};
    in_data = {out_data.data() - in_data_size, in_data_size};

    ON_RESULT_FAILURE {
        if (shared_memory_mapped) {
            shared_memory_mapped = false;
            ASSERT(R_SUCCEEDED(hardware_opus.UnmapMemory(shared_buffer.get(), shared_buffer_size)));
        }
    };

    R_TRY(hardware_opus.InitializeMultiStreamDecodeObject(
        params.sample_rate, params.channel_count, params.total_stream_count,
        params.stereo_stream_count, params.mappings.data(), shared_buffer.get(),
        shared_buffer_size));

    sample_rate = params.sample_rate;
    channel_count = params.channel_count;
    total_stream_count = params.total_stream_count;
    stereo_stream_count = params.stereo_stream_count;
    use_large_frame_size = params.use_large_frame_size;
    decode_object_initialized = true;
    R_SUCCEED();
}

Result OpusDecoder::DecodeInterleaved(u32* out_data_size, u64* out_time_taken,
                                      u32* out_sample_count, std::span<const u8> input_data,
                                      std::span<u8> output_data, bool reset) {
    u32 out_samples;
    u64 time_taken{};

    R_UNLESS(input_data.size_bytes() > sizeof(OpusPacketHeader), ResultInputDataTooSmall);

    auto* header_p{reinterpret_cast<const OpusPacketHeader*>(input_data.data())};
    OpusPacketHeader header{ReverseHeader(*header_p)};

    R_UNLESS(in_data.size_bytes() >= header.size &&
                 header.size + sizeof(OpusPacketHeader) <= input_data.size_bytes(),
             ResultBufferTooSmall);

    if (!shared_memory_mapped) {
        R_TRY(hardware_opus.MapMemory(shared_buffer.get(), shared_buffer_size));
        shared_memory_mapped = true;
    }

    std::memcpy(in_data.data(), input_data.data() + sizeof(OpusPacketHeader), header.size);

    R_TRY(hardware_opus.DecodeInterleaved(out_samples, out_data.data(), out_data.size_bytes(),
                                          channel_count, in_data.data(), header.size,
                                          shared_buffer.get(), time_taken, reset));

    std::memcpy(output_data.data(), out_data.data(), out_samples * channel_count * sizeof(s16));

    *out_data_size = header.size + sizeof(OpusPacketHeader);
    *out_sample_count = out_samples;
    if (out_time_taken) {
        *out_time_taken = time_taken / 1000;
    }
    R_SUCCEED();
}

Result OpusDecoder::SetContext([[maybe_unused]] std::span<const u8> context) {
    R_SUCCEED_IF(shared_memory_mapped);
    shared_memory_mapped = true;
    R_RETURN(hardware_opus.MapMemory(shared_buffer.get(), shared_buffer_size));
}

Result OpusDecoder::DecodeInterleavedForMultiStream(u32* out_data_size, u64* out_time_taken,
                                                    u32* out_sample_count,
                                                    std::span<const u8> input_data,
                                                    std::span<u8> output_data, bool reset) {
    u32 out_samples;
    u64 time_taken{};

    R_UNLESS(input_data.size_bytes() > sizeof(OpusPacketHeader), ResultInputDataTooSmall);

    auto* header_p{reinterpret_cast<const OpusPacketHeader*>(input_data.data())};
    OpusPacketHeader header{ReverseHeader(*header_p)};

    LOG_TRACE(Service_Audio, "header size 0x{:X} input data size 0x{:X} in_data size 0x{:X}",
              header.size, input_data.size_bytes(), in_data.size_bytes());

    R_UNLESS(in_data.size_bytes() >= header.size &&
                 header.size + sizeof(OpusPacketHeader) <= input_data.size_bytes(),
             ResultBufferTooSmall);

    if (!shared_memory_mapped) {
        R_TRY(hardware_opus.MapMemory(shared_buffer.get(), shared_buffer_size));
        shared_memory_mapped = true;
    }

    std::memcpy(in_data.data(), input_data.data() + sizeof(OpusPacketHeader), header.size);

    R_TRY(hardware_opus.DecodeInterleavedForMultiStream(
        out_samples, out_data.data(), out_data.size_bytes(), channel_count, in_data.data(),
        header.size, shared_buffer.get(), time_taken, reset));

    std::memcpy(output_data.data(), out_data.data(), out_samples * channel_count * sizeof(s16));

    *out_data_size = header.size + sizeof(OpusPacketHeader);
    *out_sample_count = out_samples;
    if (out_time_taken) {
        *out_time_taken = time_taken / 1000;
    }
    R_SUCCEED();
}

} // namespace AudioCore::OpusDecoder
