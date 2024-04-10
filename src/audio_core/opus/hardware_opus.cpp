// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <array>

#include "audio_core/audio_core.h"
#include "audio_core/opus/hardware_opus.h"
#include "core/core.h"

namespace AudioCore::OpusDecoder {
namespace {
using namespace Service::Audio;

static constexpr Result ResultCodeFromLibOpusErrorCode(u64 error_code) {
    s32 error{static_cast<s32>(error_code)};
    ASSERT(error <= OPUS_OK);
    switch (error) {
    case OPUS_ALLOC_FAIL:
        R_THROW(ResultLibOpusAllocFail);
    case OPUS_INVALID_STATE:
        R_THROW(ResultLibOpusInvalidState);
    case OPUS_UNIMPLEMENTED:
        R_THROW(ResultLibOpusUnimplemented);
    case OPUS_INVALID_PACKET:
        R_THROW(ResultLibOpusInvalidPacket);
    case OPUS_INTERNAL_ERROR:
        R_THROW(ResultLibOpusInternalError);
    case OPUS_BUFFER_TOO_SMALL:
        R_THROW(ResultBufferTooSmall);
    case OPUS_BAD_ARG:
        R_THROW(ResultLibOpusBadArg);
    case OPUS_OK:
        R_RETURN(ResultSuccess);
    }
    UNREACHABLE();
}

} // namespace

HardwareOpus::HardwareOpus(Core::System& system_)
    : system{system_}, opus_decoder{system.AudioCore().ADSP().OpusDecoder()} {
    opus_decoder.SetSharedMemory(shared_memory);
}

u32 HardwareOpus::GetWorkBufferSize(u32 channel) {
    if (!opus_decoder.IsRunning()) {
        return 0;
    }
    std::scoped_lock l{mutex};
    shared_memory.host_send_data[0] = channel;
    opus_decoder.Send(ADSP::Direction::DSP, ADSP::OpusDecoder::Message::GetWorkBufferSize);
    auto msg = opus_decoder.Receive(ADSP::Direction::Host);
    if (msg != ADSP::OpusDecoder::Message::GetWorkBufferSizeOK) {
        LOG_ERROR(Service_Audio, "OpusDecoder returned invalid message. Expected {} got {}",
                  ADSP::OpusDecoder::Message::GetWorkBufferSizeOK, msg);
        return 0;
    }
    return static_cast<u32>(shared_memory.dsp_return_data[0]);
}

u32 HardwareOpus::GetWorkBufferSizeForMultiStream(u32 total_stream_count, u32 stereo_stream_count) {
    std::scoped_lock l{mutex};
    shared_memory.host_send_data[0] = total_stream_count;
    shared_memory.host_send_data[1] = stereo_stream_count;
    opus_decoder.Send(ADSP::Direction::DSP,
                      ADSP::OpusDecoder::Message::GetWorkBufferSizeForMultiStream);
    auto msg = opus_decoder.Receive(ADSP::Direction::Host);
    if (msg != ADSP::OpusDecoder::Message::GetWorkBufferSizeForMultiStreamOK) {
        LOG_ERROR(Service_Audio, "OpusDecoder returned invalid message. Expected {} got {}",
                  ADSP::OpusDecoder::Message::GetWorkBufferSizeForMultiStreamOK, msg);
        return 0;
    }
    return static_cast<u32>(shared_memory.dsp_return_data[0]);
}

Result HardwareOpus::InitializeDecodeObject(u32 sample_rate, u32 channel_count, void* buffer,
                                            u64 buffer_size) {
    std::scoped_lock l{mutex};
    shared_memory.host_send_data[0] = (u64)buffer;
    shared_memory.host_send_data[1] = buffer_size;
    shared_memory.host_send_data[2] = sample_rate;
    shared_memory.host_send_data[3] = channel_count;

    opus_decoder.Send(ADSP::Direction::DSP, ADSP::OpusDecoder::Message::InitializeDecodeObject);
    auto msg = opus_decoder.Receive(ADSP::Direction::Host);
    if (msg != ADSP::OpusDecoder::Message::InitializeDecodeObjectOK) {
        LOG_ERROR(Service_Audio, "OpusDecoder returned invalid message. Expected {} got {}",
                  ADSP::OpusDecoder::Message::InitializeDecodeObjectOK, msg);
        R_THROW(ResultInvalidOpusDSPReturnCode);
    }

    R_RETURN(ResultCodeFromLibOpusErrorCode(shared_memory.dsp_return_data[0]));
}

Result HardwareOpus::InitializeMultiStreamDecodeObject(u32 sample_rate, u32 channel_count,
                                                       u32 total_stream_count,
                                                       u32 stereo_stream_count,
                                                       const void* mappings, void* buffer,
                                                       u64 buffer_size) {
    std::scoped_lock l{mutex};
    shared_memory.host_send_data[0] = (u64)buffer;
    shared_memory.host_send_data[1] = buffer_size;
    shared_memory.host_send_data[2] = sample_rate;
    shared_memory.host_send_data[3] = channel_count;
    shared_memory.host_send_data[4] = total_stream_count;
    shared_memory.host_send_data[5] = stereo_stream_count;

    ASSERT(channel_count <= MaxChannels);
    std::memcpy(shared_memory.channel_mapping.data(), mappings, channel_count * sizeof(u8));

    opus_decoder.Send(ADSP::Direction::DSP,
                      ADSP::OpusDecoder::Message::InitializeMultiStreamDecodeObject);
    auto msg = opus_decoder.Receive(ADSP::Direction::Host);
    if (msg != ADSP::OpusDecoder::Message::InitializeMultiStreamDecodeObjectOK) {
        LOG_ERROR(Service_Audio, "OpusDecoder returned invalid message. Expected {} got {}",
                  ADSP::OpusDecoder::Message::InitializeMultiStreamDecodeObjectOK, msg);
        R_THROW(ResultInvalidOpusDSPReturnCode);
    }

    R_RETURN(ResultCodeFromLibOpusErrorCode(shared_memory.dsp_return_data[0]));
}

Result HardwareOpus::ShutdownDecodeObject(void* buffer, u64 buffer_size) {
    std::scoped_lock l{mutex};
    shared_memory.host_send_data[0] = (u64)buffer;
    shared_memory.host_send_data[1] = buffer_size;

    opus_decoder.Send(ADSP::Direction::DSP, ADSP::OpusDecoder::Message::ShutdownDecodeObject);
    auto msg = opus_decoder.Receive(ADSP::Direction::Host);
    ASSERT_MSG(msg == ADSP::OpusDecoder::Message::ShutdownDecodeObjectOK,
               "Expected Opus shutdown code {}, got {}",
               ADSP::OpusDecoder::Message::ShutdownDecodeObjectOK, msg);

    R_RETURN(ResultCodeFromLibOpusErrorCode(shared_memory.dsp_return_data[0]));
}

Result HardwareOpus::ShutdownMultiStreamDecodeObject(void* buffer, u64 buffer_size) {
    std::scoped_lock l{mutex};
    shared_memory.host_send_data[0] = (u64)buffer;
    shared_memory.host_send_data[1] = buffer_size;

    opus_decoder.Send(ADSP::Direction::DSP,
                      ADSP::OpusDecoder::Message::ShutdownMultiStreamDecodeObject);
    auto msg = opus_decoder.Receive(ADSP::Direction::Host);
    ASSERT_MSG(msg == ADSP::OpusDecoder::Message::ShutdownMultiStreamDecodeObjectOK,
               "Expected Opus shutdown code {}, got {}",
               ADSP::OpusDecoder::Message::ShutdownMultiStreamDecodeObjectOK, msg);

    R_RETURN(ResultCodeFromLibOpusErrorCode(shared_memory.dsp_return_data[0]));
}

Result HardwareOpus::DecodeInterleaved(u32& out_sample_count, void* output_data,
                                       u64 output_data_size, u32 channel_count, void* input_data,
                                       u64 input_data_size, void* buffer, u64& out_time_taken,
                                       bool reset) {
    std::scoped_lock l{mutex};
    shared_memory.host_send_data[0] = (u64)buffer;
    shared_memory.host_send_data[1] = (u64)input_data;
    shared_memory.host_send_data[2] = input_data_size;
    shared_memory.host_send_data[3] = (u64)output_data;
    shared_memory.host_send_data[4] = output_data_size;
    shared_memory.host_send_data[5] = 0;
    shared_memory.host_send_data[6] = reset;

    opus_decoder.Send(ADSP::Direction::DSP, ADSP::OpusDecoder::Message::DecodeInterleaved);
    auto msg = opus_decoder.Receive(ADSP::Direction::Host);
    if (msg != ADSP::OpusDecoder::Message::DecodeInterleavedOK) {
        LOG_ERROR(Service_Audio, "OpusDecoder returned invalid message. Expected {} got {}",
                  ADSP::OpusDecoder::Message::DecodeInterleavedOK, msg);
        R_THROW(ResultInvalidOpusDSPReturnCode);
    }

    auto error_code{static_cast<s32>(shared_memory.dsp_return_data[0])};
    if (error_code == OPUS_OK) {
        out_sample_count = static_cast<u32>(shared_memory.dsp_return_data[1]);
        out_time_taken = 1000 * shared_memory.dsp_return_data[2];
    }
    R_RETURN(ResultCodeFromLibOpusErrorCode(error_code));
}

Result HardwareOpus::DecodeInterleavedForMultiStream(u32& out_sample_count, void* output_data,
                                                     u64 output_data_size, u32 channel_count,
                                                     void* input_data, u64 input_data_size,
                                                     void* buffer, u64& out_time_taken,
                                                     bool reset) {
    std::scoped_lock l{mutex};
    shared_memory.host_send_data[0] = (u64)buffer;
    shared_memory.host_send_data[1] = (u64)input_data;
    shared_memory.host_send_data[2] = input_data_size;
    shared_memory.host_send_data[3] = (u64)output_data;
    shared_memory.host_send_data[4] = output_data_size;
    shared_memory.host_send_data[5] = 0;
    shared_memory.host_send_data[6] = reset;

    opus_decoder.Send(ADSP::Direction::DSP,
                      ADSP::OpusDecoder::Message::DecodeInterleavedForMultiStream);
    auto msg = opus_decoder.Receive(ADSP::Direction::Host);
    if (msg != ADSP::OpusDecoder::Message::DecodeInterleavedForMultiStreamOK) {
        LOG_ERROR(Service_Audio, "OpusDecoder returned invalid message. Expected {} got {}",
                  ADSP::OpusDecoder::Message::DecodeInterleavedForMultiStreamOK, msg);
        R_THROW(ResultInvalidOpusDSPReturnCode);
    }

    auto error_code{static_cast<s32>(shared_memory.dsp_return_data[0])};
    if (error_code == OPUS_OK) {
        out_sample_count = static_cast<u32>(shared_memory.dsp_return_data[1]);
        out_time_taken = 1000 * shared_memory.dsp_return_data[2];
    }
    R_RETURN(ResultCodeFromLibOpusErrorCode(error_code));
}

Result HardwareOpus::MapMemory(void* buffer, u64 buffer_size) {
    std::scoped_lock l{mutex};
    shared_memory.host_send_data[0] = (u64)buffer;
    shared_memory.host_send_data[1] = buffer_size;

    opus_decoder.Send(ADSP::Direction::DSP, ADSP::OpusDecoder::Message::MapMemory);
    auto msg = opus_decoder.Receive(ADSP::Direction::Host);
    if (msg != ADSP::OpusDecoder::Message::MapMemoryOK) {
        LOG_ERROR(Service_Audio, "OpusDecoder returned invalid message. Expected {} got {}",
                  ADSP::OpusDecoder::Message::MapMemoryOK, msg);
        R_THROW(ResultInvalidOpusDSPReturnCode);
    }
    R_SUCCEED();
}

Result HardwareOpus::UnmapMemory(void* buffer, u64 buffer_size) {
    std::scoped_lock l{mutex};
    shared_memory.host_send_data[0] = (u64)buffer;
    shared_memory.host_send_data[1] = buffer_size;

    opus_decoder.Send(ADSP::Direction::DSP, ADSP::OpusDecoder::Message::UnmapMemory);
    auto msg = opus_decoder.Receive(ADSP::Direction::Host);
    if (msg != ADSP::OpusDecoder::Message::UnmapMemoryOK) {
        LOG_ERROR(Service_Audio, "OpusDecoder returned invalid message. Expected {} got {}",
                  ADSP::OpusDecoder::Message::UnmapMemoryOK, msg);
        R_THROW(ResultInvalidOpusDSPReturnCode);
    }
    R_SUCCEED();
}

} // namespace AudioCore::OpusDecoder
