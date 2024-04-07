// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "common/bit_field.h"
#include "common/common_funcs.h"
#include "common/common_types.h"
#include "core/hid/hid_types.h"

namespace Service::HID {
static constexpr std::size_t MaxSupportedNpadIdTypes = 10;
static constexpr std::size_t StyleIndexCount = 7;

// This is nn::hid::NpadJoyHoldType
enum class NpadJoyHoldType : u64 {
    Vertical = 0,
    Horizontal = 1,
};

// This is nn::hid::NpadJoyAssignmentMode
enum class NpadJoyAssignmentMode : u32 {
    Dual = 0,
    Single = 1,
};

// This is nn::hid::NpadJoyDeviceType
enum class NpadJoyDeviceType : s64 {
    Left = 0,
    Right = 1,
};

// This is nn::hid::NpadHandheldActivationMode
enum class NpadHandheldActivationMode : u64 {
    Dual = 0,
    Single = 1,
    None = 2,
    MaxActivationMode = 3,
};

// This is nn::hid::system::AppletFooterUiAttributesSet
struct AppletFooterUiAttributes {
    INSERT_PADDING_BYTES(0x4);
};

// This is nn::hid::system::AppletFooterUiType
enum class AppletFooterUiType : u8 {
    None = 0,
    HandheldNone = 1,
    HandheldJoyConLeftOnly = 2,
    HandheldJoyConRightOnly = 3,
    HandheldJoyConLeftJoyConRight = 4,
    JoyDual = 5,
    JoyDualLeftOnly = 6,
    JoyDualRightOnly = 7,
    JoyLeftHorizontal = 8,
    JoyLeftVertical = 9,
    JoyRightHorizontal = 10,
    JoyRightVertical = 11,
    SwitchProController = 12,
    CompatibleProController = 13,
    CompatibleJoyCon = 14,
    LarkHvc1 = 15,
    LarkHvc2 = 16,
    LarkNesLeft = 17,
    LarkNesRight = 18,
    Lucia = 19,
    Verification = 20,
    Lagon = 21,
};

using AppletFooterUiVariant = u8;

// This is "nn::hid::system::AppletDetailedUiType".
struct AppletDetailedUiType {
    AppletFooterUiVariant ui_variant;
    INSERT_PADDING_BYTES(0x2);
    AppletFooterUiType footer;
};
static_assert(sizeof(AppletDetailedUiType) == 0x4, "AppletDetailedUiType is an invalid size");
// This is nn::hid::NpadCommunicationMode
enum class NpadCommunicationMode : u64 {
    Mode_5ms = 0,
    Mode_10ms = 1,
    Mode_15ms = 2,
    Default = 3,
};

enum class NpadRevision : u32 {
    Revision0 = 0,
    Revision1 = 1,
    Revision2 = 2,
    Revision3 = 3,
};

// This is nn::hid::detail::ColorAttribute
enum class ColorAttribute : u32 {
    Ok = 0,
    ReadError = 1,
    NoController = 2,
};
static_assert(sizeof(ColorAttribute) == 4, "ColorAttribute is an invalid size");

// This is nn::hid::detail::NpadFullKeyColorState
struct NpadFullKeyColorState {
    ColorAttribute attribute{ColorAttribute::NoController};
    Core::HID::NpadControllerColor fullkey{};
};
static_assert(sizeof(NpadFullKeyColorState) == 0xC, "NpadFullKeyColorState is an invalid size");

// This is nn::hid::detail::NpadJoyColorState
struct NpadJoyColorState {
    ColorAttribute attribute{ColorAttribute::NoController};
    Core::HID::NpadControllerColor left{};
    Core::HID::NpadControllerColor right{};
};
static_assert(sizeof(NpadJoyColorState) == 0x14, "NpadJoyColorState is an invalid size");

// This is nn::hid::NpadAttribute
struct NpadAttribute {
    union {
        u32 raw{};
        BitField<0, 1, u32> is_connected;
        BitField<1, 1, u32> is_wired;
        BitField<2, 1, u32> is_left_connected;
        BitField<3, 1, u32> is_left_wired;
        BitField<4, 1, u32> is_right_connected;
        BitField<5, 1, u32> is_right_wired;
    };
};
static_assert(sizeof(NpadAttribute) == 4, "NpadAttribute is an invalid size");

// This is nn::hid::NpadFullKeyState
// This is nn::hid::NpadHandheldState
// This is nn::hid::NpadJoyDualState
// This is nn::hid::NpadJoyLeftState
// This is nn::hid::NpadJoyRightState
// This is nn::hid::NpadPalmaState
// This is nn::hid::NpadSystemExtState
struct NPadGenericState {
    s64_le sampling_number{};
    Core::HID::NpadButtonState npad_buttons{};
    Core::HID::AnalogStickState l_stick{};
    Core::HID::AnalogStickState r_stick{};
    NpadAttribute connection_status{};
    INSERT_PADDING_BYTES(4); // Reserved
};
static_assert(sizeof(NPadGenericState) == 0x28, "NPadGenericState is an invalid size");

// This is nn::hid::server::NpadGcTriggerState
struct NpadGcTriggerState {
    s64 sampling_number{};
    s32 l_analog{};
    s32 r_analog{};
};
static_assert(sizeof(NpadGcTriggerState) == 0x10, "NpadGcTriggerState is an invalid size");

// This is nn::hid::NpadSystemProperties
struct NPadSystemProperties {
    union {
        s64 raw{};
        BitField<0, 1, s64> is_charging_joy_dual;
        BitField<1, 1, s64> is_charging_joy_left;
        BitField<2, 1, s64> is_charging_joy_right;
        BitField<3, 1, s64> is_powered_joy_dual;
        BitField<4, 1, s64> is_powered_joy_left;
        BitField<5, 1, s64> is_powered_joy_right;
        BitField<9, 1, s64> is_system_unsupported_button;
        BitField<10, 1, s64> is_system_ext_unsupported_button;
        BitField<11, 1, s64> is_vertical;
        BitField<12, 1, s64> is_horizontal;
        BitField<13, 1, s64> use_plus;
        BitField<14, 1, s64> use_minus;
        BitField<15, 1, s64> use_directional_buttons;
    };
};
static_assert(sizeof(NPadSystemProperties) == 0x8, "NPadSystemProperties is an invalid size");

// This is nn::hid::NpadSystemButtonProperties
struct NpadSystemButtonProperties {
    union {
        s32 raw{};
        BitField<0, 1, s32> is_home_button_protection_enabled;
    };
};
static_assert(sizeof(NpadSystemButtonProperties) == 0x4, "NPadButtonProperties is an invalid size");

// This is nn::hid::system::DeviceType
struct DeviceType {
    union {
        u32 raw{};
        BitField<0, 1, s32> fullkey;
        BitField<1, 1, s32> debug_pad;
        BitField<2, 1, s32> handheld_left;
        BitField<3, 1, s32> handheld_right;
        BitField<4, 1, s32> joycon_left;
        BitField<5, 1, s32> joycon_right;
        BitField<6, 1, s32> palma;
        BitField<7, 1, s32> lark_hvc_left;
        BitField<8, 1, s32> lark_hvc_right;
        BitField<9, 1, s32> lark_nes_left;
        BitField<10, 1, s32> lark_nes_right;
        BitField<11, 1, s32> handheld_lark_hvc_left;
        BitField<12, 1, s32> handheld_lark_hvc_right;
        BitField<13, 1, s32> handheld_lark_nes_left;
        BitField<14, 1, s32> handheld_lark_nes_right;
        BitField<15, 1, s32> lucia;
        BitField<16, 1, s32> lagon;
        BitField<17, 1, s32> lager;
        BitField<31, 1, s32> system;
    };
};

// This is nn::hid::detail::NfcXcdDeviceHandleStateImpl
struct NfcXcdDeviceHandleStateImpl {
    u64 handle{};
    bool is_available{};
    bool is_activated{};
    INSERT_PADDING_BYTES(0x6); // Reserved
    u64 sampling_number{};
};
static_assert(sizeof(NfcXcdDeviceHandleStateImpl) == 0x18,
              "NfcXcdDeviceHandleStateImpl is an invalid size");

// This is nn::hid::NpadLarkType
enum class NpadLarkType : u32 {
    Invalid,
    H1,
    H2,
    NL,
    NR,
};

// This is nn::hid::NpadLuciaType
enum class NpadLuciaType : u32 {
    Invalid,
    J,
    E,
    U,
};

// This is nn::hid::NpadLagonType
enum class NpadLagonType : u32 {
    Invalid,
};

// This is nn::hid::NpadLagerType
enum class NpadLagerType : u32 {
    Invalid,
    J,
    E,
    U,
};

} // namespace Service::HID
