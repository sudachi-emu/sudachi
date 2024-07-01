// SPDX-FileCopyrightText: Copyright 2021 sudachi Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "common/common_funcs.h"
#include "common/common_types.h"

namespace Service::android {

/// Attributes queryable with Query
enum class NativeWindow : s32 {
    Width = 0,
    Height = 1,
    Format = 2,
    MinUndequeedBuffers = 3,
    QueuesToWindowComposer = 4,
    ConcreteType = 5,
    DefaultWidth = 6,
    DefaultHeight = 7,
    TransformHint = 8,
    ConsumerRunningBehind = 9,
    ConsumerUsageBits = 10,
    StickyTransform = 11,
    DefaultDataSpace = 12,
    BufferAge = 13,
};

/// Parameter for Connect/Disconnect
enum class NativeWindowApi : s32 {
    NoConnectedApi = 0,
    Egl = 1,
    Cpu = 2,
    Media = 3,
    Camera = 4,
};

/// Scaling mode parameter for QueueBuffer
enum class NativeWindowScalingMode : s32 {
    Freeze = 0,
    ScaleToWindow = 1,
    ScaleCrop = 2,
    NoScaleCrop = 3,
    PreserveAspectRatio = 4,
};

/// Transform parameter for QueueBuffer
enum class NativeWindowTransform : u32 {
    None = 0x0,
    InverseDisplay = 0x08,
    ReturnFrameNumber = 0x20
};
DECLARE_ENUM_FLAG_OPERATORS(NativeWindowTransform);

} // namespace Service::android
