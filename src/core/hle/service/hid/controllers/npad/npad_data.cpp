// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/hle/service/hid/controllers/npad/npad_data.h"
#include "core/hle/service/hid/hid_util.h"

namespace Service::HID {

NPadData::NPadData() {
    ClearNpadSystemCommonPolicy();
}

NPadData::~NPadData() = default;

NpadStatus NPadData::GetNpadStatus() const {
    return status;
}

void NPadData::SetNpadAnalogStickUseCenterClamp(bool is_enabled) {
    status.use_center_clamp.Assign(is_enabled);
}

bool NPadData::GetNpadAnalogStickUseCenterClamp() const {
    return status.use_center_clamp.As<bool>();
}

void NPadData::SetNpadSystemExtStateEnabled(bool is_enabled) {
    status.system_ext_state.Assign(is_enabled);
}

bool NPadData::GetNpadSystemExtState() const {
    return status.system_ext_state.As<bool>();
}

Result NPadData::SetSupportedNpadIdType(std::span<const Core::HID::NpadIdType> list) {
    // Note: Real limit is 11. But array size is 10. N's bug?
    if (list.size() > MaxSupportedNpadIdTypes) {
        return ResultInvalidArraySize;
    }

    supported_npad_id_types_count = list.size();
    memcpy(supported_npad_id_types.data(), list.data(),
           list.size() * sizeof(Core::HID::NpadIdType));

    return ResultSuccess;
}

std::size_t NPadData::GetSupportedNpadIdType(std::span<Core::HID::NpadIdType> out_list) const {
    std::size_t out_size = std::min(supported_npad_id_types_count, out_list.size());

    memcpy(out_list.data(), supported_npad_id_types.data(),
           out_size * sizeof(Core::HID::NpadIdType));

    return out_size;
}

bool NPadData::IsNpadIdTypeSupported(Core::HID::NpadIdType npad_id) const {
    for (std::size_t i = 0; i < supported_npad_id_types_count; i++) {
        if (supported_npad_id_types[i] == npad_id) {
            return true;
        }
    }

    return false;
}

void NPadData::SetNpadSystemCommonPolicy(bool is_full_policy) {
    supported_npad_style_set = Core::HID::NpadStyleSet::Fullkey | Core::HID::NpadStyleSet::JoyDual |
                               Core::HID::NpadStyleSet::SystemExt | Core::HID::NpadStyleSet::System;
    handheld_activation_mode = NpadHandheldActivationMode::Dual;

    status.is_supported_styleset_set.Assign(true);
    status.is_hold_type_set.Assign(true);
    status.lr_assignment_mode.Assign(false);
    status.is_policy.Assign(true);
    if (is_full_policy) {
        status.is_full_policy.Assign(true);
    }

    supported_npad_id_types_count = 10;
    supported_npad_id_types[0] = Core::HID::NpadIdType::Player1;
    supported_npad_id_types[1] = Core::HID::NpadIdType::Player2;
    supported_npad_id_types[2] = Core::HID::NpadIdType::Player3;
    supported_npad_id_types[3] = Core::HID::NpadIdType::Player4;
    supported_npad_id_types[4] = Core::HID::NpadIdType::Player5;
    supported_npad_id_types[5] = Core::HID::NpadIdType::Player6;
    supported_npad_id_types[6] = Core::HID::NpadIdType::Player7;
    supported_npad_id_types[7] = Core::HID::NpadIdType::Player8;
    supported_npad_id_types[8] = Core::HID::NpadIdType::Other;
    supported_npad_id_types[9] = Core::HID::NpadIdType::Handheld;

    for (auto& input_protection : is_unintended_home_button_input_protection) {
        input_protection = true;
    }
}

void NPadData::ClearNpadSystemCommonPolicy() {
    status.raw = 0;
    supported_npad_style_set = Core::HID::NpadStyleSet::All;
    npad_hold_type = NpadJoyHoldType::Vertical;
    handheld_activation_mode = NpadHandheldActivationMode::Dual;

    for (auto& button_assignment : npad_button_assignment) {
        button_assignment = Core::HID::NpadButton::None;
    }

    supported_npad_id_types_count = 10;
    supported_npad_id_types[0] = Core::HID::NpadIdType::Player1;
    supported_npad_id_types[1] = Core::HID::NpadIdType::Player2;
    supported_npad_id_types[2] = Core::HID::NpadIdType::Player3;
    supported_npad_id_types[3] = Core::HID::NpadIdType::Player4;
    supported_npad_id_types[4] = Core::HID::NpadIdType::Player5;
    supported_npad_id_types[5] = Core::HID::NpadIdType::Player6;
    supported_npad_id_types[6] = Core::HID::NpadIdType::Player7;
    supported_npad_id_types[7] = Core::HID::NpadIdType::Player8;
    supported_npad_id_types[8] = Core::HID::NpadIdType::Other;
    supported_npad_id_types[9] = Core::HID::NpadIdType::Handheld;

    for (auto& input_protection : is_unintended_home_button_input_protection) {
        input_protection = true;
    }
}

void NPadData::SetNpadJoyHoldType(NpadJoyHoldType hold_type) {
    npad_hold_type = hold_type;
    status.is_hold_type_set.Assign(true);
}

NpadJoyHoldType NPadData::GetNpadJoyHoldType() const {
    return npad_hold_type;
}

void NPadData::SetHandheldActivationMode(NpadHandheldActivationMode activation_mode) {
    handheld_activation_mode = activation_mode;
}

NpadHandheldActivationMode NPadData::GetHandheldActivationMode() const {
    return handheld_activation_mode;
}

void NPadData::SetSupportedNpadStyleSet(Core::HID::NpadStyleSet style_set) {
    supported_npad_style_set = style_set;
    status.is_supported_styleset_set.Assign(true);
    status.is_hold_type_set.Assign(true);
}

Core::HID::NpadStyleSet NPadData::GetSupportedNpadStyleSet() const {
    return supported_npad_style_set;
}

bool NPadData::IsNpadStyleIndexSupported(Core::HID::NpadStyleIndex style_index) const {
    Core::HID::NpadStyleTag style = {supported_npad_style_set};
    switch (style_index) {
    case Core::HID::NpadStyleIndex::ProController:
        return style.fullkey.As<bool>();
    case Core::HID::NpadStyleIndex::Handheld:
        return style.handheld.As<bool>();
    case Core::HID::NpadStyleIndex::JoyconDual:
        return style.joycon_dual.As<bool>();
    case Core::HID::NpadStyleIndex::JoyconLeft:
        return style.joycon_left.As<bool>();
    case Core::HID::NpadStyleIndex::JoyconRight:
        return style.joycon_right.As<bool>();
    case Core::HID::NpadStyleIndex::GameCube:
        return style.gamecube.As<bool>();
    case Core::HID::NpadStyleIndex::Pokeball:
        return style.palma.As<bool>();
    case Core::HID::NpadStyleIndex::NES:
        return style.lark.As<bool>();
    case Core::HID::NpadStyleIndex::SNES:
        return style.lucia.As<bool>();
    case Core::HID::NpadStyleIndex::N64:
        return style.lagoon.As<bool>();
    case Core::HID::NpadStyleIndex::SegaGenesis:
        return style.lager.As<bool>();
    default:
        return false;
    }
}

void NPadData::SetLrAssignmentMode(bool is_enabled) {
    status.lr_assignment_mode.Assign(is_enabled);
}

bool NPadData::GetLrAssignmentMode() const {
    return status.lr_assignment_mode.As<bool>();
}

void NPadData::SetAssigningSingleOnSlSrPress(bool is_enabled) {
    status.assigning_single_on_sl_sr_press.Assign(is_enabled);
}

bool NPadData::GetAssigningSingleOnSlSrPress() const {
    return status.assigning_single_on_sl_sr_press.As<bool>();
}

void NPadData::SetHomeProtectionEnabled(bool is_enabled, Core::HID::NpadIdType npad_id) {
    is_unintended_home_button_input_protection[NpadIdTypeToIndex(npad_id)] = is_enabled;
}

bool NPadData::GetHomeProtectionEnabled(Core::HID::NpadIdType npad_id) const {
    return is_unintended_home_button_input_protection[NpadIdTypeToIndex(npad_id)];
}

void NPadData::SetCaptureButtonAssignment(Core::HID::NpadButton button_assignment,
                                          std::size_t style_index) {
    npad_button_assignment[style_index] = button_assignment;
}

Core::HID::NpadButton NPadData::GetCaptureButtonAssignment(std::size_t style_index) const {
    return npad_button_assignment[style_index];
}

std::size_t NPadData::GetNpadCaptureButtonAssignmentList(
    std::span<Core::HID::NpadButton> out_list) const {
    for (std::size_t i = 0; i < out_list.size(); i++) {
        Core::HID::NpadStyleSet style_set = GetStylesetByIndex(i);
        if ((style_set & supported_npad_style_set) == Core::HID::NpadStyleSet::None ||
            npad_button_assignment[i] == Core::HID::NpadButton::None) {
            return i;
        }
        out_list[i] = npad_button_assignment[i];
    }

    return out_list.size();
}

} // namespace Service::HID
