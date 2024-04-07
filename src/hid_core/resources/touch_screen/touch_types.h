// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <array>

#include <array>
#include "common/bit_field.h"
#include "common/common_funcs.h"
#include "common/common_types.h"
#include "common/point.h"
#include "hid_core/hid_types.h"

namespace Service::HID {
constexpr std::size_t MaxFingers = 16;
constexpr std::size_t MaxPoints = 4;
constexpr u32 TouchSensorWidth = 1280;
constexpr u32 TouchSensorHeight = 720;
constexpr s32 MaxRotationAngle = 270;
constexpr u32 MaxTouchDiameter = 30;
constexpr u32 TouchBorders = 15;

// HW is around 700, value is set to 400 to make it easier to trigger with mouse
constexpr f32 SwipeThreshold = 400.0f; // Threshold in pixels/s
constexpr f32 AngleThreshold = 0.015f; // Threshold in radians
constexpr f32 PinchThreshold = 0.5f;   // Threshold in pixels
constexpr f32 PressDelay = 0.5f;       // Time in seconds
constexpr f32 DoubleTapDelay = 0.35f;  // Time in seconds

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
    GestureTypeMax = Rotate,
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
    std::array<Common::Point<s32>, MaxPoints> points{};
    std::size_t active_points{};
    Common::Point<s32> mid_point{};
    s64 detection_count{};
    u64 delta_time{};
    f32 average_distance{};
    f32 angle{};
};

// This is nn::hid::TouchState
struct TouchState {
    u64 delta_time{};
    Core::HID::TouchAttribute attribute{};
    u32 finger{};
    Common::Point<u32> position{};
    u32 diameter_x{};
    u32 diameter_y{};
    s32 rotation_angle{};
};
static_assert(sizeof(TouchState) == 0x28, "Touchstate is an invalid size");

// This is nn::hid::TouchScreenState
struct TouchScreenState {
    s64 sampling_number{};
    s32 entry_count{};
    INSERT_PADDING_BYTES(4); // Reserved
    std::array<TouchState, MaxFingers> states{};
};
static_assert(sizeof(TouchScreenState) == 0x290, "TouchScreenState is an invalid size");

struct TouchFingerMap {
    s32 finger_count{};
    Core::HID::TouchScreenModeForNx touch_mode;
    INSERT_PADDING_BYTES(3);
    std::array<u32, MaxFingers> finger_ids{};
};
static_assert(sizeof(TouchFingerMap) == 0x48, "TouchFingerMap is an invalid size");

struct TouchAruidData {
    u64 aruid;
    u32 basic_gesture_id;
    u64 used_1;
    u64 used_2;
    u64 used_3;
    u64 used_4;
    GestureType gesture_type;
    u16 resolution_width;
    u16 resolution_height;
    TouchFingerMap finger_map;
};
static_assert(sizeof(TouchAruidData) == 0x80, "TouchAruidData is an invalid size");

struct AutoPilotState {
    u64 count;
    std::array<TouchState, 16> state;
};
static_assert(sizeof(AutoPilotState) == 0x288, "AutoPilotState is an invalid size");

} // namespace Service::HID
