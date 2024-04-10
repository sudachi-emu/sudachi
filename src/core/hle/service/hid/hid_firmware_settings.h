// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "common/common_types.h"

namespace Service::HID {

/// Loads firmware config from nn::settings::fwdbg
class HidFirmwareSettings {
public:
    using FirmwareSetting = std::array<u8, 4>;
    using FeaturesPerId = std::array<bool, 0xA8>;

    HidFirmwareSettings();

    void Reload();
    void LoadSettings(bool reload_config);

    bool IsDebugPadEnabled();
    bool IsDeviceManaged();
    bool IsEmulateFutureDevice();
    bool IsTouchI2cManaged();
    bool IsHandheldForced();
    bool IsRailEnabled();
    bool IsHardwareErrorEmulated();
    bool IsBleDisabled();
    bool IsDscaleDisabled();
    bool IsTouchAutoUpdateDisabled();

    FirmwareSetting GetFirmwareUpdateFailure();
    FeaturesPerId FeaturesDisabledPerId();

private:
    bool is_initalized{};

    // Debug settings
    bool is_debug_pad_enabled{};
    bool is_device_managed{};
    bool is_touch_i2c_managed{};
    bool is_future_devices_emulated{};
    bool is_mcu_hardware_error_emulated{};
    bool is_rail_enabled{};
    bool is_firmware_update_failure_emulated{};
    bool is_ble_disabled{};
    bool is_dscale_disabled{};
    bool is_handheld_forced{};
    bool is_touch_firmware_auto_update_disabled{};
    FirmwareSetting is_firmware_update_failure{};
    FeaturesPerId features_per_id_disabled{};
};

} // namespace Service::HID
