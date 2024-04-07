// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "core/hid/hid_types.h"
#include "core/hle/service/hid/errors.h"

namespace Service::HID {

constexpr bool IsNpadIdValid(const Core::HID::NpadIdType npad_id) {
    switch (npad_id) {
    case Core::HID::NpadIdType::Player1:
    case Core::HID::NpadIdType::Player2:
    case Core::HID::NpadIdType::Player3:
    case Core::HID::NpadIdType::Player4:
    case Core::HID::NpadIdType::Player5:
    case Core::HID::NpadIdType::Player6:
    case Core::HID::NpadIdType::Player7:
    case Core::HID::NpadIdType::Player8:
    case Core::HID::NpadIdType::Other:
    case Core::HID::NpadIdType::Handheld:
        return true;
    default:
        return false;
    }
}

constexpr Result IsSixaxisHandleValid(const Core::HID::SixAxisSensorHandle& handle) {
    const auto npad_id = IsNpadIdValid(static_cast<Core::HID::NpadIdType>(handle.npad_id));
    const bool device_index = handle.device_index < Core::HID::DeviceIndex::MaxDeviceIndex;

    if (!npad_id) {
        return ResultInvalidNpadId;
    }
    if (!device_index) {
        return NpadDeviceIndexOutOfRange;
    }

    return ResultSuccess;
}

constexpr Result IsVibrationHandleValid(const Core::HID::VibrationDeviceHandle& handle) {
    switch (handle.npad_type) {
    case Core::HID::NpadStyleIndex::ProController:
    case Core::HID::NpadStyleIndex::Handheld:
    case Core::HID::NpadStyleIndex::JoyconDual:
    case Core::HID::NpadStyleIndex::JoyconLeft:
    case Core::HID::NpadStyleIndex::JoyconRight:
    case Core::HID::NpadStyleIndex::GameCube:
    case Core::HID::NpadStyleIndex::N64:
    case Core::HID::NpadStyleIndex::SystemExt:
    case Core::HID::NpadStyleIndex::System:
        // These support vibration
        break;
    default:
        return ResultVibrationInvalidStyleIndex;
    }

    if (!IsNpadIdValid(static_cast<Core::HID::NpadIdType>(handle.npad_id))) {
        return ResultVibrationInvalidNpadId;
    }

    if (handle.device_index >= Core::HID::DeviceIndex::MaxDeviceIndex) {
        return ResultVibrationDeviceIndexOutOfRange;
    }

    return ResultSuccess;
}

/// Converts a Core::HID::NpadIdType to an array index.
constexpr size_t NpadIdTypeToIndex(Core::HID::NpadIdType npad_id_type) {
    switch (npad_id_type) {
    case Core::HID::NpadIdType::Player1:
        return 0;
    case Core::HID::NpadIdType::Player2:
        return 1;
    case Core::HID::NpadIdType::Player3:
        return 2;
    case Core::HID::NpadIdType::Player4:
        return 3;
    case Core::HID::NpadIdType::Player5:
        return 4;
    case Core::HID::NpadIdType::Player6:
        return 5;
    case Core::HID::NpadIdType::Player7:
        return 6;
    case Core::HID::NpadIdType::Player8:
        return 7;
    case Core::HID::NpadIdType::Handheld:
        return 8;
    case Core::HID::NpadIdType::Other:
        return 9;
    default:
        return 8;
    }
}

/// Converts an array index to a Core::HID::NpadIdType
constexpr Core::HID::NpadIdType IndexToNpadIdType(size_t index) {
    switch (index) {
    case 0:
        return Core::HID::NpadIdType::Player1;
    case 1:
        return Core::HID::NpadIdType::Player2;
    case 2:
        return Core::HID::NpadIdType::Player3;
    case 3:
        return Core::HID::NpadIdType::Player4;
    case 4:
        return Core::HID::NpadIdType::Player5;
    case 5:
        return Core::HID::NpadIdType::Player6;
    case 6:
        return Core::HID::NpadIdType::Player7;
    case 7:
        return Core::HID::NpadIdType::Player8;
    case 8:
        return Core::HID::NpadIdType::Handheld;
    case 9:
        return Core::HID::NpadIdType::Other;
    default:
        return Core::HID::NpadIdType::Invalid;
    }
}

constexpr Core::HID::NpadStyleSet GetStylesetByIndex(std::size_t index) {
    switch (index) {
    case 0:
        return Core::HID::NpadStyleSet::Fullkey;
    case 1:
        return Core::HID::NpadStyleSet::Handheld;
    case 2:
        return Core::HID::NpadStyleSet::JoyDual;
    case 3:
        return Core::HID::NpadStyleSet::JoyLeft;
    case 4:
        return Core::HID::NpadStyleSet::JoyRight;
    case 5:
        return Core::HID::NpadStyleSet::Palma;
    default:
        return Core::HID::NpadStyleSet::None;
    }
}

} // namespace Service::HID
