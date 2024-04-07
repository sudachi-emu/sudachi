// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/hle/service/hid/hid_firmware_settings.h"

namespace Service::HID {

HidFirmwareSettings::HidFirmwareSettings() {
    LoadSettings(true);
}

void HidFirmwareSettings::Reload() {
    LoadSettings(true);
}

void HidFirmwareSettings::LoadSettings(bool reload_config) {
    if (is_initalized && !reload_config) {
        return;
    }

    // TODO: Use nn::settings::fwdbg::GetSettingsItemValue to load config values

    is_debug_pad_enabled = true;
    is_device_managed = true;
    is_touch_i2c_managed = is_device_managed;
    is_future_devices_emulated = false;
    is_mcu_hardware_error_emulated = false;
    is_rail_enabled = true;
    is_firmware_update_failure_emulated = false;
    is_firmware_update_failure = {};
    is_ble_disabled = false;
    is_dscale_disabled = false;
    is_handheld_forced = true;
    features_per_id_disabled = {};
    is_touch_firmware_auto_update_disabled = false;
    is_initalized = true;
}

bool HidFirmwareSettings::IsDebugPadEnabled() {
    LoadSettings(false);
    return is_debug_pad_enabled;
}

bool HidFirmwareSettings::IsDeviceManaged() {
    LoadSettings(false);
    return is_device_managed;
}

bool HidFirmwareSettings::IsEmulateFutureDevice() {
    LoadSettings(false);
    return is_future_devices_emulated;
}

bool HidFirmwareSettings::IsTouchI2cManaged() {
    LoadSettings(false);
    return is_touch_i2c_managed;
}

bool HidFirmwareSettings::IsHandheldForced() {
    LoadSettings(false);
    return is_handheld_forced;
}

bool HidFirmwareSettings::IsRailEnabled() {
    LoadSettings(false);
    return is_rail_enabled;
}

bool HidFirmwareSettings::IsHardwareErrorEmulated() {
    LoadSettings(false);
    return is_mcu_hardware_error_emulated;
}

bool HidFirmwareSettings::IsBleDisabled() {
    LoadSettings(false);
    return is_ble_disabled;
}

bool HidFirmwareSettings::IsDscaleDisabled() {
    LoadSettings(false);
    return is_dscale_disabled;
}

bool HidFirmwareSettings::IsTouchAutoUpdateDisabled() {
    LoadSettings(false);
    return is_touch_firmware_auto_update_disabled;
}

HidFirmwareSettings::FirmwareSetting HidFirmwareSettings::GetFirmwareUpdateFailure() {
    LoadSettings(false);
    return is_firmware_update_failure;
}

HidFirmwareSettings::FeaturesPerId HidFirmwareSettings::FeaturesDisabledPerId() {
    LoadSettings(false);
    return features_per_id_disabled;
}

} // namespace Service::HID
