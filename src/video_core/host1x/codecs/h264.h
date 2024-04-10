// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <span>
#include <vector>

#include "common/bit_field.h"
#include "common/common_funcs.h"
#include "common/common_types.h"
#include "common/scratch_buffer.h"
#include "video_core/host1x/codecs/decoder.h"
#include "video_core/host1x/nvdec_common.h"

namespace Tegra {

namespace Host1x {
class Host1x;
} // namespace Host1x

namespace Decoders {

class H264BitWriter {
public:
    H264BitWriter();
    ~H264BitWriter();

    /// The following Write methods are based on clause 9.1 in the H.264 specification.
    /// WriteSe and WriteUe write in the Exp-Golomb-coded syntax
    void WriteU(s32 value, s32 value_sz);
    void WriteSe(s32 value);
    void WriteUe(u32 value);

    /// Finalize the bitstream
    void End();

    /// append a bit to the stream, equivalent value to the state parameter
    void WriteBit(bool state);

    /// Based on section 7.3.2.1.1.1 and Table 7-4 in the H.264 specification
    /// Writes the scaling matrices of the sream
    void WriteScalingList(Common::ScratchBuffer<u8>& scan, std::span<const u8> list, s32 start,
                          s32 count);

    /// Return the bitstream as a vector.
    [[nodiscard]] std::vector<u8>& GetByteArray();
    [[nodiscard]] const std::vector<u8>& GetByteArray() const;

private:
    void WriteBits(s32 value, s32 bit_count);
    void WriteExpGolombCodedInt(s32 value);
    void WriteExpGolombCodedUInt(u32 value);
    [[nodiscard]] s32 GetFreeBufferBits();
    void Flush();

    s32 buffer_size{8};

