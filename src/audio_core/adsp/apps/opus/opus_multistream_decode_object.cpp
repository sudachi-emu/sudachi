// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "audio_core/adsp/apps/opus/opus_multistream_decode_object.h"
#include "common/assert.h"

namespace AudioCore::ADSP::OpusDecoder {

namespace {
bool IsValidChannelCount(u32 channel_count) {
    return channel_count == 1 || channel_count == 2;
}

bool IsValidStreamCounts(u32 total_stream_count, u32 stereo_stream_count) {
    return total_stream_count > 0 && static_cast<s32>(stereo_stream_count) >= 0 &&
           stereo_stream_count <= total_stream_count && IsValidChannelCount(total_stream_count);
}
} // namespace

u32 OpusMultiStreamDecodeObject::GetWorkBufferSize(u32 total_stream_count,
                                                   u32 stereo_stream_count) {
    if (IsValidStreamCounts(total_stream_count, stereo_stream_count)) {
        return static_cast<u32>(sizeof(OpusMultiStreamDecodeObject)) +
               opus_multistream_decoder_get_size(total_stream_count, stereo_stream_count);
    }
    return 0;
}

OpusMultiStreamDecodeObject& OpusMultiStreamDecodeObject::Initialize(u64 buffer, u64 buffer2) {
    auto* new_decoder = reinterpret_cast<OpusMultiStreamDecodeObject*>(buffer);
    auto* comparison = reinterpret_cast<OpusMultiStreamDecodeObject*>(buffer2);

    if (new_decoder->magic == DecodeMultiStreamObjectMagic) {
        if (!new_decoder->initialized ||
            (new_decoder->initialized && new_decoder->self == comparison)) {
            new_decoder->state_valid = true;
        }
    } else {
        new_decoder->initialized = false;
        new_decoder->state_valid = true;
    }
    return *new_decoder;
}

s32 OpusMultiStreamDecodeObject::InitializeDecoder(u32 sample_rate, u32 total_stream_count,
                                                   u32 channel_count, u32 stereo_stream_count,
                                                   u8* mappings) {
    if (!state_valid) {
        return OPUS_INVALID_STATE;
    }

    if (initialized) {
        return OPUS_OK;
    }

    // See OpusDecodeObject::InitializeDecoder for an explanation of this
    decoder = (LibOpusMSDecoder*)(this + 1);
    s32 ret = opus_multistream_decoder_init(decoder, sample_rate, channel_count, total_stream_count,
                                            stereo_stream_count, mappings);
    if (ret == OPUS_OK) {
        magic = DecodeMultiStreamObjectMagic;
        initialized = true;
        state_valid = true;
        self = this;
        final_range = 0;
    }
    return ret;
}

s32 OpusMultiStreamDecodeObject::Shutdown() {
    if (!state_valid) {
        return OPUS_INVALID_STATE;
    }

    if (initialized) {
        magic = 0x0;
        initialized = false;
        state_valid = false;
        self = nullptr;
        final_range = 0;
        decoder = nullptr;
    }
    return OPUS_OK;
}

s32 OpusMultiStreamDecodeObject::ResetDecoder() {
    return opus_multistream_decoder_ctl(decoder, OPUS_RESET_STATE);
}

s32 OpusMultiStreamDecodeObject::Decode(u32& out_sample_count, u64 output_data,
                                        u64 output_data_size, u64 input_data, u64 input_data_size) {
    ASSERT(initialized);
    out_sample_count = 0;

    if (!state_valid) {
        return OPUS_INVALID_STATE;
    }

    auto ret_code_or_samples = opus_multistream_decode(
        decoder, reinterpret_cast<const u8*>(input_data), static_cast<opus_int32>(input_data_size),
        reinterpret_cast<opus_int16*>(output_data), static_cast<opus_int32>(output_data_size), 0);

    if (ret_code_or_samples < OPUS_OK) {
        return ret_code_or_samples;
    }

    out_sample_count = ret_code_or_samples;
    return opus_multistream_decoder_ctl(decoder, OPUS_GET_FINAL_RANGE_REQUEST, &final_range);
}

} // namespace AudioCore::ADSP::OpusDecoder
