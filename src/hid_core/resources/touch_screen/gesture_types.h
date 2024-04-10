// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <array>
#include "common/bit_field.h"
#include "common/common_types.h"
#include "common/point.h"

namespace Service::HID {
static constexpr size_t MAX_FINGERS = 16;
static constexpr size_t MAX_POINTS = 4;

// This is nn::hid::GestureType
enum class GestureType : u32 {
    Idle,     // Nothing touching the screen
    Complete, // Set at the end of a touch event
    Cancel,   // Set when the number of fingers change
    Touch,    // A finger just touched the screen
    Press,    // Set if last type is touch and the finger hasn't moved
    Tap,      // Fast press then release
    Pan,      // All points moving together across the screen
    Swipe,    // Fast press movement and release of a single point
    Pinch,    // All points moving away/closer to the midpoint
    Rotate,   // All points rotating from the midpoint
};

// This is nn::hid::GestureDirection
enum class GestureDirection : u32 {
    None,
    Left,
    Up,
    Right,
    Down,
};

// This is nn::hid::GestureAttribute
struct GestureAttribute {
    union {
        u32 raw{};

        BitField<4, 1, u32> is_new_touch;
        BitField<8, 1, u32> is_double_tap;
    };
};
static_assert(sizeof(GestureAttribute) == 4, "GestureAttribute is an invalid size");

// This is nn::hid::GestureState
struct GestureState {
    s64 sampling_number{};
    s64 detection_count{};
    GestureType type{GestureType::Idle};
    GestureDirection direction{GestureDirection::None};
    Common::Point<s32> pos{};
    Common::Point<s32> delta{};
    f32 vel_x{};
    f32 vel_y{};
    GestureAttribute attributes{};
    f32 scale{};
    f32 rotation_angle{};
    s32 point_count{};
    std::array<Common::Point<s32>, 4> points{};
};
static_assert(sizeof(GestureState) == 0x60, "GestureState is an invalid size");

struct GestureProperties {
    std::array<Common::Point<s32>, MAX_POINTS> points{};
    std::size_t active_points{};
    Common::Point<s32> mid_point{};
    s64 detection_count{};
    u64 delta_time{};
    f32 average_distance{};
    f32 angle{};
};

} // namespace Service::HID
