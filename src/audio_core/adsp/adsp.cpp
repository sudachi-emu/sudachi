// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "audio_core/adsp/adsp.h"
#include "core/core.h"

namespace AudioCore::ADSP {

ADSP::ADSP(Core::System& system, Sink::Sink& sink) {
    audio_renderer = std::make_unique<AudioRenderer::AudioRenderer>(system, sink);
    opus_decoder = std::make_unique<OpusDecoder::OpusDecoder>(system);
    opus_decoder->Send(Direction::DSP, OpusDecoder::Message::Start);
    if (opus_decoder->Receive(Direction::Host) != OpusDecoder::Message::StartOK) {
        LOG_ERROR(Service_Audio, "OpusDecoder failed to initialize.");
        return;
    }
}

AudioRenderer::AudioRenderer& ADSP::AudioRenderer() {
    return *audio_renderer.get();
}

OpusDecoder::OpusDecoder& ADSP::OpusDecoder() {
    return *opus_decoder.get();
}

} // namespace AudioCore::ADSP
