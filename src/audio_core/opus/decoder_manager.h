// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "audio_core/opus/hardware_opus.h"
#include "audio_core/opus/parameters.h"
#include "common/common_types.h"
#include "core/hle/service/audio/errors.h"

namespace Core {
class System;
}

namespace AudioCore::OpusDecoder {

class OpusDecoderManager {
public:
    OpusDecoderManager(Core::System& system);

    HardwareOpus& GetHardwareOpus() {
        return hardware_opus;
    }

    Result GetWorkBufferSize(const OpusParameters& params, u32& out_size);
    Result GetWorkBufferSizeEx(const OpusParametersEx& params, u32& out_size);
    Result GetWorkBufferSizeExEx(const OpusParametersEx& params, u32& out_size);
    Result GetWorkBufferSizeForMultiStream(const OpusMultiStreamParameters& params, u32& out_size);
    Result GetWorkBufferSizeForMultiStreamEx(const OpusMultiStreamParametersEx& params,
                                             u32& out_size);
    Result GetWorkBufferSizeForMultiStreamExEx(const OpusMultiStreamParametersEx& params,
                                               u32& out_size);

private:
    Core::System& system;
    HardwareOpus hardware_opus;
    std::array<u32, MaxChannels> required_workbuffer_sizes{};
};

} // namespace AudioCore::OpusDecoder
