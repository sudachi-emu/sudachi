// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "common/common_funcs.h"
#include "common/common_types.h"
#include "common/vector_math.h"
#include "hid_core/hid_types.h"
#include "hid_core/resources/debug_pad/debug_pad_types.h"
#include "hid_core/resources/keyboard/keyboard_types.h"
#include "hid_core/resources/mouse/mouse_types.h"
#include "hid_core/resources/npad/npad_types.h"
#include "hid_core/resources/ring_lifo.h"
#include "hid_core/resources/system_buttons/system_button_types.h"
#include "hid_core/resources/touch_screen/touch_types.h"

namespace Service::HID {
static const std::size_t HidEntryCount = 17;

struct CommonHeader {
    s64 timestamp{};
    s64 total_entry_count{};
    s64 last_entry_index{};
    s64 entry_count{};
};
static_assert(sizeof(CommonHeader) == 0x20, "CommonHeader is an invalid size");

// This is nn::hid::detail::DebugPadSharedMemoryFormat
struct DebugPadSharedMemoryFormat {
    // This is nn::hid::detail::DebugPadLifo
    Lifo<DebugPadState, HidEntryCount> debug_pad_lifo{};
    static_assert(sizeof(debug_pad_lifo) == 0x2C8, "debug_pad_lifo is an invalid size");
    INSERT_PADDING_WORDS(0x4E);
};
static_assert(sizeof(DebugPadSharedMemoryFormat) == 0x400,
              "DebugPadSharedMemoryFormat is an invalid size");

// This is nn::hid::detail::TouchScreenSharedMemoryFormat
struct TouchScreenSharedMemoryFormat {
    // This is nn::hid::detail::TouchScreenLifo
    Lifo<TouchScreenState, HidEntryCount> touch_screen_lifo{};
    static_assert(sizeof(touch_screen_lifo) == 0x2C38, "touch_screen_lifo is an invalid size");
    INSERT_PADDING_WORDS(0xF2);
};
static_assert(sizeof(TouchScreenSharedMemoryFormat) == 0x3000,
              "TouchScreenSharedMemoryFormat is an invalid size");

// This is nn::hid::detail::MouseSharedMemoryFormat
struct MouseSharedMemoryFormat {
    // This is nn::hid::detail::MouseLifo
    Lifo<Core::HID::MouseState, HidEntryCount> mouse_lifo{};
    static_assert(sizeof(mouse_lifo) == 0x350, "mouse_lifo is an invalid size");
    INSERT_PADDING_WORDS(0x2C);
};
static_assert(sizeof(MouseSharedMemoryFormat) == 0x400,
              "MouseSharedMemoryFormat is an invalid size");

// This is nn::hid::detail::KeyboardSharedMemoryFormat
struct KeyboardSharedMemoryFormat {
    // This is nn::hid::detail::KeyboardLifo
    Lifo<KeyboardState, HidEntryCount> keyboard_lifo{};
    static_assert(sizeof(keyboard_lifo) == 0x3D8, "keyboard_lifo is an invalid size");
    INSERT_PADDING_WORDS(0xA);
};
static_assert(sizeof(KeyboardSharedMemoryFormat) == 0x400,
              "KeyboardSharedMemoryFormat is an invalid size");

// This is nn::hid::detail::DigitizerSharedMemoryFormat
struct DigitizerSharedMemoryFormat {
    CommonHeader header;
    INSERT_PADDING_BYTES(0xFE0);
};
static_assert(sizeof(DigitizerSharedMemoryFormat) == 0x1000,
              "DigitizerSharedMemoryFormat is an invalid size");

// This is nn::hid::detail::HomeButtonSharedMemoryFormat
struct HomeButtonSharedMemoryFormat {
    Lifo<HomeButtonState, HidEntryCount> home_lifo{};
    INSERT_PADDING_BYTES(0x48);
};
static_assert(sizeof(HomeButtonSharedMemoryFormat) == 0x200,
              "HomeButtonSharedMemoryFormat is an invalid size");

// This is nn::hid::detail::SleepButtonSharedMemoryFormat
struct SleepButtonSharedMemoryFormat {
    Lifo<SleepButtonState, HidEntryCount> sleep_lifo{};
    INSERT_PADDING_BYTES(0x48);
};
static_assert(sizeof(SleepButtonSharedMemoryFormat) == 0x200,
              "SleepButtonSharedMemoryFormat is an invalid size");

// This is nn::hid::detail::CaptureButtonSharedMemoryFormat
struct CaptureButtonSharedMemoryFormat {
    Lifo<CaptureButtonState, HidEntryCount> capture_lifo{};
    INSERT_PADDING_BYTES(0x48);
};
static_assert(sizeof(CaptureButtonSharedMemoryFormat) == 0x200,
              "CaptureButtonSharedMemoryFormat is an invalid size");

// This is nn::hid::detail::InputDetectorSharedMemoryFormat
struct InputDetectorSharedMemoryFormat {
    CommonHeader header;
    INSERT_PADDING_BYTES(0x7E0);
};
static_assert(sizeof(InputDetectorSharedMemoryFormat) == 0x800,
              "InputDetectorSharedMemoryFormat is an invalid size");

// This is nn::hid::detail::UniquePadSharedMemoryFormat
struct UniquePadSharedMemoryFormat {
    CommonHeader header;
    INSERT_PADDING_BYTES(0x3FE0);
};
static_assert(sizeof(UniquePadSharedMemoryFormat) == 0x4000,
              "UniquePadSharedMemoryFormat is an invalid size");

// This is nn::hid::detail::NpadSixAxisSensorLifo
struct NpadSixAxisSensorLifo {
    Lifo<Core::HID::SixAxisSensorState, HidEntryCount> lifo;
};

// This is nn::hid::detail::NpadInternalState
struct NpadInternalState {
    Core::HID::NpadStyleTag style_tag{Core::HID::NpadStyleSet::None};
    NpadJoyAssignmentMode assignment_mode{NpadJoyAssignmentMode::Dual};
    NpadFullKeyColorState fullkey_color{};
    NpadJoyColorState joycon_color{};
    Lifo<NPadGenericState, HidEntryCount> fullkey_lifo{};
    Lifo<NPadGenericState, HidEntryCount> handheld_lifo{};
    Lifo<NPadGenericState, HidEntryCount> joy_dual_lifo{};
    Lifo<NPadGenericState, HidEntryCount> joy_left_lifo{};
    Lifo<NPadGenericState, HidEntryCount> joy_right_lifo{};
    Lifo<NPadGenericState, HidEntryCount> palma_lifo{};
    Lifo<NPadGenericState, HidEntryCount> system_ext_lifo{};
    NpadSixAxisSensorLifo sixaxis_fullkey_lifo{};
    NpadSixAxisSensorLifo sixaxis_handheld_lifo{};
    NpadSixAxisSensorLifo sixaxis_dual_left_lifo{};
    NpadSixAxisSensorLifo sixaxis_dual_right_lifo{};
    NpadSixAxisSensorLifo sixaxis_left_lifo{};
    NpadSixAxisSensorLifo sixaxis_right_lifo{};
    DeviceType device_type{};
    INSERT_PADDING_BYTES(0x4); // Reserved
    NPadSystemProperties system_properties{};
    NpadSystemButtonProperties button_properties{};
    Core::HID::NpadBatteryLevel battery_level_dual{};
    Core::HID::NpadBatteryLevel battery_level_left{};
    Core::HID::NpadBatteryLevel battery_level_right{};
    AppletFooterUiAttributes applet_footer_attributes{};
    AppletFooterUiType applet_footer_type{AppletFooterUiType::None};
    INSERT_PADDING_BYTES(0x5B); // Reserved
    INSERT_PADDING_BYTES(0x20); // Unknown
    Lifo<NpadGcTriggerState, HidEntryCount> gc_trigger_lifo{};
    NpadLarkType lark_type_l_and_main{};
    NpadLarkType lark_type_r{};
    NpadLuciaType lucia_type{};
    NpadLagerType lager_type{};
    Core::HID::SixAxisSensorProperties sixaxis_fullkey_properties;
    Core::HID::SixAxisSensorProperties sixaxis_handheld_properties;
    Core::HID::SixAxisSensorProperties sixaxis_dual_left_properties;
    Core::HID::SixAxisSensorProperties sixaxis_dual_right_properties;
    Core::HID::SixAxisSensorProperties sixaxis_left_properties;
    Core::HID::SixAxisSensorProperties sixaxis_right_properties;
};
static_assert(sizeof(NpadInternalState) == 0x43F8, "NpadInternalState is an invalid size");

// This is nn::hid::detail::NpadSharedMemoryEntry
struct NpadSharedMemoryEntry {
    NpadInternalState internal_state;
    INSERT_PADDING_BYTES(0xC08);
};
static_assert(sizeof(NpadSharedMemoryEntry) == 0x5000, "NpadSharedMemoryEntry is an invalid size");

// This is nn::hid::detail::NpadSharedMemoryFormat
struct NpadSharedMemoryFormat {
    std::array<NpadSharedMemoryEntry, MaxSupportedNpadIdTypes> npad_entry;
};
static_assert(sizeof(NpadSharedMemoryFormat) == 0x32000,
              "NpadSharedMemoryFormat is an invalid size");

// This is nn::hid::detail::GestureSharedMemoryFormat
struct GestureSharedMemoryFormat {
    // This is nn::hid::detail::GestureLifo
    Lifo<GestureState, HidEntryCount> gesture_lifo{};
    static_assert(sizeof(gesture_lifo) == 0x708, "gesture_lifo is an invalid size");
    INSERT_PADDING_WORDS(0x3E);
};
static_assert(sizeof(GestureSharedMemoryFormat) == 0x800,
              "GestureSharedMemoryFormat is an invalid size");

// This is nn::hid::detail::ConsoleSixAxisSensorSharedMemoryFormat
struct ConsoleSixAxisSensorSharedMemoryFormat {
    u64 sampling_number{};
    bool is_seven_six_axis_sensor_at_rest{};
    INSERT_PADDING_BYTES(3); // padding
    f32 verticalization_error{};
    Common::Vec3f gyro_bias{};
    INSERT_PADDING_BYTES(4); // padding
};
static_assert(sizeof(ConsoleSixAxisSensorSharedMemoryFormat) == 0x20,
              "ConsoleSixAxisSensorSharedMemoryFormat is an invalid size");

// This is nn::hid::detail::SharedMemoryFormat
struct SharedMemoryFormat {
    void Initialize() {}

