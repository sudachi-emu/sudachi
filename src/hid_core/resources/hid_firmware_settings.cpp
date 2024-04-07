// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/hle/service/set/system_settings_server.h"
#include "core/hle/service/sm/sm.h"
#include "hid_core/resources/hid_firmware_settings.h"

namespace Service::HID {

HidFirmwareSettings::HidFirmwareSettings(Core::System& system) {
    m_set_sys =
        system.ServiceManager().GetService<Service::Set::ISystemSettingsServer>("set:sys", true);
    LoadSettings(true);
}

void HidFirmwareSettings::Reload() {
    LoadSettings(true);
}

void HidFirmwareSettings::LoadSettings(bool reload_config) {
    if (is_initialized && !reload_config) {
        return;
    }

    m_set_sys->GetSettingsItemValueImpl<bool>(is_debug_pad_enabled, "hid_debug",
                                              "enables_debugpad");
    m_set_sys->GetSettingsItemValueImpl<bool>(is_device_managed, "hid_debug", "manages_devices");
    m_set_sys->GetSettingsItemValueImpl<bool>(is_touch_i2c_managed, "hid_debug",
                                              "manages_touch_ic_i2c");
    m_set_sys->GetSettingsItemValueImpl<bool>(is_future_devices_emulated, "hid_debug",
                                              "emulate_future_device");
    m_set_sys->GetSettingsItemValueImpl<bool>(is_mcu_hardware_error_emulated, "hid_debug",
                                              "emulate_mcu_hardware_error");
    m_set_sys->GetSettingsItemValueImpl<bool>(is_rail_enabled, "hid_debug", "enables_rail");
    m_set_sys->GetSettingsItemValueImpl<bool>(is_firmware_update_failure_emulated, "hid_debug",
                                              "emulate_firmware_update_failure");
    is_firmware_update_failure = {};
    m_set_sys->GetSettingsItemValueImpl<bool>(is_ble_disabled, "hid_debug", "ble_disabled");
    m_set_sys->GetSettingsItemValueImpl<bool>(is_dscale_disabled, "hid_debug", "dscale_disabled");
    m_set_sys->GetSettingsItemValueImpl<bool>(is_handheld_forced, "hid_debug", "force_handheld");
    features_per_id_disabled = {};
    m_set_sys->GetSettingsItemValueImpl<bool>(is_touch_firmware_auto_update_disabled, "hid_debug",
                                              "touch_firmware_auto_update_disabled");

    bool has_rail_interface{};
    bool has_sio_mcu{};
    m_set_sys->GetSettingsItemValueImpl<bool>(has_rail_interface, "hid", "has_rail_interface");
    m_set_sys->GetSettingsItemValueImpl<bool>(has_sio_mcu, "hid", "has_sio_mcu");
    platform_config.has_rail_interface.Assign(has_rail_interface);
    platform_config.has_sio_mcu.Assign(has_sio_mcu);

    is_initialized = true;
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

Set::PlatformConfig HidFirmwareSettings::GetPlatformConfig() {
    LoadSettings(false);
    return platform_config;
}

} // namespace Service::HID
