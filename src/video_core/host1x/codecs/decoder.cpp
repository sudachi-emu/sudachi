// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/assert.h"
#include "common/settings.h"
#include "video_core/host1x/codecs/decoder.h"
#include "video_core/host1x/host1x.h"
#include "video_core/memory_manager.h"

namespace Tegra {

Decoder::Decoder(Host1x::Host1x& host1x_, s32 id_, const Host1x::NvdecCommon::NvdecRegisters& regs_,
                 Host1x::FrameQueue& frame_queue_)
    : host1x(host1x_), memory_manager{host1x.GMMU()}, regs{regs_}, id{id_},
      frame_queue{frame_queue_} {}

Decoder::~Decoder() = default;

void Decoder::Decode() {
    if (!initialized) {
        return;
    }

    const auto packet_data = ComposeFrame();
    // Send assembled bitstream to decoder.
    if (!decode_api.SendPacket(packet_data)) {
        return;
    }

    // Only receive/store visible frames.
    if (vp9_hidden_frame) {
        return;
    }

    // Receive output frames from decoder.
    auto frame = decode_api.ReceiveFrame();

    if (IsInterlaced()) {
        auto [luma_top, luma_bottom, chroma_top, chroma_bottom] = GetInterlacedOffsets();
        auto frame_copy = frame;

        if (!frame.get()) {
            LOG_ERROR(HW_GPU,
                      "Nvdec {} dailed to decode interlaced frame for top 0x{:X} bottom 0x{:X}", id,
                      luma_top, luma_bottom);
        }

        if (UsingDecodeOrder()) {
            frame_queue.PushDecodeOrder(id, luma_top, std::move(frame));
            frame_queue.PushDecodeOrder(id, luma_bottom, std::move(frame_copy));
        } else {
            frame_queue.PushPresentOrder(id, luma_top, std::move(frame));
            frame_queue.PushPresentOrder(id, luma_bottom, std::move(frame_copy));
        }
    } else {
        auto [luma_offset, chroma_offset] = GetProgressiveOffsets();

        if (!frame.get()) {
            LOG_ERROR(HW_GPU, "Nvdec {} failed to decode progressive frame for luma 0x{:X}", id,
                      luma_offset);
        }

        if (UsingDecodeOrder()) {
            frame_queue.PushDecodeOrder(id, luma_offset, std::move(frame));
        } else {
            frame_queue.PushPresentOrder(id, luma_offset, std::move(frame));
        }
    }
}

} // namespace Tegra
