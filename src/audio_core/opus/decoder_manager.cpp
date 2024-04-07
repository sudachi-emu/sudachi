// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "audio_core/adsp/apps/opus/opus_decoder.h"
#include "audio_core/opus/decoder_manager.h"
#include "common/alignment.h"
#include "core/core.h"

namespace AudioCore::OpusDecoder {
using namespace Service::Audio;

namespace {
bool IsValidChannelCount(u32 channel_count) {
    return channel_count == 1 || channel_count == 2;
}

bool IsValidMultiStreamChannelCount(u32 channel_count) {
    return channel_count > 0 && channel_count <= OpusStreamCountMax;
}

bool IsValidSampleRate(u32 sample_rate) {
    return sample_rate == 8'000 || sample_rate == 12'000 || sample_rate == 16'000 ||
           sample_rate == 24'000 || sample_rate == 48'000;
}

bool IsValidStreamCount(u32 channel_count, u32 total_stream_count, u32 stereo_stream_count) {
    return total_stream_count > 0 && static_cast<s32>(stereo_stream_count) >= 0 &&
           stereo_stream_count <= total_stream_count &&
           total_stream_count + stereo_stream_count <= channel_count;
}

} // namespace

OpusDecoderManager::OpusDecoderManager(Core::System& system_)
    : system{system_}, hardware_opus{system} {
    for (u32 i = 0; i < MaxChannels; i++) {
        required_workbuffer_sizes[i] = hardware_opus.GetWorkBufferSize(1 + i);
    }
}

Result OpusDecoderManager::GetWorkBufferSize(const OpusParameters& params, u32& out_size) {
    OpusParametersEx ex{
        .sample_rate = params.sample_rate,
        .channel_count = params.channel_count,
        .use_large_frame_size = false,
    };
    R_RETURN(GetWorkBufferSizeExEx(ex, out_size));
}

Result OpusDecoderManager::GetWorkBufferSizeEx(const OpusParametersEx& params, u32& out_size) {
    R_RETURN(GetWorkBufferSizeExEx(params, out_size));
}

Result OpusDecoderManager::GetWorkBufferSizeExEx(const OpusParametersEx& params, u32& out_size) {
    R_UNLESS(IsValidChannelCount(params.channel_count), ResultInvalidOpusChannelCount);
    R_UNLESS(IsValidSampleRate(params.sample_rate), ResultInvalidOpusSampleRate);

    auto work_buffer_size{required_workbuffer_sizes[params.channel_count - 1]};
    auto frame_size{params.use_large_frame_size ? 5760 : 1920};
    work_buffer_size +=
        Common::AlignUp((frame_size * params.channel_count) / (48'000 / params.sample_rate), 64);
    out_size = work_buffer_size + 0x600;
    R_SUCCEED();
}

Result OpusDecoderManager::GetWorkBufferSizeForMultiStream(const OpusMultiStreamParameters& params,
                                                           u32& out_size) {
    OpusMultiStreamParametersEx ex{
        .sample_rate = params.sample_rate,
        .channel_count = params.channel_count,
        .total_stream_count = params.total_stream_count,
        .stereo_stream_count = params.stereo_stream_count,
        .use_large_frame_size = false,
        .mappings = {},
    };
    R_RETURN(GetWorkBufferSizeForMultiStreamExEx(ex, out_size));
}

Result OpusDecoderManager::GetWorkBufferSizeForMultiStreamEx(
    const OpusMultiStreamParametersEx& params, u32& out_size) {
    R_RETURN(GetWorkBufferSizeForMultiStreamExEx(params, out_size));
}

Result OpusDecoderManager::GetWorkBufferSizeForMultiStreamExEx(
    const OpusMultiStreamParametersEx& params, u32& out_size) {
    R_UNLESS(IsValidMultiStreamChannelCount(params.channel_count), ResultInvalidOpusChannelCount);
    R_UNLESS(IsValidSampleRate(params.sample_rate), ResultInvalidOpusSampleRate);
    R_UNLESS(IsValidStreamCount(params.channel_count, params.total_stream_count,
                                params.stereo_stream_count),
             ResultInvalidOpusSampleRate);

    auto work_buffer_size{hardware_opus.GetWorkBufferSizeForMultiStream(
        params.total_stream_count, params.stereo_stream_count)};
    auto frame_size{params.use_large_frame_size ? 5760 : 1920};
    work_buffer_size += Common::AlignUp(1500 * params.total_stream_count, 64);
    work_buffer_size +=
        Common::AlignUp((frame_size * params.channel_count) / (48'000 / params.sample_rate), 64);
    out_size = work_buffer_size;
    R_SUCCEED();
}

} // namespace AudioCore::OpusDecoder
