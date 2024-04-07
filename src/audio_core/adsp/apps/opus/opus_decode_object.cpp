// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "audio_core/adsp/apps/opus/opus_decode_object.h"
#include "common/assert.h"

namespace AudioCore::ADSP::OpusDecoder {
namespace {
bool IsValidChannelCount(u32 channel_count) {
    return channel_count == 1 || channel_count == 2;
}
} // namespace

u32 OpusDecodeObject::GetWorkBufferSize(u32 channel_count) {
    if (!IsValidChannelCount(channel_count)) {
        return 0;
    }
    return static_cast<u32>(sizeof(OpusDecodeObject)) + opus_decoder_get_size(channel_count);
}

OpusDecodeObject& OpusDecodeObject::Initialize(u64 buffer, u64 buffer2) {
    auto* new_decoder = reinterpret_cast<OpusDecodeObject*>(buffer);
    auto* comparison = reinterpret_cast<OpusDecodeObject*>(buffer2);

    if (new_decoder->magic == DecodeObjectMagic) {
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

s32 OpusDecodeObject::InitializeDecoder(u32 sample_rate, u32 channel_count) {
    if (!state_valid) {
        return OPUS_INVALID_STATE;
    }

    if (initialized) {
        return OPUS_OK;
    }

    // Unfortunately libopus does not expose the OpusDecoder struct publicly, so we can't include
    // it in this class. Nintendo does not allocate memory, which is why we have a workbuffer
    // provided.
    // We could use _create and have libopus allocate it for us, but then we have to separately
    // track which decoder is being used between this and multistream in order to call the correct
    // destroy from the host side.
    // This is a bit cringe, but is safe as these objects are only ever initialized inside the given
    // workbuffer, and GetWorkBufferSize will guarantee there's enough space to follow.
    decoder = (LibOpusDecoder*)(this + 1);
    s32 ret = opus_decoder_init(decoder, sample_rate, channel_count);
    if (ret == OPUS_OK) {
        magic = DecodeObjectMagic;
        initialized = true;
        state_valid = true;
        self = this;
        final_range = 0;
    }
    return ret;
}

s32 OpusDecodeObject::Shutdown() {
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

s32 OpusDecodeObject::ResetDecoder() {
    return opus_decoder_ctl(decoder, OPUS_RESET_STATE);
}

s32 OpusDecodeObject::Decode(u32& out_sample_count, u64 output_data, u64 output_data_size,
                             u64 input_data, u64 input_data_size) {
    ASSERT(initialized);
    out_sample_count = 0;

    if (!state_valid) {
        return OPUS_INVALID_STATE;
    }

    auto ret_code_or_samples = opus_decode(
        decoder, reinterpret_cast<const u8*>(input_data), static_cast<opus_int32>(input_data_size),
        reinterpret_cast<opus_int16*>(output_data), static_cast<opus_int32>(output_data_size), 0);

    if (ret_code_or_samples < OPUS_OK) {
        return ret_code_or_samples;
    }

    out_sample_count = ret_code_or_samples;
    return opus_decoder_ctl(decoder, OPUS_GET_FINAL_RANGE_REQUEST, &final_range);
}

} // namespace AudioCore::ADSP::OpusDecoder
