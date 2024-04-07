// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "core/hle/service/kernel_helpers.h"
#include "core/hle/service/service.h"

namespace Core {
class System;
}

namespace Kernel {
class KEvent;
}

namespace Service::HID {
class ResourceManager;
class HidFirmwareSettings;

class IHidSystemServer final : public ServiceFramework<IHidSystemServer> {
public:
    explicit IHidSystemServer(Core::System& system_, std::shared_ptr<ResourceManager> resource,
                              std::shared_ptr<HidFirmwareSettings> settings);
    ~IHidSystemServer() override;

private:
    void GetPlatformConfig(HLERequestContext& ctx);
    void ApplyNpadSystemCommonPolicy(HLERequestContext& ctx);
    void EnableAssigningSingleOnSlSrPress(HLERequestContext& ctx);
    void DisableAssigningSingleOnSlSrPress(HLERequestContext& ctx);
    void GetLastActiveNpad(HLERequestContext& ctx);
    void ApplyNpadSystemCommonPolicyFull(HLERequestContext& ctx);
    void GetNpadFullKeyGripColor(HLERequestContext& ctx);
    void GetMaskedSupportedNpadStyleSet(HLERequestContext& ctx);
    void SetSupportedNpadStyleSetAll(HLERequestContext& ctx);
    void GetNpadCaptureButtonAssignment(HLERequestContext& ctx);
    void GetAppletDetailedUiType(HLERequestContext& ctx);
    void GetNpadInterfaceType(HLERequestContext& ctx);
    void GetNpadLeftRightInterfaceType(HLERequestContext& ctx);
    void HasBattery(HLERequestContext& ctx);
    void HasLeftRightBattery(HLERequestContext& ctx);
    void GetUniquePadsFromNpad(HLERequestContext& ctx);
    void SetNpadSystemExtStateEnabled(HLERequestContext& ctx);
    void RegisterAppletResourceUserId(HLERequestContext& ctx);
    void UnregisterAppletResourceUserId(HLERequestContext& ctx);
    void EnableAppletToGetInput(HLERequestContext& ctx);
    void SetAruidValidForVibration(HLERequestContext& ctx);
    void EnableAppletToGetSixAxisSensor(HLERequestContext& ctx);
    void EnableAppletToGetPadInput(HLERequestContext& ctx);
    void EnableAppletToGetTouchScreen(HLERequestContext& ctx);
    void SetVibrationMasterVolume(HLERequestContext& ctx);
    void GetVibrationMasterVolume(HLERequestContext& ctx);
    void BeginPermitVibrationSession(HLERequestContext& ctx);
    void EndPermitVibrationSession(HLERequestContext& ctx);
    void IsJoyConRailEnabled(HLERequestContext& ctx);
    void IsJoyConAttachedOnAllRail(HLERequestContext& ctx);
    void AcquireConnectionTriggerTimeoutEvent(HLERequestContext& ctx);
    void AcquireDeviceRegisteredEventForControllerSupport(HLERequestContext& ctx);
    void GetRegisteredDevices(HLERequestContext& ctx);
    void AcquireUniquePadConnectionEventHandle(HLERequestContext& ctx);
    void GetUniquePadIds(HLERequestContext& ctx);
    void AcquireJoyDetachOnBluetoothOffEventHandle(HLERequestContext& ctx);
    void IsUsbFullKeyControllerEnabled(HLERequestContext& ctx);
    void EnableUsbFullKeyController(HLERequestContext& ctx);
    void IsHandheldButtonPressedOnConsoleMode(HLERequestContext& ctx);
    void InitializeFirmwareUpdate(HLERequestContext& ctx);
    void CheckFirmwareUpdateRequired(HLERequestContext& ctx);
    void SetFirmwareHotfixUpdateSkipEnabled(HLERequestContext& ctx);
    void InitializeUsbFirmwareUpdate(HLERequestContext& ctx);
    void FinalizeUsbFirmwareUpdate(HLERequestContext& ctx);
    void CheckUsbFirmwareUpdateRequired(HLERequestContext& ctx);
    void InitializeUsbFirmwareUpdateWithoutMemory(HLERequestContext& ctx);
    void SetTouchScreenMagnification(HLERequestContext& ctx);
    void GetTouchScreenFirmwareVersion(HLERequestContext& ctx);
    void SetTouchScreenDefaultConfiguration(HLERequestContext& ctx);
    void GetTouchScreenDefaultConfiguration(HLERequestContext& ctx);
    void SetForceHandheldStyleVibration(HLERequestContext& ctx);
    void IsUsingCustomButtonConfig(HLERequestContext& ctx);
    void IsAnyCustomButtonConfigEnabled(HLERequestContext& ctx);

    std::shared_ptr<ResourceManager> GetResourceManager();

    Kernel::KEvent* acquire_connection_trigger_timeout_event;
    Kernel::KEvent* acquire_device_registered_event;
    Kernel::KEvent* joy_detach_event;
    Kernel::KEvent* unique_pad_connection_event;
    KernelHelpers::ServiceContext service_context;
    std::shared_ptr<ResourceManager> resource_manager;
    std::shared_ptr<HidFirmwareSettings> firmware_settings;
};

} // namespace Service::HID
