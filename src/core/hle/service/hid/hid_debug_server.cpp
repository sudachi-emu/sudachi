// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <algorithm>

#include "core/hle/service/cmif_serialization.h"
#include "core/hle/service/hid/hid_debug_server.h"
#include "core/hle/service/ipc_helpers.h"
#include "hid_core/hid_types.h"
#include "hid_core/resource_manager.h"
#include "hid_core/resources/hid_firmware_settings.h"

#include "hid_core/resources/touch_screen/gesture.h"
#include "hid_core/resources/touch_screen/touch_screen.h"

namespace Service::HID {

IHidDebugServer::IHidDebugServer(Core::System& system_, std::shared_ptr<ResourceManager> resource,
                                 std::shared_ptr<HidFirmwareSettings> settings)
    : ServiceFramework{system_, "hid:dbg"}, resource_manager{resource},
      firmware_settings{settings} {
    // clang-format off
    static const FunctionInfo functions[] = {
        {0, nullptr, "DeactivateDebugPad"},
        {1, nullptr, "SetDebugPadAutoPilotState"},
        {2, nullptr, "UnsetDebugPadAutoPilotState"},
        {10, C<&IHidDebugServer::DeactivateTouchScreen>, "DeactivateTouchScreen"},
        {11, C<&IHidDebugServer::SetTouchScreenAutoPilotState>, "SetTouchScreenAutoPilotState"},
        {12, C<&IHidDebugServer::UnsetTouchScreenAutoPilotState>, "UnsetTouchScreenAutoPilotState"},
        {13, C<&IHidDebugServer::GetTouchScreenConfiguration>, "GetTouchScreenConfiguration"},
        {14, C<&IHidDebugServer::ProcessTouchScreenAutoTune>, "ProcessTouchScreenAutoTune"},
        {15, C<&IHidDebugServer::ForceStopTouchScreenManagement>, "ForceStopTouchScreenManagement"},
        {16, C<&IHidDebugServer::ForceRestartTouchScreenManagement>, "ForceRestartTouchScreenManagement"},
        {17, C<&IHidDebugServer::IsTouchScreenManaged>, "IsTouchScreenManaged"},
        {20, nullptr, "DeactivateMouse"},
        {21, nullptr, "SetMouseAutoPilotState"},
        {22, nullptr, "UnsetMouseAutoPilotState"},
        {25, nullptr, "SetDebugMouseAutoPilotState"},
        {26, nullptr, "UnsetDebugMouseAutoPilotState"},
        {30, nullptr, "DeactivateKeyboard"},
        {31, nullptr, "SetKeyboardAutoPilotState"},
        {32, nullptr, "UnsetKeyboardAutoPilotState"},
        {50, nullptr, "DeactivateXpad"},
        {51, nullptr, "SetXpadAutoPilotState"},
        {52, nullptr, "UnsetXpadAutoPilotState"},
        {53, nullptr, "DeactivateJoyXpad"},
        {60, nullptr, "ClearNpadSystemCommonPolicy"},
        {61, nullptr, "DeactivateNpad"},
        {62, nullptr, "ForceDisconnectNpad"},
        {91, C<&IHidDebugServer::DeactivateGesture>, "DeactivateGesture"},
        {110, nullptr, "DeactivateHomeButton"},
        {111, nullptr, "SetHomeButtonAutoPilotState"},
        {112, nullptr, "UnsetHomeButtonAutoPilotState"},
        {120, nullptr, "DeactivateSleepButton"},
        {121, nullptr, "SetSleepButtonAutoPilotState"},
        {122, nullptr, "UnsetSleepButtonAutoPilotState"},
        {123, nullptr, "DeactivateInputDetector"},
        {130, nullptr, "DeactivateCaptureButton"},
        {131, nullptr, "SetCaptureButtonAutoPilotState"},
        {132, nullptr, "UnsetCaptureButtonAutoPilotState"},
        {133, nullptr, "SetShiftAccelerometerCalibrationValue"},
        {134, nullptr, "GetShiftAccelerometerCalibrationValue"},
        {135, nullptr, "SetShiftGyroscopeCalibrationValue"},
        {136, nullptr, "GetShiftGyroscopeCalibrationValue"},
        {140, nullptr, "DeactivateConsoleSixAxisSensor"},
        {141, nullptr, "GetConsoleSixAxisSensorSamplingFrequency"},
        {142, nullptr, "DeactivateSevenSixAxisSensor"},
        {143, nullptr, "GetConsoleSixAxisSensorCountStates"},
        {144, nullptr, "GetAccelerometerFsr"},
        {145, nullptr, "SetAccelerometerFsr"},
        {146, nullptr, "GetAccelerometerOdr"},
        {147, nullptr, "SetAccelerometerOdr"},
        {148, nullptr, "GetGyroscopeFsr"},
        {149, nullptr, "SetGyroscopeFsr"},
        {150, nullptr, "GetGyroscopeOdr"},
        {151, nullptr, "SetGyroscopeOdr"},
        {152, nullptr, "GetWhoAmI"},
        {201, nullptr, "ActivateFirmwareUpdate"},
        {202, nullptr, "DeactivateFirmwareUpdate"},
        {203, nullptr, "StartFirmwareUpdate"},
        {204, nullptr, "GetFirmwareUpdateStage"},
        {205, nullptr, "GetFirmwareVersion"},
        {206, nullptr, "GetDestinationFirmwareVersion"},
        {207, nullptr, "DiscardFirmwareInfoCacheForRevert"},
        {208, nullptr, "StartFirmwareUpdateForRevert"},
        {209, nullptr, "GetAvailableFirmwareVersionForRevert"},
        {210, nullptr, "IsFirmwareUpdatingDevice"},
        {211, nullptr, "StartFirmwareUpdateIndividual"},
        {215, nullptr, "SetUsbFirmwareForceUpdateEnabled"},
        {216, nullptr, "SetAllKuinaDevicesToFirmwareUpdateMode"},
        {217, nullptr, "StartFirmwareUpdateFromImageSet"}, // 17.0.0+
        {221, nullptr, "UpdateControllerColor"},
        {222, nullptr, "ConnectUsbPadsAsync"},
        {223, nullptr, "DisconnectUsbPadsAsync"},
        {224, nullptr, "UpdateDesignInfo"},
        {225, nullptr, "GetUniquePadDriverState"},
        {226, nullptr, "GetSixAxisSensorDriverStates"},
        {227, nullptr, "GetRxPacketHistory"},
        {228, nullptr, "AcquireOperationEventHandle"},
        {229, nullptr, "ReadSerialFlash"},
        {230, nullptr, "WriteSerialFlash"},
        {231, nullptr, "GetOperationResult"},
        {232, nullptr, "EnableShipmentMode"},
        {233, nullptr, "ClearPairingInfo"},
        {234, nullptr, "GetUniquePadDeviceTypeSetInternal"},
        {235, nullptr, "EnableAnalogStickPower"},
        {236, nullptr, "RequestKuinaUartClockCal"},
        {237, nullptr, "GetKuinaUartClockCal"},
        {238, nullptr, "SetKuinaUartClockTrim"},
        {239, nullptr, "KuinaLoopbackTest"},
        {240, nullptr, "RequestBatteryVoltage"},
        {241, nullptr, "GetBatteryVoltage"},
        {242, nullptr, "GetUniquePadPowerInfo"},
        {243, nullptr, "RebootUniquePad"},
        {244, nullptr, "RequestKuinaFirmwareVersion"},
        {245, nullptr, "GetKuinaFirmwareVersion"},
        {246, nullptr, "GetVidPid"},
        {247, nullptr, "GetAnalogStickCalibrationValue"},
        {248, nullptr, "GetUniquePadIdsFull"},
        {249, nullptr, "ConnectUniquePad"},
        {250, nullptr, "IsVirtual"},
        {251, nullptr, "GetAnalogStickModuleParam"},
        {301, nullptr, "GetAbstractedPadHandles"},
        {302, nullptr, "GetAbstractedPadState"},
        {303, nullptr, "GetAbstractedPadsState"},
        {321, nullptr, "SetAutoPilotVirtualPadState"},
        {322, nullptr, "UnsetAutoPilotVirtualPadState"},
        {323, nullptr, "UnsetAllAutoPilotVirtualPadState"},
        {324, nullptr, "AttachHdlsWorkBuffer"},
        {325, nullptr, "ReleaseHdlsWorkBuffer"},
        {326, nullptr, "DumpHdlsNpadAssignmentState"},
        {327, nullptr, "DumpHdlsStates"},
        {328, nullptr, "ApplyHdlsNpadAssignmentState"},
        {329, nullptr, "ApplyHdlsStateList"},
        {330, nullptr, "AttachHdlsVirtualDevice"},
        {331, nullptr, "DetachHdlsVirtualDevice"},
        {332, nullptr, "SetHdlsState"},
        {350, nullptr, "AddRegisteredDevice"},
        {351, nullptr, "GetRegisteredDevicesCountDebug"}, // 17.0.0+
        {352, nullptr, "DeleteRegisteredDevicesDebug"}, // 17.0.0+
        {400, nullptr, "DisableExternalMcuOnNxDevice"},
        {401, nullptr, "DisableRailDeviceFiltering"},
        {402, nullptr, "EnableWiredPairing"},
        {403, nullptr, "EnableShipmentModeAutoClear"},
        {404, nullptr, "SetRailEnabled"},
        {500, nullptr, "SetFactoryInt"},
        {501, nullptr, "IsFactoryBootEnabled"},
        {550, nullptr, "SetAnalogStickModelDataTemporarily"},
        {551, nullptr, "GetAnalogStickModelData"},
        {552, nullptr, "ResetAnalogStickModelData"},
        {600, nullptr, "ConvertPadState"},
        {650, nullptr, "AddButtonPlayData"},
        {651, nullptr, "StartButtonPlayData"},
        {652, nullptr, "StopButtonPlayData"},
        {2000, nullptr, "DeactivateDigitizer"},
        {2001, nullptr, "SetDigitizerAutoPilotState"},
        {2002, nullptr, "UnsetDigitizerAutoPilotState"},
        {2002, nullptr, "ReloadFirmwareDebugSettings"},
    };
    // clang-format on

    RegisterHandlers(functions);
}

IHidDebugServer::~IHidDebugServer() = default;

Result IHidDebugServer::DeactivateTouchScreen() {
    LOG_INFO(Service_HID, "called");

    if (!firmware_settings->IsDeviceManaged()) {
        R_RETURN(GetResourceManager()->GetTouchScreen()->Deactivate());
    }

    R_SUCCEED();
}

Result IHidDebugServer::SetTouchScreenAutoPilotState(
    InArray<TouchState, BufferAttr_HipcMapAlias> auto_pilot_buffer) {
    AutoPilotState auto_pilot{};

    auto_pilot.count =
        static_cast<u64>(std::min(auto_pilot_buffer.size(), auto_pilot.state.size()));
    memcpy(auto_pilot.state.data(), auto_pilot_buffer.data(),
           auto_pilot.count * sizeof(TouchState));

    LOG_INFO(Service_HID, "called, auto_pilot_count={}", auto_pilot.count);

    R_RETURN(GetResourceManager()->GetTouchScreen()->SetTouchScreenAutoPilotState(auto_pilot));
}

Result IHidDebugServer::UnsetTouchScreenAutoPilotState() {
    LOG_INFO(Service_HID, "called");
    R_RETURN(GetResourceManager()->GetTouchScreen()->UnsetTouchScreenAutoPilotState());
}

Result IHidDebugServer::GetTouchScreenConfiguration(
    Out<Core::HID::TouchScreenConfigurationForNx> out_touchscreen_config,
    ClientAppletResourceUserId aruid) {
    LOG_INFO(Service_HID, "called, applet_resource_user_id={}", aruid.pid);

    R_TRY(GetResourceManager()->GetTouchScreen()->GetTouchScreenConfiguration(
        *out_touchscreen_config, aruid.pid));

    if (out_touchscreen_config->mode != Core::HID::TouchScreenModeForNx::Heat2 &&
        out_touchscreen_config->mode != Core::HID::TouchScreenModeForNx::Finger) {
        out_touchscreen_config->mode = Core::HID::TouchScreenModeForNx::UseSystemSetting;
    }

    R_SUCCEED();
}

Result IHidDebugServer::ProcessTouchScreenAutoTune() {
    LOG_INFO(Service_HID, "called");
    R_RETURN(GetResourceManager()->GetTouchScreen()->ProcessTouchScreenAutoTune());
}

Result IHidDebugServer::ForceStopTouchScreenManagement() {
    LOG_INFO(Service_HID, "called");

    if (!firmware_settings->IsDeviceManaged()) {
        R_SUCCEED();
    }

    auto touch_screen = GetResourceManager()->GetTouchScreen();
    auto gesture = GetResourceManager()->GetGesture();

    if (firmware_settings->IsTouchI2cManaged()) {
        bool is_touch_active{};
        bool is_gesture_active{};
        R_TRY(touch_screen->IsActive(is_touch_active));
        R_TRY(gesture->IsActive(is_gesture_active));

        if (is_touch_active) {
            R_TRY(touch_screen->Deactivate());
        }
        if (is_gesture_active) {
            R_TRY(gesture->Deactivate());
        }
    }

    R_SUCCEED();
}

Result IHidDebugServer::ForceRestartTouchScreenManagement(u32 basic_gesture_id,
                                                          ClientAppletResourceUserId aruid) {
    LOG_INFO(Service_HID, "called, basic_gesture_id={}, applet_resource_user_id={}",
             basic_gesture_id, aruid.pid);

    auto touch_screen = GetResourceManager()->GetTouchScreen();
    auto gesture = GetResourceManager()->GetGesture();

    if (firmware_settings->IsDeviceManaged() && firmware_settings->IsTouchI2cManaged()) {
        R_TRY(gesture->Activate());
        R_TRY(gesture->Activate(aruid.pid, basic_gesture_id));
        R_TRY(touch_screen->Activate());
        R_TRY(touch_screen->Activate(aruid.pid));
    }

    R_SUCCEED();
}

Result IHidDebugServer::IsTouchScreenManaged(Out<bool> out_is_managed) {
    LOG_INFO(Service_HID, "called");

    bool is_touch_active{};
    bool is_gesture_active{};
    R_TRY(GetResourceManager()->GetTouchScreen()->IsActive(is_touch_active));
    R_TRY(GetResourceManager()->GetGesture()->IsActive(is_gesture_active));

    *out_is_managed = is_touch_active || is_gesture_active;
    R_SUCCEED();
}

Result IHidDebugServer::DeactivateGesture() {
    LOG_INFO(Service_HID, "called");

    if (!firmware_settings->IsDeviceManaged()) {
        R_RETURN(GetResourceManager()->GetGesture()->Deactivate());
    }

    R_SUCCEED();
}

std::shared_ptr<ResourceManager> IHidDebugServer::GetResourceManager() {
    resource_manager->Initialize();
    return resource_manager;
}

} // namespace Service::HID
