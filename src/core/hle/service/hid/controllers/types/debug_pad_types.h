// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "common/bit_field.h"
#include "common/common_types.h"
#include "core/hid/hid_types.h"

namespace Service::HID {

// This is nn::hid::DebugPadAttribute
struct DebugPadAttribute {
    union {
        u32 raw{};
        BitField<0, 1, u32> connected;
    };
};
static_assert(sizeof(DebugPadAttribute) == 0x4, "DebugPadAttribute is an invalid size");

// This is nn::hid::DebugPadState
struct DebugPadState {
    s64 sampling_number{};
    DebugPadAttribute attribute{};
    Core::HID::DebugPadButton pad_state{};
    Core::HID::AnalogStickState r_stick{};
    Core::HID::AnalogStickState l_stick{};
};
static_assert(sizeof(DebugPadState) == 0x20, "DebugPadState is an invalid state");

} // namespace Service::HID
