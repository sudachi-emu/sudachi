// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <stb_dxt.h>
#include <string.h>
#include "common/alignment.h"
#include "video_core/textures/bcn.h"
#include "video_core/textures/workers.h"

namespace Tegra::Texture::BCN {

using BCNCompressor = void(u8* block_output, const u8* block_input, bool any_alpha);

template <u32 BytesPerBlock, bool ThresholdAlpha = false>
void CompressBCN(std::span<const uint8_t> data, uint32_t width, uint32_t height, uint32_t depth,
                 std::span<uint8_t> output, BCNCompressor f) {
    constexpr u8 alpha_threshold = 128;
    constexpr u32 bytes_per_px = 4;
    const u32 plane_dim = width * height;

    Common::ThreadWorker& workers{GetThreadWorkers()};

    for (u32 z = 0; z < depth; z++) {
        for (u32 y = 0; y < height; y += 4) {
            auto compress_row = [z, y, width, height, plane_dim, f, data, output]() {
                for (u32 x = 0; x < width; x += 4) {
                    // Gather 4x4 block of RGBA texels
                    u8 input_colors[4][4][4];
                    bool any_alpha = false;

                    for (u32 j = 0; j < 4; j++) {
                        for (u32 i = 0; i < 4; i++) {
                            const size_t coord =
                                (z * plane_dim + (y + j) * width + (x + i)) * bytes_per_px;

                            if ((x + i < width) && (y + j < height)) {
                                if constexpr (ThresholdAlpha) {
                                    if (data[coord + 3] >= alpha_threshold) {
                                        input_colors[j][i][0] = data[coord + 0];
                                        input_colors[j][i][1] = data[coord + 1];
                                        input_colors[j][i][2] = data[coord + 2];
                                        input_colors[j][i][3] = 255;
                                    } else {
                                        any_alpha = true;
                                        memset(input_colors[j][i], 0, bytes_per_px);
                                    }
                                } else {
                                    memcpy(input_colors[j][i], &data[coord], bytes_per_px);
                                }
                            } else {
                                memset(input_colors[j][i], 0, bytes_per_px);
                            }
                        }
                    }

                    const u32 bytes_per_row = BytesPerBlock * Common::DivideUp(width, 4U);
                    const u32 bytes_per_plane = bytes_per_row * Common::DivideUp(height, 4U);
                    f(output.data() + z * bytes_per_plane + (y / 4) * bytes_per_row +
                          (x / 4) * BytesPerBlock,
                      reinterpret_cast<u8*>(input_colors), any_alpha);
                }
            };
            workers.QueueWork(std::move(compress_row));
        }
        workers.WaitForRequests();
    }
}

void CompressBC1(std::span<const uint8_t> data, uint32_t width, uint32_t height, uint32_t depth,
                 std::span<uint8_t> output) {
    CompressBCN<8, true>(data, width, height, depth, output,
                         [](u8* block_output, const u8* block_input, bool any_alpha) {
                             stb_compress_bc1_block(block_output, block_input, any_alpha,
                                                    STB_DXT_NORMAL);
                         });
}

void CompressBC3(std::span<const uint8_t> data, uint32_t width, uint32_t height, uint32_t depth,
                 std::span<uint8_t> output) {
    CompressBCN<16, false>(data, width, height, depth, output,
                           [](u8* block_output, const u8* block_input, bool any_alpha) {
                               stb_compress_bc3_block(block_output, block_input, STB_DXT_NORMAL);
                           });
}

} // namespace Tegra::Texture::BCN