    DebugPadSharedMemoryFormat debug_pad;
    TouchScreenSharedMemoryFormat touch_screen;
    MouseSharedMemoryFormat mouse;
    KeyboardSharedMemoryFormat keyboard;
    DigitizerSharedMemoryFormat digitizer;
    HomeButtonSharedMemoryFormat home_button;
    SleepButtonSharedMemoryFormat sleep_button;
    CaptureButtonSharedMemoryFormat capture_button;
    InputDetectorSharedMemoryFormat input_detector;
    UniquePadSharedMemoryFormat unique_pad;
    NpadSharedMemoryFormat npad;
    GestureSharedMemoryFormat gesture;
    ConsoleSixAxisSensorSharedMemoryFormat console;
    INSERT_PADDING_BYTES(0x19E0);
    MouseSharedMemoryFormat debug_mouse;
    INSERT_PADDING_BYTES(0x2000);
};
static_assert(offsetof(SharedMemoryFormat, debug_pad) == 0x0, "debug_pad has wrong offset");
static_assert(offsetof(SharedMemoryFormat, touch_screen) == 0x400, "touch_screen has wrong offset");
static_assert(offsetof(SharedMemoryFormat, mouse) == 0x3400, "mouse has wrong offset");
static_assert(offsetof(SharedMemoryFormat, keyboard) == 0x3800, "keyboard has wrong offset");
static_assert(offsetof(SharedMemoryFormat, digitizer) == 0x3C00, "digitizer has wrong offset");
static_assert(offsetof(SharedMemoryFormat, home_button) == 0x4C00, "home_button has wrong offset");
static_assert(offsetof(SharedMemoryFormat, sleep_button) == 0x4E00,
              "sleep_button has wrong offset");
static_assert(offsetof(SharedMemoryFormat, capture_button) == 0x5000,
              "capture_button has wrong offset");
static_assert(offsetof(SharedMemoryFormat, input_detector) == 0x5200,
              "input_detector has wrong offset");
static_assert(offsetof(SharedMemoryFormat, npad) == 0x9A00, "npad has wrong offset");
static_assert(offsetof(SharedMemoryFormat, gesture) == 0x3BA00, "gesture has wrong offset");
static_assert(offsetof(SharedMemoryFormat, console) == 0x3C200, "console has wrong offset");
static_assert(offsetof(SharedMemoryFormat, debug_mouse) == 0x3DC00, "debug_mouse has wrong offset");
static_assert(sizeof(SharedMemoryFormat) == 0x40000, "SharedMemoryFormat is an invalid size");

} // namespace Service::HID