    s32 buffer{};
    s32 buffer_pos{};
    std::vector<u8> byte_array;
};

struct Offset {
    constexpr u32 Address() const noexcept {
        return offset << 8;
    }

private:
    u32 offset;
};
static_assert(std::is_trivial_v<Offset>, "Offset must be trivial");
static_assert(sizeof(Offset) == 0x4, "Offset has the wrong size!");

struct H264ParameterSet {
    s32 log2_max_pic_order_cnt_lsb_minus4; ///< 0x00
    s32 delta_pic_order_always_zero_flag;  ///< 0x04
    s32 frame_mbs_only_flag;               ///< 0x08
    u32 pic_width_in_mbs;                  ///< 0x0C
    u32 frame_height_in_mbs;               ///< 0x10
    union {                                ///< 0x14
        BitField<0, 2, u32> tile_format;
        BitField<2, 3, u32> gob_height;
        BitField<5, 27, u32> reserved_surface_format;
    };
    u32 entropy_coding_mode_flag;               ///< 0x18
    s32 pic_order_present_flag;                 ///< 0x1C
    s32 num_refidx_l0_default_active;           ///< 0x20
    s32 num_refidx_l1_default_active;           ///< 0x24
    s32 deblocking_filter_control_present_flag; ///< 0x28
    s32 redundant_pic_cnt_present_flag;         ///< 0x2C
    u32 transform_8x8_mode_flag;                ///< 0x30
    u32 pitch_luma;                             ///< 0x34
    u32 pitch_chroma;                           ///< 0x38
    Offset luma_top_offset;                     ///< 0x3C
    Offset luma_bot_offset;                     ///< 0x40
    Offset luma_frame_offset;                   ///< 0x44
    Offset chroma_top_offset;                   ///< 0x48
    Offset chroma_bot_offset;                   ///< 0x4C
    Offset chroma_frame_offset;                 ///< 0x50
    u32 hist_buffer_size;                       ///< 0x54
    union {                                     ///< 0x58
        union {
            BitField<0, 1, u64> mbaff_frame;
            BitField<1, 1, u64> direct_8x8_inference;
            BitField<2, 1, u64> weighted_pred;
            BitField<3, 1, u64> constrained_intra_pred;
            BitField<4, 1, u64> ref_pic;
            BitField<5, 1, u64> field_pic;
            BitField<6, 1, u64> bottom_field;
            BitField<7, 1, u64> second_field;
        } flags;
        BitField<8, 4, u64> log2_max_frame_num_minus4;
        BitField<12, 2, u64> chroma_format_idc;
        BitField<14, 2, u64> pic_order_cnt_type;
        BitField<16, 6, s64> pic_init_qp_minus26;
        BitField<22, 5, s64> chroma_qp_index_offset;
        BitField<27, 5, s64> second_chroma_qp_index_offset;
        BitField<32, 2, u64> weighted_bipred_idc;
        BitField<34, 7, u64> curr_pic_idx;
        BitField<41, 5, u64> curr_col_idx;
        BitField<46, 16, u64> frame_number;
        BitField<62, 1, u64> frame_surfaces;
        BitField<63, 1, u64> output_memory_layout;
    };
};
static_assert(sizeof(H264ParameterSet) == 0x60, "H264ParameterSet is an invalid size");

#define ASSERT_POSITION(field_name, position)                                                      \
    static_assert(offsetof(H264ParameterSet, field_name) == position,                              \
                  "Field " #field_name " has invalid position")

ASSERT_POSITION(log2_max_pic_order_cnt_lsb_minus4, 0x00);
ASSERT_POSITION(delta_pic_order_always_zero_flag, 0x04);
ASSERT_POSITION(frame_mbs_only_flag, 0x08);
ASSERT_POSITION(pic_width_in_mbs, 0x0C);
ASSERT_POSITION(frame_height_in_mbs, 0x10);
ASSERT_POSITION(tile_format, 0x14);
ASSERT_POSITION(entropy_coding_mode_flag, 0x18);
ASSERT_POSITION(pic_order_present_flag, 0x1C);
ASSERT_POSITION(num_refidx_l0_default_active, 0x20);
ASSERT_POSITION(num_refidx_l1_default_active, 0x24);
ASSERT_POSITION(deblocking_filter_control_present_flag, 0x28);
ASSERT_POSITION(redundant_pic_cnt_present_flag, 0x2C);
ASSERT_POSITION(transform_8x8_mode_flag, 0x30);
ASSERT_POSITION(pitch_luma, 0x34);
ASSERT_POSITION(pitch_chroma, 0x38);
ASSERT_POSITION(luma_top_offset, 0x3C);
ASSERT_POSITION(luma_bot_offset, 0x40);
ASSERT_POSITION(luma_frame_offset, 0x44);
ASSERT_POSITION(chroma_top_offset, 0x48);
ASSERT_POSITION(chroma_bot_offset, 0x4C);
ASSERT_POSITION(chroma_frame_offset, 0x50);
ASSERT_POSITION(hist_buffer_size, 0x54);
ASSERT_POSITION(flags, 0x58);
#undef ASSERT_POSITION

struct DpbEntry {
    union {
        BitField<0, 7, u32> index;
        BitField<7, 5, u32> col_idx;
        BitField<12, 2, u32> state;
        BitField<14, 1, u32> is_long_term;
        BitField<15, 1, u32> non_existing;
        BitField<16, 1, u32> is_field;
        BitField<17, 4, u32> top_field_marking;
        BitField<21, 4, u32> bottom_field_marking;
        BitField<25, 1, u32> output_memory_layout;
        BitField<26, 6, u32> reserved;
    } flags;
    std::array<u32, 2> field_order_cnt;
    u32 frame_idx;
};
static_assert(sizeof(DpbEntry) == 0x10, "DpbEntry has the wrong size!");

struct DisplayParam {
    union {
        BitField<0, 1, u32> enable_tf_output;
        BitField<1, 1, u32> vc1_map_y_flag;
        BitField<2, 3, u32> map_y_value;
        BitField<5, 1, u32> vc1_map_uv_flag;
        BitField<6, 3, u32> map_uv_value;
        BitField<9, 8, u32> out_stride;
        BitField<17, 3, u32> tiling_format;
        BitField<20, 1, u32> output_structure; // 0=frame, 1=field
        BitField<21, 11, u32> reserved0;
    };
    std::array<s32, 2> output_top;
    std::array<s32, 2> output_bottom;
    union {
        BitField<0, 1, u32> enable_histogram;
        BitField<1, 12, u32> histogram_start_x;
        BitField<13, 12, u32> histogram_start_y;
        BitField<25, 7, u32> reserved1;
    };
    union {
        BitField<0, 12, u32> histogram_end_x;
        BitField<12, 12, u32> histogram_end_y;
        BitField<24, 8, u32> reserved2;
    };
};
static_assert(sizeof(DisplayParam) == 0x1C, "DisplayParam has the wrong size!");

struct H264DecoderContext {
    INSERT_PADDING_WORDS_NOINIT(13);                        ///< 0x0000
    std::array<u8, 16> eos;                                 ///< 0x0034
    u8 explicit_eos_present_flag;                           ///< 0x0044
    u8 hint_dump_en;                                        ///< 0x0045
    INSERT_PADDING_BYTES_NOINIT(2);                         ///< 0x0046
    u32 stream_len;                                         ///< 0x0048
    u32 slice_count;                                        ///< 0x004C
    u32 mbhist_buffer_size;                                 ///< 0x0050
    u32 gptimer_timeout_value;                              ///< 0x0054
    H264ParameterSet h264_parameter_set;                    ///< 0x0058
    std::array<s32, 2> curr_field_order_cnt;                ///< 0x00B8
    std::array<DpbEntry, 16> dpb;                           ///< 0x00C0
    std::array<u8, 0x60> weight_scale_4x4;                  ///< 0x01C0
    std::array<u8, 0x80> weight_scale_8x8;                  ///< 0x0220
    std::array<u8, 2> num_inter_view_refs_lX;               ///< 0x02A0
    std::array<u8, 14> reserved2;                           ///< 0x02A2
    std::array<std::array<s8, 16>, 2> inter_view_refidx_lX; ///< 0x02B0
    union {                                                 ///< 0x02D0
        BitField<0, 1, u32> lossless_ipred8x8_filter_enable;
        BitField<1, 1, u32> qpprime_y_zero_transform_bypass_flag;
        BitField<2, 30, u32> reserved3;
    };
    DisplayParam display_param;   ///< 0x02D4
    std::array<u32, 3> reserved4; ///< 0x02F0
};
static_assert(sizeof(H264DecoderContext) == 0x2FC, "H264DecoderContext is an invalid size");

#define ASSERT_POSITION(field_name, position)                                                      \
    static_assert(offsetof(H264DecoderContext, field_name) == position,                            \
                  "Field " #field_name " has invalid position")

ASSERT_POSITION(stream_len, 0x48);
ASSERT_POSITION(h264_parameter_set, 0x58);
ASSERT_POSITION(dpb, 0xC0);
ASSERT_POSITION(weight_scale_4x4, 0x1C0);
#undef ASSERT_POSITION

class H264 final : public Decoder {
public:
    explicit H264(Host1x::Host1x& host1x, const Host1x::NvdecCommon::NvdecRegisters& regs, s32 id,
                  Host1x::FrameQueue& frame_queue);
    ~H264() override;

    H264(const H264&) = delete;
    H264& operator=(const H264&) = delete;

    H264(H264&&) = delete;
    H264& operator=(H264&&) = delete;

    /// Compose the H264 frame for FFmpeg decoding
    [[nodiscard]] std::span<const u8> ComposeFrame() override;

    std::tuple<u64, u64> GetProgressiveOffsets() override;
    std::tuple<u64, u64, u64, u64> GetInterlacedOffsets() override;
    bool IsInterlaced() override;

    std::string_view GetCurrentCodecName() const override {
        return "H264";
    }

private:
    bool is_first_frame{true};
    Common::ScratchBuffer<u8> frame_scratch;
    Common::ScratchBuffer<u8> scan_scratch;
    H264DecoderContext current_context{};
};

} // namespace Decoders
} // namespace Tegra
