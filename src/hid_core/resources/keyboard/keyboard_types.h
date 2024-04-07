// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "common/common_types.h"
#include "hid_core/hid_types.h"

namespace Service::HID {

// This is nn::hid::detail::KeyboardState
struct KeyboardState {
    s64 sampling_number{};
    Core::HID::KeyboardModifier modifier{};
    Core::HID::KeyboardAttribute attribute{};
    Core::HID::KeyboardKey key{};
};
static_assert(sizeof(KeyboardState) == 0x30, "KeyboardState is an invalid size");

} // namespace Service::HID
