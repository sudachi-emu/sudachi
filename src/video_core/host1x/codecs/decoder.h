// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>
#include <mutex>
#include <optional>
#include <string_view>
#include <unordered_map>
#include <queue>

#include "common/common_types.h"
#include "video_core/host1x/ffmpeg/ffmpeg.h"
#include "video_core/host1x/nvdec_common.h"

namespace Tegra {

namespace Host1x {
class Host1x;
class FrameQueue;
} // namespace Host1x

class Decoder {
public:
    virtual ~Decoder();

    /// Call decoders to construct headers, decode AVFrame with ffmpeg
    void Decode();

    bool UsingDecodeOrder() const {
        return decode_api.UsingDecodeOrder();
    }

    /// Returns the value of current_codec
    [[nodiscard]] Host1x::NvdecCommon::VideoCodec GetCurrentCodec() const {
        return codec;
    }

    /// Return name of the current codec
    [[nodiscard]] virtual std::string_view GetCurrentCodecName() const = 0;

protected:
    explicit Decoder(Host1x::Host1x& host1x, s32 id,
                     const Host1x::NvdecCommon::NvdecRegisters& regs,
                     Host1x::FrameQueue& frame_queue);

    virtual std::span<const u8> ComposeFrame() = 0;
    virtual std::tuple<u64, u64> GetProgressiveOffsets() = 0;
    virtual std::tuple<u64, u64, u64, u64> GetInterlacedOffsets() = 0;
    virtual bool IsInterlaced() = 0;

    Host1x::Host1x& host1x;
    Tegra::MemoryManager& memory_manager;
    const Host1x::NvdecCommon::NvdecRegisters& regs;
    s32 id;
    Host1x::FrameQueue& frame_queue;
    Host1x::NvdecCommon::VideoCodec codec;
    FFmpeg::DecodeApi decode_api;
    bool initialized{};
    bool vp9_hidden_frame{};
};

} // namespace Tegra
