// SPDX-FileCopyrightText: Copyright 2021 sudachi Emulator Project
// SPDX-FileCopyrightText: Copyright 2010 The Android Open Source Project
// SPDX-License-Identifier: GPL-3.0-or-later
// Parts of this implementation were based on:
// https://cs.android.com/android/platform/superproject/+/android-5.1.1_r38:frameworks/native/include/gui/IGraphicBufferProducer.h

#pragma once

#include "common/common_funcs.h"
#include "common/common_types.h"
#include "common/math_util.h"
#include "core/hle/service/nvnflinger/ui/fence.h"
#include "core/hle/service/nvnflinger/window.h"

namespace Service::android {

class InputParcel;

#pragma pack(push, 1)
struct QueueBufferInput final {
    explicit QueueBufferInput(InputParcel& parcel);
    explicit QueueBufferInput() = default;

    void Deflate(s64* timestamp_, bool* is_auto_timestamp_, Common::Rectangle<s32>* crop_,
                 NativeWindowScalingMode* scaling_mode_, NativeWindowTransform* transform_,
                 u32* sticky_transform_, bool* async_, s32* swap_interval_, Fence* fence_) const {
        *timestamp_ = timestamp;
        *is_auto_timestamp_ = static_cast<bool>(is_auto_timestamp);
        *crop_ = crop;
        *scaling_mode_ = scaling_mode;
        *transform_ = transform;
        *sticky_transform_ = sticky_transform;
        *async_ = static_cast<bool>(async);
        *swap_interval_ = swap_interval;
        *fence_ = fence;
    }

    s64 timestamp{};
    s32 is_auto_timestamp{};
    Common::Rectangle<s32> crop{};
    NativeWindowScalingMode scaling_mode{};
    NativeWindowTransform transform{};
    u32 sticky_transform{};
    s32 async{};
    s32 swap_interval{};
    Fence fence{};
};
#pragma pack(pop)
static_assert(sizeof(QueueBufferInput) == 84, "QueueBufferInput has wrong size");

struct QueueBufferOutput final {
    QueueBufferOutput();

    void Deflate(u32* width_, u32* height_, u32* transform_hint_, u32* num_pending_buffers_,
                 u64* frame_number_) const {
        *width_ = width;
        *height_ = height;
        *transform_hint_ = transform_hint;
        *num_pending_buffers_ = num_pending_buffers;
        *frame_number_ = frame_number;
    }

    void Inflate(u32 width_, u32 height_, u32 transform_hint_, u32 num_pending_buffers_,
                 u64 frame_number_) {
        width = width_;
        height = height_;
        transform_hint = transform_hint_;
        num_pending_buffers = num_pending_buffers_;
        frame_number = frame_number_;
    }

public:
    u32 width{};
    u32 height{};
    u32 transform_hint{};
    u32 num_pending_buffers{};
    u64 frame_number{};
};
static_assert(sizeof(QueueBufferOutput) == 24, "QueueBufferOutput has wrong size");

} // namespace Service::android
