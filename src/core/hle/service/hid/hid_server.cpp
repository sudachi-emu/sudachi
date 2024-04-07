// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <array>

#include "common/common_types.h"
#include "common/logging/log.h"
#include "common/settings.h"
#include "core/hle/kernel/k_shared_memory.h"
#include "core/hle/kernel/k_transfer_memory.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/service/cmif_serialization.h"
#include "core/hle/service/hid/active_vibration_device_list.h"
#include "core/hle/service/hid/applet_resource.h"
#include "core/hle/service/hid/hid_server.h"
#include "core/hle/service/ipc_helpers.h"
#include "core/memory.h"
#include "hid_core/hid_result.h"
#include "hid_core/hid_util.h"
#include "hid_core/resource_manager.h"
#include "hid_core/resources/hid_firmware_settings.h"

#include "hid_core/resources/controller_base.h"
#include "hid_core/resources/debug_pad/debug_pad.h"
#include "hid_core/resources/keyboard/keyboard.h"
#include "hid_core/resources/mouse/mouse.h"
#include "hid_core/resources/npad/npad.h"
#include "hid_core/resources/npad/npad_types.h"
#include "hid_core/resources/npad/npad_vibration.h"
#include "hid_core/resources/palma/palma.h"
#include "hid_core/resources/six_axis/console_six_axis.h"
#include "hid_core/resources/six_axis/seven_six_axis.h"
#include "hid_core/resources/six_axis/six_axis.h"
#include "hid_core/resources/touch_screen/gesture.h"
#include "hid_core/resources/touch_screen/touch_screen.h"
#include "hid_core/resources/vibration/gc_vibration_device.h"
#include "hid_core/resources/vibration/n64_vibration_device.h"
#include "hid_core/resources/vibration/vibration_device.h"

namespace Service::HID {

IHidServer::IHidServer(Core::System& system_, std::shared_ptr<ResourceManager> resource,
                       std::shared_ptr<HidFirmwareSettings> settings)
    : ServiceFramework{system_, "hid"}, resource_manager{resource}, firmware_settings{settings} {
    // clang-format off
    static const FunctionInfo functions[] = {
        {0, C<&IHidServer::CreateAppletResource>, "CreateAppletResource"},
        {1, C<&IHidServer::ActivateDebugPad>, "ActivateDebugPad"},
        {11, C<&IHidServer::ActivateTouchScreen>, "ActivateTouchScreen"},
        {21, C<&IHidServer::ActivateMouse>, "ActivateMouse"},
        {26, nullptr, "ActivateDebugMouse"},
        {31, C<&IHidServer::ActivateKeyboard>, "ActivateKeyboard"},
        {32, C<&IHidServer::SendKeyboardLockKeyEvent>, "SendKeyboardLockKeyEvent"},
        {40, C<&IHidServer::AcquireXpadIdEventHandle>, "AcquireXpadIdEventHandle"},
        {41, C<&IHidServer::ReleaseXpadIdEventHandle>, "ReleaseXpadIdEventHandle"},
        {51, C<&IHidServer::ActivateXpad>, "ActivateXpad"},
        {55, C<&IHidServer::GetXpadIds>, "GetXpadIds"},
        {56, C<&IHidServer::ActivateJoyXpad>, "ActivateJoyXpad"},
        {58, C<&IHidServer::GetJoyXpadLifoHandle>, "GetJoyXpadLifoHandle"},
        {59, C<&IHidServer::GetJoyXpadIds>, "GetJoyXpadIds"},
        {60, C<&IHidServer::ActivateSixAxisSensor>, "ActivateSixAxisSensor"},
        {61, C<&IHidServer::DeactivateSixAxisSensor>, "DeactivateSixAxisSensor"},
        {62, C<&IHidServer::GetSixAxisSensorLifoHandle>, "GetSixAxisSensorLifoHandle"},
        {63, C<&IHidServer::ActivateJoySixAxisSensor>, "ActivateJoySixAxisSensor"},
        {64, C<&IHidServer::DeactivateJoySixAxisSensor>, "DeactivateJoySixAxisSensor"},
        {65, C<&IHidServer::GetJoySixAxisSensorLifoHandle>, "GetJoySixAxisSensorLifoHandle"},
        {66, C<&IHidServer::StartSixAxisSensor>, "StartSixAxisSensor"},
        {67, C<&IHidServer::StopSixAxisSensor>, "StopSixAxisSensor"},
        {68, C<&IHidServer::IsSixAxisSensorFusionEnabled>, "IsSixAxisSensorFusionEnabled"},
        {69, C<&IHidServer::EnableSixAxisSensorFusion>, "EnableSixAxisSensorFusion"},
        {70, C<&IHidServer::SetSixAxisSensorFusionParameters>, "SetSixAxisSensorFusionParameters"},
        {71, C<&IHidServer::GetSixAxisSensorFusionParameters>, "GetSixAxisSensorFusionParameters"},
        {72, C<&IHidServer::ResetSixAxisSensorFusionParameters>, "ResetSixAxisSensorFusionParameters"},
        {73, nullptr, "SetAccelerometerParameters"},
        {74, nullptr, "GetAccelerometerParameters"},
        {75, nullptr, "ResetAccelerometerParameters"},
        {76, nullptr, "SetAccelerometerPlayMode"},
        {77, nullptr, "GetAccelerometerPlayMode"},
        {78, nullptr, "ResetAccelerometerPlayMode"},
        {79, C<&IHidServer::SetGyroscopeZeroDriftMode>, "SetGyroscopeZeroDriftMode"},
        {80, C<&IHidServer::GetGyroscopeZeroDriftMode>, "GetGyroscopeZeroDriftMode"},
        {81, C<&IHidServer::ResetGyroscopeZeroDriftMode>, "ResetGyroscopeZeroDriftMode"},
        {82, C<&IHidServer::IsSixAxisSensorAtRest>, "IsSixAxisSensorAtRest"},
        {83, C<&IHidServer::IsFirmwareUpdateAvailableForSixAxisSensor>, "IsFirmwareUpdateAvailableForSixAxisSensor"},
        {84, C<&IHidServer::EnableSixAxisSensorUnalteredPassthrough>, "EnableSixAxisSensorUnalteredPassthrough"},
        {85, C<&IHidServer::IsSixAxisSensorUnalteredPassthroughEnabled>, "IsSixAxisSensorUnalteredPassthroughEnabled"},
        {86, nullptr, "StoreSixAxisSensorCalibrationParameter"},
        {87, C<&IHidServer::LoadSixAxisSensorCalibrationParameter>, "LoadSixAxisSensorCalibrationParameter"},
        {88, C<&IHidServer::GetSixAxisSensorIcInformation>, "GetSixAxisSensorIcInformation"},
        {89, C<&IHidServer::ResetIsSixAxisSensorDeviceNewlyAssigned>, "ResetIsSixAxisSensorDeviceNewlyAssigned"},
        {91, C<&IHidServer::ActivateGesture>, "ActivateGesture"},
        {100, C<&IHidServer::SetSupportedNpadStyleSet>, "SetSupportedNpadStyleSet"},
        {101, C<&IHidServer::GetSupportedNpadStyleSet>, "GetSupportedNpadStyleSet"},
        {102, C<&IHidServer::SetSupportedNpadIdType>, "SetSupportedNpadIdType"},
        {103, C<&IHidServer::ActivateNpad>, "ActivateNpad"},
        {104, C<&IHidServer::DeactivateNpad>, "DeactivateNpad"},
        {106, C<&IHidServer::AcquireNpadStyleSetUpdateEventHandle>, "AcquireNpadStyleSetUpdateEventHandle"},
        {107, C<&IHidServer::DisconnectNpad>, "DisconnectNpad"},
        {108, C<&IHidServer::GetPlayerLedPattern>, "GetPlayerLedPattern"},
        {109, C<&IHidServer::ActivateNpadWithRevision>, "ActivateNpadWithRevision"},
        {120, C<&IHidServer::SetNpadJoyHoldType>, "SetNpadJoyHoldType"},
        {121, C<&IHidServer::GetNpadJoyHoldType>, "GetNpadJoyHoldType"},
        {122, C<&IHidServer::SetNpadJoyAssignmentModeSingleByDefault>, "SetNpadJoyAssignmentModeSingleByDefault"},
        {123, C<&IHidServer::SetNpadJoyAssignmentModeSingle>, "SetNpadJoyAssignmentModeSingle"},
        {124, C<&IHidServer::SetNpadJoyAssignmentModeDual>, "SetNpadJoyAssignmentModeDual"},
        {125, C<&IHidServer::MergeSingleJoyAsDualJoy>, "MergeSingleJoyAsDualJoy"},
        {126, C<&IHidServer::StartLrAssignmentMode>, "StartLrAssignmentMode"},
        {127, C<&IHidServer::StopLrAssignmentMode>, "StopLrAssignmentMode"},
        {128, C<&IHidServer::SetNpadHandheldActivationMode>, "SetNpadHandheldActivationMode"},
        {129, C<&IHidServer::GetNpadHandheldActivationMode>, "GetNpadHandheldActivationMode"},
        {130, C<&IHidServer::SwapNpadAssignment>, "SwapNpadAssignment"},
        {131, C<&IHidServer::IsUnintendedHomeButtonInputProtectionEnabled>, "IsUnintendedHomeButtonInputProtectionEnabled"},
        {132, C<&IHidServer::EnableUnintendedHomeButtonInputProtection>, "EnableUnintendedHomeButtonInputProtection"},
        {133, C<&IHidServer::SetNpadJoyAssignmentModeSingleWithDestination>, "SetNpadJoyAssignmentModeSingleWithDestination"},
        {134, C<&IHidServer::SetNpadAnalogStickUseCenterClamp>, "SetNpadAnalogStickUseCenterClamp"},
        {135, C<&IHidServer::SetNpadCaptureButtonAssignment>, "SetNpadCaptureButtonAssignment"},
        {136, C<&IHidServer::ClearNpadCaptureButtonAssignment>, "ClearNpadCaptureButtonAssignment"},
        {200, C<&IHidServer::GetVibrationDeviceInfo>, "GetVibrationDeviceInfo"},
        {201, C<&IHidServer::SendVibrationValue>, "SendVibrationValue"},
        {202, C<&IHidServer::GetActualVibrationValue>, "GetActualVibrationValue"},
        {203, C<&IHidServer::CreateActiveVibrationDeviceList>, "CreateActiveVibrationDeviceList"},
        {204, C<&IHidServer::PermitVibration>, "PermitVibration"},
        {205, C<&IHidServer::IsVibrationPermitted>, "IsVibrationPermitted"},
        {206, C<&IHidServer::SendVibrationValues>, "SendVibrationValues"},
        {207, C<&IHidServer::SendVibrationGcErmCommand>, "SendVibrationGcErmCommand"},
        {208, C<&IHidServer::GetActualVibrationGcErmCommand>, "GetActualVibrationGcErmCommand"},
        {209, C<&IHidServer::BeginPermitVibrationSession>, "BeginPermitVibrationSession"},
        {210, C<&IHidServer::EndPermitVibrationSession>, "EndPermitVibrationSession"},
        {211, C<&IHidServer::IsVibrationDeviceMounted>, "IsVibrationDeviceMounted"},
        {212, C<&IHidServer::SendVibrationValueInBool>, "SendVibrationValueInBool"},
        {213, nullptr, "SendVibrationValueInMode"}, // 17.0.0+
        {214, nullptr, "SendVibrationValuesInMode"}, // 17.0.0+
        {300, C<&IHidServer::ActivateConsoleSixAxisSensor>, "ActivateConsoleSixAxisSensor"},
        {301, C<&IHidServer::StartConsoleSixAxisSensor>, "StartConsoleSixAxisSensor"},
        {302, C<&IHidServer::StopConsoleSixAxisSensor>, "StopConsoleSixAxisSensor"},
        {303, C<&IHidServer::ActivateSevenSixAxisSensor>, "ActivateSevenSixAxisSensor"},
        {304, C<&IHidServer::StartSevenSixAxisSensor>, "StartSevenSixAxisSensor"},
        {305, C<&IHidServer::StopSevenSixAxisSensor>, "StopSevenSixAxisSensor"},
        {306, C<&IHidServer::InitializeSevenSixAxisSensor>, "InitializeSevenSixAxisSensor"},
        {307, C<&IHidServer::FinalizeSevenSixAxisSensor>, "FinalizeSevenSixAxisSensor"},
        {308, nullptr, "SetSevenSixAxisSensorFusionStrength"},
        {309, nullptr, "GetSevenSixAxisSensorFusionStrength"},
        {310, C<&IHidServer::ResetSevenSixAxisSensorTimestamp>, "ResetSevenSixAxisSensorTimestamp"},
        {311, nullptr, "ForceActivateConsoleSixAxisSensor"}, // 17.0.0+
        {312, nullptr, "ForceDeactivateConsoleSixAxisSensor"}, // 17.0.0+
        {400, C<&IHidServer::IsUsbFullKeyControllerEnabled>, "IsUsbFullKeyControllerEnabled"},
        {401, nullptr, "EnableUsbFullKeyController"},
        {402, nullptr, "IsUsbFullKeyControllerConnected"},
        {403, nullptr, "HasBattery"},
        {404, nullptr, "HasLeftRightBattery"},
        {405, nullptr, "GetNpadInterfaceType"},
        {406, nullptr, "GetNpadLeftRightInterfaceType"},
        {407, nullptr, "GetNpadOfHighestBatteryLevel"},
        {408, nullptr, "GetNpadOfHighestBatteryLevelForJoyRight"},
        {500, C<&IHidServer::GetPalmaConnectionHandle>, "GetPalmaConnectionHandle"},
        {501, C<&IHidServer::InitializePalma>, "InitializePalma"},
        {502, C<&IHidServer::AcquirePalmaOperationCompleteEvent>, "AcquirePalmaOperationCompleteEvent"},
        {503, C<&IHidServer::GetPalmaOperationInfo>, "GetPalmaOperationInfo"},
        {504, C<&IHidServer::PlayPalmaActivity>, "PlayPalmaActivity"},
        {505, C<&IHidServer::SetPalmaFrModeType>, "SetPalmaFrModeType"},
        {506, C<&IHidServer::ReadPalmaStep>, "ReadPalmaStep"},
        {507, C<&IHidServer::EnablePalmaStep>, "EnablePalmaStep"},
        {508, C<&IHidServer::ResetPalmaStep>, "ResetPalmaStep"},
        {509, C<&IHidServer::ReadPalmaApplicationSection>, "ReadPalmaApplicationSection"},
        {510, C<&IHidServer::WritePalmaApplicationSection>, "WritePalmaApplicationSection"},
        {511, C<&IHidServer::ReadPalmaUniqueCode>, "ReadPalmaUniqueCode"},
        {512, C<&IHidServer::SetPalmaUniqueCodeInvalid>, "SetPalmaUniqueCodeInvalid"},
        {513, C<&IHidServer::WritePalmaActivityEntry>, "WritePalmaActivityEntry"},
        {514, C<&IHidServer::WritePalmaRgbLedPatternEntry>, "WritePalmaRgbLedPatternEntry"},
        {515, C<&IHidServer::WritePalmaWaveEntry>, "WritePalmaWaveEntry"},
        {516, C<&IHidServer::SetPalmaDataBaseIdentificationVersion>, "SetPalmaDataBaseIdentificationVersion"},
        {517, C<&IHidServer::GetPalmaDataBaseIdentificationVersion>, "GetPalmaDataBaseIdentificationVersion"},
        {518, C<&IHidServer::SuspendPalmaFeature>, "SuspendPalmaFeature"},
        {519, C<&IHidServer::GetPalmaOperationResult>, "GetPalmaOperationResult"},
        {520, C<&IHidServer::ReadPalmaPlayLog>, "ReadPalmaPlayLog"},
        {521, C<&IHidServer::ResetPalmaPlayLog>, "ResetPalmaPlayLog"},
        {522, C<&IHidServer::SetIsPalmaAllConnectable>, "SetIsPalmaAllConnectable"},
        {523, C<&IHidServer::SetIsPalmaPairedConnectable>, "SetIsPalmaPairedConnectable"},
        {524, C<&IHidServer::PairPalma>, "PairPalma"},
        {525, C<&IHidServer::SetPalmaBoostMode>, "SetPalmaBoostMode"},
        {526, C<&IHidServer::CancelWritePalmaWaveEntry>, "CancelWritePalmaWaveEntry"},
        {527, C<&IHidServer::EnablePalmaBoostMode>, "EnablePalmaBoostMode"},
        {528, C<&IHidServer::GetPalmaBluetoothAddress>, "GetPalmaBluetoothAddress"},
        {529, C<&IHidServer::SetDisallowedPalmaConnection>, "SetDisallowedPalmaConnection"},
        {1000, C<&IHidServer::SetNpadCommunicationMode>, "SetNpadCommunicationMode"},
        {1001, C<&IHidServer::GetNpadCommunicationMode>, "GetNpadCommunicationMode"},
        {1002, C<&IHidServer::SetTouchScreenConfiguration>, "SetTouchScreenConfiguration"},
        {1003, C<&IHidServer::IsFirmwareUpdateNeededForNotification>, "IsFirmwareUpdateNeededForNotification"},
        {1004, C<&IHidServer::SetTouchScreenResolution>, "SetTouchScreenResolution"},
        {2000, nullptr, "ActivateDigitizer"},
    };
    // clang-format on

    RegisterHandlers(functions);
}

IHidServer::~IHidServer() = default;

Result IHidServer::CreateAppletResource(OutInterface<IAppletResource> out_applet_resource,
                                        ClientAppletResourceUserId aruid) {
    const auto result = GetResourceManager()->CreateAppletResource(aruid.pid);

    LOG_DEBUG(Service_HID, "called, applet_resource_user_id={}, result=0x{:X}", aruid.pid,
              result.raw);

    *out_applet_resource = std::make_shared<IAppletResource>(system, resource_manager, aruid.pid);
    R_SUCCEED();
}

Result IHidServer::ActivateDebugPad(ClientAppletResourceUserId aruid) {
    LOG_DEBUG(Service_HID, "called, applet_resource_user_id={}", aruid.pid);

    if (!firmware_settings->IsDeviceManaged()) {
        R_TRY(GetResourceManager()->GetDebugPad()->Activate());
    }

    R_RETURN(GetResourceManager()->GetDebugPad()->Activate(aruid.pid));
}

Result IHidServer::ActivateTouchScreen(ClientAppletResourceUserId aruid) {
    LOG_DEBUG(Service_HID, "called, applet_resource_user_id={}", aruid.pid);

    if (!firmware_settings->IsDeviceManaged()) {
        R_TRY(GetResourceManager()->GetTouchScreen()->Activate());
    }

    R_RETURN(GetResourceManager()->GetTouchScreen()->Activate(aruid.pid));
}

Result IHidServer::ActivateMouse(ClientAppletResourceUserId aruid) {
    LOG_DEBUG(Service_HID, "called, applet_resource_user_id={}", aruid.pid);

    if (!firmware_settings->IsDeviceManaged()) {
        R_TRY(GetResourceManager()->GetMouse()->Activate());
    }

    R_RETURN(GetResourceManager()->GetMouse()->Activate(aruid.pid));
}

Result IHidServer::ActivateKeyboard(ClientAppletResourceUserId aruid) {
    LOG_DEBUG(Service_HID, "called, applet_resource_user_id={}", aruid.pid);

    if (!firmware_settings->IsDeviceManaged()) {
        R_TRY(GetResourceManager()->GetKeyboard()->Activate());
    }

    R_RETURN(GetResourceManager()->GetKeyboard()->Activate(aruid.pid));
}

Result IHidServer::SendKeyboardLockKeyEvent(u32 flags) {
    LOG_WARNING(Service_HID, "(STUBBED) called. flags={}", flags);
    R_SUCCEED();
}

Result IHidServer::AcquireXpadIdEventHandle(OutCopyHandle<Kernel::KReadableEvent> out_event,
                                            ClientAppletResourceUserId aruid) {
    LOG_DEBUG(Service_HID, "called, applet_resource_user_id={}", aruid.pid);

    // This function has been stubbed since 10.0.0+
    *out_event = nullptr;
    R_SUCCEED();
}

Result IHidServer::ReleaseXpadIdEventHandle(ClientAppletResourceUserId aruid) {
    LOG_DEBUG(Service_HID, "called, applet_resource_user_id={}", aruid.pid);

    // This function has been stubbed since 10.0.0+
    R_SUCCEED();
}

Result IHidServer::ActivateXpad(u32 basic_xpad_id, ClientAppletResourceUserId aruid) {
    LOG_DEBUG(Service_HID, "called, basic_xpad_id={}, applet_resource_user_id={}", basic_xpad_id,
              aruid.pid);

    // This function has been stubbed since 10.0.0+
    R_SUCCEED();
}

Result IHidServer::GetXpadIds(Out<u64> out_count,
                              OutArray<u32, BufferAttr_HipcPointer> out_basic_pad_ids) {
    LOG_DEBUG(Service_HID, "called");

    // This function has been hardcoded since 10.0.0+
    out_basic_pad_ids[0] = 0;
    out_basic_pad_ids[1] = 1;
    out_basic_pad_ids[2] = 2;
    out_basic_pad_ids[3] = 3;
    *out_count = 4;
    R_SUCCEED();
}

Result IHidServer::ActivateJoyXpad(u32 joy_xpad_id) {
    LOG_DEBUG(Service_HID, "called, joy_xpad_id={}", joy_xpad_id);

    // This function has been stubbed since 10.0.0+
    R_SUCCEED();
}

Result IHidServer::GetJoyXpadLifoHandle(
    OutCopyHandle<Kernel::KSharedMemory> out_shared_memory_handle, u32 joy_xpad_id) {
    LOG_DEBUG(Service_HID, "called, joy_xpad_id={}", joy_xpad_id);

    // This function has been stubbed since 10.0.0+
    *out_shared_memory_handle = nullptr;
    R_SUCCEED();
}

Result IHidServer::GetJoyXpadIds(Out<s64> out_basic_xpad_id_count) {
    LOG_DEBUG(Service_HID, "called");

    // This function has been hardcoded since 10.0.0+
    *out_basic_xpad_id_count = 0;
    R_SUCCEED();
}

Result IHidServer::ActivateSixAxisSensor(u32 joy_xpad_id) {
    LOG_DEBUG(Service_HID, "called, joy_xpad_id={}", joy_xpad_id);

    // This function has been stubbed since 10.0.0+
    R_SUCCEED();
}

Result IHidServer::DeactivateSixAxisSensor(u32 joy_xpad_id) {
    LOG_DEBUG(Service_HID, "called, joy_xpad_id={}", joy_xpad_id);

    // This function has been stubbed since 10.0.0+
    R_SUCCEED();
}

Result IHidServer::GetSixAxisSensorLifoHandle(
    OutCopyHandle<Kernel::KSharedMemory> out_shared_memory_handle, u32 joy_xpad_id) {
    LOG_DEBUG(Service_HID, "called, joy_xpad_id={}", joy_xpad_id);

    // This function has been stubbed since 10.0.0+
    *out_shared_memory_handle = nullptr;
    R_SUCCEED();
}

Result IHidServer::ActivateJoySixAxisSensor(u32 joy_xpad_id) {
    LOG_DEBUG(Service_HID, "called, joy_xpad_id={}", joy_xpad_id);

    // This function has been stubbed since 10.0.0+
    R_SUCCEED();
}

Result IHidServer::DeactivateJoySixAxisSensor(u32 joy_xpad_id) {
    LOG_DEBUG(Service_HID, "called, joy_xpad_id={}", joy_xpad_id);

    // This function has been stubbed since 10.0.0+
    R_SUCCEED();
}

Result IHidServer::GetJoySixAxisSensorLifoHandle(
    OutCopyHandle<Kernel::KSharedMemory> out_shared_memory_handle, u32 joy_xpad_id) {
    LOG_DEBUG(Service_HID, "called, joy_xpad_id={}", joy_xpad_id);

    // This function has been stubbed since 10.0.0+
    *out_shared_memory_handle = nullptr;
    R_SUCCEED();
}

Result IHidServer::StartSixAxisSensor(Core::HID::SixAxisSensorHandle sixaxis_handle,
                                      ClientAppletResourceUserId aruid) {
    LOG_DEBUG(Service_HID,
              "called, npad_type={}, npad_id={}, device_index={}, applet_resource_user_id={}",
              sixaxis_handle.npad_type, sixaxis_handle.npad_id, sixaxis_handle.device_index,
              aruid.pid);

    R_RETURN(GetResourceManager()->GetSixAxis()->SetSixAxisEnabled(sixaxis_handle, true));
}

Result IHidServer::StopSixAxisSensor(Core::HID::SixAxisSensorHandle sixaxis_handle,
                                     ClientAppletResourceUserId aruid) {
    LOG_DEBUG(Service_HID,
              "called, npad_type={}, npad_id={}, device_index={}, applet_resource_user_id={}",
              sixaxis_handle.npad_type, sixaxis_handle.npad_id, sixaxis_handle.device_index,
              aruid.pid);

    R_RETURN(GetResourceManager()->GetSixAxis()->SetSixAxisEnabled(sixaxis_handle, false));
}

Result IHidServer::IsSixAxisSensorFusionEnabled(Out<bool> out_is_enabled,
                                                Core::HID::SixAxisSensorHandle sixaxis_handle,
                                                ClientAppletResourceUserId aruid) {
    LOG_DEBUG(Service_HID,
              "called, npad_type={}, npad_id={}, device_index={}, applet_resource_user_id={}",
              sixaxis_handle.npad_type, sixaxis_handle.npad_id, sixaxis_handle.device_index,
              aruid.pid);

    R_RETURN(GetResourceManager()->GetSixAxis()->IsSixAxisSensorFusionEnabled(sixaxis_handle,
                                                                              *out_is_enabled));
}

Result IHidServer::EnableSixAxisSensorFusion(bool is_enabled,
                                             Core::HID::SixAxisSensorHandle sixaxis_handle,
                                             ClientAppletResourceUserId aruid) {
    LOG_DEBUG(Service_HID,
              "called, is_enabled={}, npad_type={}, npad_id={}, "
              "device_index={}, applet_resource_user_id={}",
              is_enabled, sixaxis_handle.npad_type, sixaxis_handle.npad_id,
              sixaxis_handle.device_index, aruid.pid);

    R_RETURN(
        GetResourceManager()->GetSixAxis()->SetSixAxisFusionEnabled(sixaxis_handle, is_enabled));
}

Result IHidServer::SetSixAxisSensorFusionParameters(
    Core::HID::SixAxisSensorHandle sixaxis_handle,
    Core::HID::SixAxisSensorFusionParameters sixaxis_fusion, ClientAppletResourceUserId aruid) {
    LOG_DEBUG(Service_HID,
              "called, npad_type={}, npad_id={}, device_index={}, parameter1={}, "
              "parameter2={}, applet_resource_user_id={}",
              sixaxis_handle.npad_type, sixaxis_handle.npad_id, sixaxis_handle.device_index,
              sixaxis_fusion.parameter1, sixaxis_fusion.parameter2, aruid.pid);

    R_RETURN(GetResourceManager()->GetSixAxis()->SetSixAxisFusionParameters(sixaxis_handle,
                                                                            sixaxis_fusion));
}

Result IHidServer::GetSixAxisSensorFusionParameters(
    Out<Core::HID::SixAxisSensorFusionParameters> out_fusion_parameters,
    Core::HID::SixAxisSensorHandle sixaxis_handle, ClientAppletResourceUserId aruid) {
    LOG_DEBUG(Service_HID,
              "called, npad_type={}, npad_id={}, device_index={}, applet_resource_user_id={}",
              sixaxis_handle.npad_type, sixaxis_handle.npad_id, sixaxis_handle.device_index,
              aruid.pid);

    R_RETURN(GetResourceManager()->GetSixAxis()->GetSixAxisFusionParameters(
        sixaxis_handle, *out_fusion_parameters));
}

Result IHidServer::ResetSixAxisSensorFusionParameters(Core::HID::SixAxisSensorHandle sixaxis_handle,
                                                      ClientAppletResourceUserId aruid) {
    LOG_DEBUG(Service_HID,
              "called, npad_type={}, npad_id={}, device_index={}, applet_resource_user_id={}",
              sixaxis_handle.npad_type, sixaxis_handle.npad_id, sixaxis_handle.device_index,
              aruid.pid);

    // Since these parameters are unknown just use what HW outputs
    const Core::HID::SixAxisSensorFusionParameters fusion_parameters{
        .parameter1 = 0.03f,
        .parameter2 = 0.4f,
    };

    R_TRY(GetResourceManager()->GetSixAxis()->SetSixAxisFusionParameters(sixaxis_handle,
                                                                         fusion_parameters));
    R_RETURN(GetResourceManager()->GetSixAxis()->SetSixAxisFusionEnabled(sixaxis_handle, true));
}

Result IHidServer::SetGyroscopeZeroDriftMode(Core::HID::SixAxisSensorHandle sixaxis_handle,
                                             Core::HID::GyroscopeZeroDriftMode drift_mode,
                                             ClientAppletResourceUserId aruid) {
    LOG_DEBUG(Service_HID,
              "called, npad_type={}, npad_id={}, device_index={}, drift_mode={}, "
              "applet_resource_user_id={}",
              sixaxis_handle.npad_type, sixaxis_handle.npad_id, sixaxis_handle.device_index,
              drift_mode, aruid.pid);

    R_RETURN(
        GetResourceManager()->GetSixAxis()->SetGyroscopeZeroDriftMode(sixaxis_handle, drift_mode));
}

Result IHidServer::GetGyroscopeZeroDriftMode(Out<Core::HID::GyroscopeZeroDriftMode> out_drift_mode,
                                             Core::HID::SixAxisSensorHandle sixaxis_handle,
                                             ClientAppletResourceUserId aruid) {
    LOG_DEBUG(Service_HID,
              "called, npad_type={}, npad_id={}, device_index={}, applet_resource_user_id={}",
              sixaxis_handle.npad_type, sixaxis_handle.npad_id, sixaxis_handle.device_index,
              aruid.pid);

    R_RETURN(GetResourceManager()->GetSixAxis()->GetGyroscopeZeroDriftMode(sixaxis_handle,
                                                                           *out_drift_mode));
}

Result IHidServer::ResetGyroscopeZeroDriftMode(Core::HID::SixAxisSensorHandle sixaxis_handle,
                                               ClientAppletResourceUserId aruid) {
    LOG_DEBUG(Service_HID,
              "called, npad_type={}, npad_id={}, device_index={}, applet_resource_user_id={}",
              sixaxis_handle.npad_type, sixaxis_handle.npad_id, sixaxis_handle.device_index,
              aruid.pid);

    const auto drift_mode{Core::HID::GyroscopeZeroDriftMode::Standard};
    R_RETURN(
        GetResourceManager()->GetSixAxis()->SetGyroscopeZeroDriftMode(sixaxis_handle, drift_mode));
}

Result IHidServer::IsSixAxisSensorAtRest(Out<bool> out_is_at_rest,
                                         Core::HID::SixAxisSensorHandle sixaxis_handle,
                                         ClientAppletResourceUserId aruid) {
    LOG_DEBUG(Service_HID,
              "called, npad_type={}, npad_id={}, device_index={}, applet_resource_user_id={}",
              sixaxis_handle.npad_type, sixaxis_handle.npad_id, sixaxis_handle.device_index,
              aruid.pid);

    R_RETURN(
        GetResourceManager()->GetSixAxis()->IsSixAxisSensorAtRest(sixaxis_handle, *out_is_at_rest));
}

Result IHidServer::IsFirmwareUpdateAvailableForSixAxisSensor(
    Out<bool> out_is_firmware_available, Core::HID::SixAxisSensorHandle sixaxis_handle,
    ClientAppletResourceUserId aruid) {
    LOG_WARNING(
        Service_HID,
        "(STUBBED) called, npad_type={}, npad_id={}, device_index={}, applet_resource_user_id={}",
        sixaxis_handle.npad_type, sixaxis_handle.npad_id, sixaxis_handle.device_index, aruid.pid);

    R_RETURN(GetResourceManager()->GetNpad()->IsFirmwareUpdateAvailableForSixAxisSensor(
        aruid.pid, sixaxis_handle, *out_is_firmware_available));
}

Result IHidServer::EnableSixAxisSensorUnalteredPassthrough(
    bool is_enabled, Core::HID::SixAxisSensorHandle sixaxis_handle,
    ClientAppletResourceUserId aruid) {
    LOG_DEBUG(Service_HID,
              "(STUBBED) called, enabled={}, npad_type={}, npad_id={}, device_index={}, "
              "applet_resource_user_id={}",
              is_enabled, sixaxis_handle.npad_type, sixaxis_handle.npad_id,
              sixaxis_handle.device_index, aruid.pid);

    R_RETURN(GetResourceManager()->GetSixAxis()->EnableSixAxisSensorUnalteredPassthrough(
        sixaxis_handle, is_enabled));
}

Result IHidServer::IsSixAxisSensorUnalteredPassthroughEnabled(
    Out<bool> out_is_enabled, Core::HID::SixAxisSensorHandle sixaxis_handle,
    ClientAppletResourceUserId aruid) {
    LOG_DEBUG(
        Service_HID,
        "(STUBBED) called, npad_type={}, npad_id={}, device_index={}, applet_resource_user_id={}",
        sixaxis_handle.npad_type, sixaxis_handle.npad_id, sixaxis_handle.device_index, aruid.pid);

    R_RETURN(GetResourceManager()->GetSixAxis()->IsSixAxisSensorUnalteredPassthroughEnabled(
        sixaxis_handle, *out_is_enabled));
}

Result IHidServer::LoadSixAxisSensorCalibrationParameter(
    OutLargeData<Core::HID::SixAxisSensorCalibrationParameter, BufferAttr_HipcMapAlias>
        out_calibration,
    Core::HID::SixAxisSensorHandle sixaxis_handle, ClientAppletResourceUserId aruid) {
    LOG_WARNING(
        Service_HID,
        "(STUBBED) called, npad_type={}, npad_id={}, device_index={}, applet_resource_user_id={}",
        sixaxis_handle.npad_type, sixaxis_handle.npad_id, sixaxis_handle.device_index, aruid.pid);

    R_RETURN(GetResourceManager()->GetSixAxis()->LoadSixAxisSensorCalibrationParameter(
        sixaxis_handle, *out_calibration));
}

Result IHidServer::GetSixAxisSensorIcInformation(
    OutLargeData<Core::HID::SixAxisSensorIcInformation, BufferAttr_HipcPointer> out_ic_information,
    Core::HID::SixAxisSensorHandle sixaxis_handle, ClientAppletResourceUserId aruid) {
    LOG_WARNING(
        Service_HID,
        "(STUBBED) called, npad_type={}, npad_id={}, device_index={}, applet_resource_user_id={}",
        sixaxis_handle.npad_type, sixaxis_handle.npad_id, sixaxis_handle.device_index, aruid.pid);

    R_RETURN(GetResourceManager()->GetSixAxis()->GetSixAxisSensorIcInformation(
        sixaxis_handle, *out_ic_information));
}

Result IHidServer::ResetIsSixAxisSensorDeviceNewlyAssigned(
    Core::HID::SixAxisSensorHandle sixaxis_handle, ClientAppletResourceUserId aruid) {
    LOG_WARNING(
        Service_HID,
        "(STUBBED) called, npad_type={}, npad_id={}, device_index={}, applet_resource_user_id={}",
        sixaxis_handle.npad_type, sixaxis_handle.npad_id, sixaxis_handle.device_index, aruid.pid);

    R_RETURN(GetResourceManager()->GetNpad()->ResetIsSixAxisSensorDeviceNewlyAssigned(
        aruid.pid, sixaxis_handle));
}

Result IHidServer::ActivateGesture(u32 basic_gesture_id, ClientAppletResourceUserId aruid) {
    LOG_INFO(Service_HID, "called, basic_gesture_id={}, applet_resource_user_id={}",
             basic_gesture_id, aruid.pid);

    if (!firmware_settings->IsDeviceManaged()) {
        R_TRY(GetResourceManager()->GetGesture()->Activate());
    }

    R_RETURN(GetResourceManager()->GetGesture()->Activate(aruid.pid, basic_gesture_id));
}

Result IHidServer::SetSupportedNpadStyleSet(Core::HID::NpadStyleSet supported_style_set,
                                            ClientAppletResourceUserId aruid) {
    LOG_DEBUG(Service_HID, "called, supported_style_set={}, applet_resource_user_id={}",
              supported_style_set, aruid.pid);

    R_TRY(
        GetResourceManager()->GetNpad()->SetSupportedNpadStyleSet(aruid.pid, supported_style_set));

    Core::HID::NpadStyleTag style_tag{supported_style_set};
    const auto revision = GetResourceManager()->GetNpad()->GetRevision(aruid.pid);

    if (style_tag.palma != 0 && revision < NpadRevision::Revision3) {
        // GetResourceManager()->GetPalma()->EnableBoostMode(aruid.pid, true);
    }

    R_SUCCEED()
}

Result IHidServer::GetSupportedNpadStyleSet(Out<Core::HID::NpadStyleSet> out_supported_style_set,
                                            ClientAppletResourceUserId aruid) {
    LOG_DEBUG(Service_HID, "called, applet_resource_user_id={}", aruid.pid);

    R_RETURN(GetResourceManager()->GetNpad()->GetSupportedNpadStyleSet(aruid.pid,
                                                                       *out_supported_style_set));
}

Result IHidServer::SetSupportedNpadIdType(
    ClientAppletResourceUserId aruid,
    InArray<Core::HID::NpadIdType, BufferAttr_HipcPointer> supported_npad_list) {
    LOG_DEBUG(Service_HID, "called, applet_resource_user_id={}", aruid.pid);

    R_RETURN(
        GetResourceManager()->GetNpad()->SetSupportedNpadIdType(aruid.pid, supported_npad_list));
}

Result IHidServer::ActivateNpad(ClientAppletResourceUserId aruid) {
    LOG_DEBUG(Service_HID, "called, applet_resource_user_id={}", aruid.pid);

    auto npad = GetResourceManager()->GetNpad();

    GetResourceManager()->GetNpad()->SetRevision(aruid.pid, NpadRevision::Revision0);
    R_RETURN(GetResourceManager()->GetNpad()->Activate(aruid.pid));
}

Result IHidServer::DeactivateNpad(ClientAppletResourceUserId aruid) {
    LOG_DEBUG(Service_HID, "called, applet_resource_user_id={}", aruid.pid);

    // This function does nothing since 10.0.0+
    R_SUCCEED();
}

Result IHidServer::AcquireNpadStyleSetUpdateEventHandle(
    OutCopyHandle<Kernel::KReadableEvent> out_event, Core::HID::NpadIdType npad_id,
    ClientAppletResourceUserId aruid, u64 unknown) {
    LOG_DEBUG(Service_HID, "called, npad_id={}, applet_resource_user_id={}, unknown={}", npad_id,
              aruid.pid, unknown);

    R_RETURN(GetResourceManager()->GetNpad()->AcquireNpadStyleSetUpdateEventHandle(
        aruid.pid, out_event, npad_id));
}

Result IHidServer::DisconnectNpad(Core::HID::NpadIdType npad_id, ClientAppletResourceUserId aruid) {
    LOG_DEBUG(Service_HID, "called, npad_id={}, applet_resource_user_id={}", npad_id, aruid.pid);

    R_RETURN(GetResourceManager()->GetNpad()->DisconnectNpad(aruid.pid, npad_id));
}

Result IHidServer::GetPlayerLedPattern(Out<Core::HID::LedPattern> out_led_pattern,
                                       Core::HID::NpadIdType npad_id) {
    LOG_DEBUG(Service_HID, "called, npad_id={}", npad_id);

    switch (npad_id) {
    case Core::HID::NpadIdType::Player1:
        *out_led_pattern = Core::HID::LedPattern{1, 0, 0, 0};
        R_SUCCEED();
    case Core::HID::NpadIdType::Player2:
        *out_led_pattern = Core::HID::LedPattern{1, 1, 0, 0};
        R_SUCCEED();
    case Core::HID::NpadIdType::Player3:
        *out_led_pattern = Core::HID::LedPattern{1, 1, 1, 0};
        R_SUCCEED();
    case Core::HID::NpadIdType::Player4:
        *out_led_pattern = Core::HID::LedPattern{1, 1, 1, 1};
        R_SUCCEED();
    case Core::HID::NpadIdType::Player5:
        *out_led_pattern = Core::HID::LedPattern{1, 0, 0, 1};
        R_SUCCEED();
    case Core::HID::NpadIdType::Player6:
        *out_led_pattern = Core::HID::LedPattern{1, 0, 1, 0};
        R_SUCCEED();
    case Core::HID::NpadIdType::Player7:
        *out_led_pattern = Core::HID::LedPattern{1, 0, 1, 1};
        R_SUCCEED();
    case Core::HID::NpadIdType::Player8:
        *out_led_pattern = Core::HID::LedPattern{0, 1, 1, 0};
        R_SUCCEED();
    default:
        *out_led_pattern = Core::HID::LedPattern{0, 0, 0, 0};
        R_SUCCEED();
    }
}

Result IHidServer::ActivateNpadWithRevision(NpadRevision revision,
                                            ClientAppletResourceUserId aruid) {
    LOG_DEBUG(Service_HID, "called, revision={}, applet_resource_user_id={}", revision, aruid.pid);

    GetResourceManager()->GetNpad()->SetRevision(aruid.pid, revision);
    R_RETURN(GetResourceManager()->GetNpad()->Activate(aruid.pid));
}

Result IHidServer::SetNpadJoyHoldType(ClientAppletResourceUserId aruid, NpadJoyHoldType hold_type) {
    LOG_DEBUG(Service_HID, "called, applet_resource_user_id={}, hold_type={}", aruid.pid,
              hold_type);

    if (hold_type != NpadJoyHoldType::Horizontal && hold_type != NpadJoyHoldType::Vertical) {
        // This should crash console
        ASSERT_MSG(false, "Invalid npad joy hold type");
    }

    R_RETURN(GetResourceManager()->GetNpad()->SetNpadJoyHoldType(aruid.pid, hold_type));
}

Result IHidServer::GetNpadJoyHoldType(Out<NpadJoyHoldType> out_hold_type,
                                      ClientAppletResourceUserId aruid) {
    LOG_DEBUG(Service_HID, "called, applet_resource_user_id={}", aruid.pid);

    R_RETURN(GetResourceManager()->GetNpad()->GetNpadJoyHoldType(aruid.pid, *out_hold_type));
}

Result IHidServer::SetNpadJoyAssignmentModeSingleByDefault(Core::HID::NpadIdType npad_id,
                                                           ClientAppletResourceUserId aruid) {
    LOG_INFO(Service_HID, "called, npad_id={}, applet_resource_user_id={}", npad_id, aruid.pid);

    Core::HID::NpadIdType new_npad_id{};
    GetResourceManager()->GetNpad()->SetNpadMode(
        aruid.pid, new_npad_id, npad_id, NpadJoyDeviceType::Left, NpadJoyAssignmentMode::Single);
    R_SUCCEED();
}

Result IHidServer::SetNpadJoyAssignmentModeSingle(Core::HID::NpadIdType npad_id,
                                                  ClientAppletResourceUserId aruid,
                                                  NpadJoyDeviceType npad_joy_device_type) {
    LOG_INFO(Service_HID, "called, npad_id={}, applet_resource_user_id={}, npad_joy_device_type={}",
             npad_id, aruid.pid, npad_joy_device_type);

    Core::HID::NpadIdType new_npad_id{};
    GetResourceManager()->GetNpad()->SetNpadMode(
        aruid.pid, new_npad_id, npad_id, npad_joy_device_type, NpadJoyAssignmentMode::Single);
    R_SUCCEED();
}

Result IHidServer::SetNpadJoyAssignmentModeDual(Core::HID::NpadIdType npad_id,
                                                ClientAppletResourceUserId aruid) {
    LOG_DEBUG(Service_HID, "called, npad_id={}, applet_resource_user_id={}", npad_id, aruid.pid);

    Core::HID::NpadIdType new_npad_id{};
    GetResourceManager()->GetNpad()->SetNpadMode(aruid.pid, new_npad_id, npad_id, {},
                                                 NpadJoyAssignmentMode::Dual);
    R_SUCCEED();
}

Result IHidServer::MergeSingleJoyAsDualJoy(Core::HID::NpadIdType npad_id_1,
                                           Core::HID::NpadIdType npad_id_2,
                                           ClientAppletResourceUserId aruid) {
    LOG_DEBUG(Service_HID, "called, npad_id_1={}, npad_id_2={}, applet_resource_user_id={}",
              npad_id_1, npad_id_2, aruid.pid);

    R_RETURN(
        GetResourceManager()->GetNpad()->MergeSingleJoyAsDualJoy(aruid.pid, npad_id_1, npad_id_2));
}

Result IHidServer::StartLrAssignmentMode(ClientAppletResourceUserId aruid) {
    LOG_DEBUG(Service_HID, "called, applet_resource_user_id={}", aruid.pid);

    GetResourceManager()->GetNpad()->StartLrAssignmentMode(aruid.pid);
    R_SUCCEED();
}

Result IHidServer::StopLrAssignmentMode(ClientAppletResourceUserId aruid) {
    LOG_DEBUG(Service_HID, "called, applet_resource_user_id={}", aruid.pid);

    GetResourceManager()->GetNpad()->StopLrAssignmentMode(aruid.pid);
    R_SUCCEED();
}

Result IHidServer::SetNpadHandheldActivationMode(ClientAppletResourceUserId aruid,
                                                 NpadHandheldActivationMode activation_mode) {
    LOG_DEBUG(Service_HID, "called, applet_resource_user_id={}, activation_mode={}", aruid.pid,
              activation_mode);

    if (activation_mode >= NpadHandheldActivationMode::MaxActivationMode) {
        // Console should crash here
        ASSERT_MSG(false, "Activation mode should be always None, Single or Dual");
        R_SUCCEED();
    }

    R_RETURN(
        GetResourceManager()->GetNpad()->SetNpadHandheldActivationMode(aruid.pid, activation_mode));
}

Result IHidServer::GetNpadHandheldActivationMode(
    Out<NpadHandheldActivationMode> out_activation_mode, ClientAppletResourceUserId aruid) {
    LOG_DEBUG(Service_HID, "called, applet_resource_user_id={}", aruid.pid);

    R_RETURN(GetResourceManager()->GetNpad()->GetNpadHandheldActivationMode(aruid.pid,
                                                                            *out_activation_mode));
}

Result IHidServer::SwapNpadAssignment(Core::HID::NpadIdType npad_id_1,
                                      Core::HID::NpadIdType npad_id_2,
                                      ClientAppletResourceUserId aruid) {
    LOG_DEBUG(Service_HID, "called, npad_id_1={}, npad_id_2={}, applet_resource_user_id={}",
              npad_id_1, npad_id_2, aruid.pid);

    R_RETURN(GetResourceManager()->GetNpad()->SwapNpadAssignment(aruid.pid, npad_id_1, npad_id_2))
}

Result IHidServer::IsUnintendedHomeButtonInputProtectionEnabled(Out<bool> out_is_enabled,
                                                                Core::HID::NpadIdType npad_id,
                                                                ClientAppletResourceUserId aruid) {
    LOG_INFO(Service_HID, "called, npad_id={}, applet_resource_user_id={}", npad_id, aruid.pid);

    R_UNLESS(IsNpadIdValid(npad_id), ResultInvalidNpadId);
    R_RETURN(GetResourceManager()->GetNpad()->IsUnintendedHomeButtonInputProtectionEnabled(
        *out_is_enabled, aruid.pid, npad_id));
}

Result IHidServer::EnableUnintendedHomeButtonInputProtection(bool is_enabled,
                                                             Core::HID::NpadIdType npad_id,
                                                             ClientAppletResourceUserId aruid) {
    LOG_DEBUG(Service_HID, "called, is_enabled={}, npad_id={}, applet_resource_user_id={}",
              is_enabled, npad_id, aruid.pid);

    R_UNLESS(IsNpadIdValid(npad_id), ResultInvalidNpadId);
    R_RETURN(GetResourceManager()->GetNpad()->EnableUnintendedHomeButtonInputProtection(
        aruid.pid, npad_id, is_enabled));
}

Result IHidServer::SetNpadJoyAssignmentModeSingleWithDestination(
    Out<bool> out_is_reassigned, Out<Core::HID::NpadIdType> out_new_npad_id,
    Core::HID::NpadIdType npad_id, ClientAppletResourceUserId aruid,
    NpadJoyDeviceType npad_joy_device_type) {
    LOG_INFO(Service_HID, "called, npad_id={}, applet_resource_user_id={}, npad_joy_device_type={}",
             npad_id, aruid.pid, npad_joy_device_type);

    *out_is_reassigned = GetResourceManager()->GetNpad()->SetNpadMode(
        aruid.pid, *out_new_npad_id, npad_id, npad_joy_device_type, NpadJoyAssignmentMode::Single);

    R_SUCCEED();
}

Result IHidServer::SetNpadAnalogStickUseCenterClamp(bool use_center_clamp,
                                                    ClientAppletResourceUserId aruid) {
    LOG_INFO(Service_HID, "called, use_center_clamp={}, applet_resource_user_id={}",
             use_center_clamp, aruid.pid);

    GetResourceManager()->GetNpad()->SetNpadAnalogStickUseCenterClamp(aruid.pid, use_center_clamp);
    R_SUCCEED();
}

Result IHidServer::SetNpadCaptureButtonAssignment(Core::HID::NpadStyleSet npad_styleset,
                                                  ClientAppletResourceUserId aruid,
                                                  Core::HID::NpadButton button) {
    LOG_INFO(Service_HID, "called, npad_styleset={}, applet_resource_user_id={}, button={}",
             npad_styleset, aruid.pid, button);

    R_RETURN(GetResourceManager()->GetNpad()->SetNpadCaptureButtonAssignment(
        aruid.pid, npad_styleset, button));
}

Result IHidServer::ClearNpadCaptureButtonAssignment(ClientAppletResourceUserId aruid) {
    LOG_INFO(Service_HID, "called, applet_resource_user_id={}", aruid.pid);

    R_RETURN(GetResourceManager()->GetNpad()->ClearNpadCaptureButtonAssignment(aruid.pid));
}

Result IHidServer::GetVibrationDeviceInfo(
    Out<Core::HID::VibrationDeviceInfo> out_vibration_device_info,
    Core::HID::VibrationDeviceHandle vibration_device_handle) {
    LOG_DEBUG(Service_HID, "called, npad_type={}, npad_id={}, device_index={}",
              vibration_device_handle.npad_type, vibration_device_handle.npad_id,
              vibration_device_handle.device_index);

    R_RETURN(GetResourceManager()->GetVibrationDeviceInfo(*out_vibration_device_info,
                                                          vibration_device_handle));
}

Result IHidServer::SendVibrationValue(Core::HID::VibrationDeviceHandle vibration_device_handle,
                                      Core::HID::VibrationValue vibration_value,
                                      ClientAppletResourceUserId aruid) {
    LOG_DEBUG(Service_HID,
              "called, npad_type={}, npad_id={}, device_index={}, applet_resource_user_id={}",
              vibration_device_handle.npad_type, vibration_device_handle.npad_id,
              vibration_device_handle.device_index, aruid.pid);

    GetResourceManager()->SendVibrationValue(aruid.pid, vibration_device_handle, vibration_value);
    R_SUCCEED()
}

Result IHidServer::GetActualVibrationValue(Out<Core::HID::VibrationValue> out_vibration_value,
                                           Core::HID::VibrationDeviceHandle vibration_device_handle,
                                           ClientAppletResourceUserId aruid) {
    LOG_DEBUG(Service_HID,
              "called, npad_type={}, npad_id={}, device_index={}, applet_resource_user_id={}",
              vibration_device_handle.npad_type, vibration_device_handle.npad_id,
              vibration_device_handle.device_index, aruid.pid);

    bool has_active_aruid{};
    R_TRY(GetResourceManager()->IsVibrationAruidActive(aruid.pid, has_active_aruid));

    if (!has_active_aruid) {
        *out_vibration_value = Core::HID::DEFAULT_VIBRATION_VALUE;
        R_SUCCEED();
    }

    R_TRY(IsVibrationHandleValid(vibration_device_handle));
    NpadVibrationDevice* device =
        GetResourceManager()->GetNSVibrationDevice(vibration_device_handle);

    if (device == nullptr || R_FAILED(device->GetActualVibrationValue(*out_vibration_value))) {
        *out_vibration_value = Core::HID::DEFAULT_VIBRATION_VALUE;
    }

    R_SUCCEED();
}

Result IHidServer::CreateActiveVibrationDeviceList(
    OutInterface<IActiveVibrationDeviceList> out_interface) {
    LOG_DEBUG(Service_HID, "called");

    *out_interface = std::make_shared<IActiveVibrationDeviceList>(system, GetResourceManager());
    R_SUCCEED();
}

Result IHidServer::PermitVibration(bool can_vibrate) {
    LOG_DEBUG(Service_HID, "called, can_vibrate={}", can_vibrate);

    R_RETURN(GetResourceManager()->GetNpad()->GetVibrationHandler()->SetVibrationMasterVolume(
        can_vibrate ? 1.0f : 0.0f));
}

Result IHidServer::IsVibrationPermitted(Out<bool> out_is_permitted) {
    LOG_DEBUG(Service_HID, "called");

    f32 master_volume{};
    R_TRY(GetResourceManager()->GetNpad()->GetVibrationHandler()->GetVibrationMasterVolume(
        master_volume));

    *out_is_permitted = master_volume > 0.0f;
    R_SUCCEED();
}

Result IHidServer::SendVibrationValues(
    ClientAppletResourceUserId aruid,
    InArray<Core::HID::VibrationDeviceHandle, BufferAttr_HipcPointer> vibration_handles,
    InArray<Core::HID::VibrationValue, BufferAttr_HipcPointer> vibration_values) {
    LOG_DEBUG(Service_HID, "called, applet_resource_user_id={}", aruid.pid);

    R_UNLESS(vibration_handles.size() == vibration_values.size(), ResultVibrationArraySizeMismatch);

    for (std::size_t i = 0; i < vibration_handles.size(); i++) {
        R_TRY(GetResourceManager()->SendVibrationValue(aruid.pid, vibration_handles[i],
                                                       vibration_values[i]));
    }

    R_SUCCEED();
}

Result IHidServer::SendVibrationGcErmCommand(
    Core::HID::VibrationDeviceHandle vibration_device_handle, ClientAppletResourceUserId aruid,
    Core::HID::VibrationGcErmCommand gc_erm_command) {
    LOG_DEBUG(Service_HID,
              "called, npad_type={}, npad_id={}, device_index={}, applet_resource_user_id={}, "
              "gc_erm_command={}",
              vibration_device_handle.npad_type, vibration_device_handle.npad_id,
              vibration_device_handle.device_index, aruid.pid, gc_erm_command);

    bool has_active_aruid{};
    R_TRY(GetResourceManager()->IsVibrationAruidActive(aruid.pid, has_active_aruid));

    if (!has_active_aruid) {
        R_SUCCEED();
    }

    R_TRY(IsVibrationHandleValid(vibration_device_handle));
    NpadGcVibrationDevice* gc_device =
        GetResourceManager()->GetGcVibrationDevice(vibration_device_handle);
    if (gc_device != nullptr) {
        R_RETURN(gc_device->SendVibrationGcErmCommand(gc_erm_command));
    }

    R_SUCCEED();
}

Result IHidServer::GetActualVibrationGcErmCommand(
    Out<Core::HID::VibrationGcErmCommand> out_gc_erm_command,
    Core::HID::VibrationDeviceHandle vibration_device_handle, ClientAppletResourceUserId aruid) {
    LOG_DEBUG(Service_HID,
              "called, npad_type={}, npad_id={}, device_index={}, applet_resource_user_id={}",
              vibration_device_handle.npad_type, vibration_device_handle.npad_id,
              vibration_device_handle.device_index, aruid.pid);

    bool has_active_aruid{};
    R_TRY(GetResourceManager()->IsVibrationAruidActive(aruid.pid, has_active_aruid));

    if (!has_active_aruid) {
        *out_gc_erm_command = Core::HID::VibrationGcErmCommand::Stop;
    }

    R_TRY(IsVibrationHandleValid(vibration_device_handle));
    NpadGcVibrationDevice* gc_device =
        GetResourceManager()->GetGcVibrationDevice(vibration_device_handle);

    if (gc_device == nullptr ||
        R_FAILED(gc_device->GetActualVibrationGcErmCommand(*out_gc_erm_command))) {
        *out_gc_erm_command = Core::HID::VibrationGcErmCommand::Stop;
    }

    R_SUCCEED();
}

Result IHidServer::BeginPermitVibrationSession(ClientAppletResourceUserId aruid) {
    LOG_DEBUG(Service_HID, "called, applet_resource_user_id={}", aruid.pid);

    R_RETURN(GetResourceManager()->GetNpad()->GetVibrationHandler()->BeginPermitVibrationSession(
        aruid.pid));
}

Result IHidServer::EndPermitVibrationSession(ClientAppletResourceUserId aruid) {
    LOG_DEBUG(Service_HID, "called");

    R_RETURN(GetResourceManager()->GetNpad()->GetVibrationHandler()->EndPermitVibrationSession());
}

Result IHidServer::IsVibrationDeviceMounted(
    Out<bool> out_is_mounted, Core::HID::VibrationDeviceHandle vibration_device_handle,
    ClientAppletResourceUserId aruid) {
    LOG_DEBUG(Service_HID,
              "called, npad_type={}, npad_id={}, device_index={}, applet_resource_user_id={}",
              vibration_device_handle.npad_type, vibration_device_handle.npad_id,
              vibration_device_handle.device_index, aruid.pid);

    R_TRY(IsVibrationHandleValid(vibration_device_handle));

    NpadVibrationBase* device = GetResourceManager()->GetVibrationDevice(vibration_device_handle);

    if (device != nullptr) {
        *out_is_mounted = device->IsVibrationMounted();
    }

    R_SUCCEED();
}

Result IHidServer::SendVibrationValueInBool(
    bool is_vibrating, Core::HID::VibrationDeviceHandle vibration_device_handle,
    ClientAppletResourceUserId aruid) {
    LOG_DEBUG(Service_HID,
              "called, npad_type={}, npad_id={}, device_index={}, applet_resource_user_id={}, "
              "is_vibrating={}",
              vibration_device_handle.npad_type, vibration_device_handle.npad_id,
              vibration_device_handle.device_index, aruid.pid, is_vibrating);

    bool has_active_aruid{};
    R_TRY(GetResourceManager()->IsVibrationAruidActive(aruid.pid, has_active_aruid));

    if (!has_active_aruid) {
        R_SUCCEED();
    }

    R_TRY(IsVibrationHandleValid(vibration_device_handle));
    NpadN64VibrationDevice* n64_device =
        GetResourceManager()->GetN64VibrationDevice(vibration_device_handle);

    if (n64_device != nullptr) {
        R_TRY(n64_device->SendValueInBool(is_vibrating));
    }

    R_SUCCEED();
}

Result IHidServer::ActivateConsoleSixAxisSensor(ClientAppletResourceUserId aruid) {
    LOG_INFO(Service_HID, "called, applet_resource_user_id={}", aruid.pid);

    if (!firmware_settings->IsDeviceManaged()) {
        R_TRY(GetResourceManager()->GetConsoleSixAxis()->Activate());
    }

    R_RETURN(GetResourceManager()->GetConsoleSixAxis()->Activate(aruid.pid));
}

Result IHidServer::StartConsoleSixAxisSensor(
    Core::HID::ConsoleSixAxisSensorHandle console_sixaxis_handle,
    ClientAppletResourceUserId aruid) {
    LOG_WARNING(Service_HID,
                "(STUBBED) called, unknown_1={}, unknown_2={}, applet_resource_user_id={}",
                console_sixaxis_handle.unknown_1, console_sixaxis_handle.unknown_2, aruid.pid);
    R_SUCCEED();
}

Result IHidServer::StopConsoleSixAxisSensor(
    Core::HID::ConsoleSixAxisSensorHandle console_sixaxis_handle,
    ClientAppletResourceUserId aruid) {
    LOG_WARNING(Service_HID,
                "(STUBBED) called, unknown_1={}, unknown_2={}, applet_resource_user_id={}",
                console_sixaxis_handle.unknown_1, console_sixaxis_handle.unknown_2, aruid.pid);
    R_SUCCEED();
}

Result IHidServer::ActivateSevenSixAxisSensor(ClientAppletResourceUserId aruid) {
    LOG_INFO(Service_HID, "called, applet_resource_user_id={}", aruid.pid);

    if (!firmware_settings->IsDeviceManaged()) {
        R_TRY(GetResourceManager()->GetSevenSixAxis()->Activate());
    }

    GetResourceManager()->GetSevenSixAxis()->Activate(aruid.pid);
    R_SUCCEED();
}

Result IHidServer::StartSevenSixAxisSensor(ClientAppletResourceUserId aruid) {
    LOG_WARNING(Service_HID, "(STUBBED) called, applet_resource_user_id={}", aruid.pid);
    R_SUCCEED();
}

Result IHidServer::StopSevenSixAxisSensor(ClientAppletResourceUserId aruid) {
    LOG_WARNING(Service_HID, "(STUBBED) called, applet_resource_user_id={}", aruid.pid);
    R_SUCCEED();
}

Result IHidServer::InitializeSevenSixAxisSensor(ClientAppletResourceUserId aruid, u64 t_mem_1_size,
                                                u64 t_mem_2_size,
                                                InCopyHandle<Kernel::KTransferMemory> t_mem_1,
                                                InCopyHandle<Kernel::KTransferMemory> t_mem_2) {
    LOG_WARNING(Service_HID,
                "called, t_mem_1_size=0x{:08X}, t_mem_2_size=0x{:08X}, "
                "applet_resource_user_id={}",
                t_mem_1_size, t_mem_2_size, aruid.pid);

    ASSERT_MSG(t_mem_1_size == 0x1000, "t_mem_1_size is not 0x1000 bytes");
    ASSERT_MSG(t_mem_2_size == 0x7F000, "t_mem_2_size is not 0x7F000 bytes");

    ASSERT_MSG(t_mem_1->GetSize() == 0x1000, "t_mem_1 has incorrect size");
    ASSERT_MSG(t_mem_2->GetSize() == 0x7F000, "t_mem_2 has incorrect size");

    // Activate console six axis controller
    GetResourceManager()->GetConsoleSixAxis()->Activate();
    GetResourceManager()->GetSevenSixAxis()->Activate();

    GetResourceManager()->GetSevenSixAxis()->SetTransferMemoryAddress(t_mem_1->GetSourceAddress());

    R_SUCCEED();
}

Result IHidServer::FinalizeSevenSixAxisSensor(ClientAppletResourceUserId aruid) {
    LOG_WARNING(Service_HID, "(STUBBED) called, applet_resource_user_id={}", aruid.pid);

    R_SUCCEED();
}

Result IHidServer::ResetSevenSixAxisSensorTimestamp(ClientAppletResourceUserId aruid) {
    LOG_WARNING(Service_HID, "called, applet_resource_user_id={}", aruid.pid);

    GetResourceManager()->GetSevenSixAxis()->ResetTimestamp();
    R_SUCCEED();
}

Result IHidServer::IsUsbFullKeyControllerEnabled(Out<bool> out_is_enabled,
                                                 ClientAppletResourceUserId aruid) {
    LOG_WARNING(Service_HID, "(STUBBED) called");

    *out_is_enabled = false;
    R_SUCCEED();
}

Result IHidServer::GetPalmaConnectionHandle(Out<Palma::PalmaConnectionHandle> out_handle,
                                            Core::HID::NpadIdType npad_id,
                                            ClientAppletResourceUserId aruid) {
    LOG_WARNING(Service_HID, "(STUBBED) called, npad_id={}, applet_resource_user_id={}", npad_id,
                aruid.pid);

    R_RETURN(GetResourceManager()->GetPalma()->GetPalmaConnectionHandle(npad_id, *out_handle));
}

Result IHidServer::InitializePalma(Palma::PalmaConnectionHandle connection_handle) {
    LOG_WARNING(Service_HID, "(STUBBED) called, connection_handle={}", connection_handle.npad_id);

    R_RETURN(GetResourceManager()->GetPalma()->InitializePalma(connection_handle));
}

Result IHidServer::AcquirePalmaOperationCompleteEvent(
    OutCopyHandle<Kernel::KReadableEvent> out_event,
    Palma::PalmaConnectionHandle connection_handle) {
    LOG_WARNING(Service_HID, "(STUBBED) called, connection_handle={}", connection_handle.npad_id);

    *out_event =
        &GetResourceManager()->GetPalma()->AcquirePalmaOperationCompleteEvent(connection_handle);
    R_SUCCEED();
}

Result IHidServer::GetPalmaOperationInfo(Out<Palma::PalmaOperationType> out_operation_type,
                                         Palma::PalmaConnectionHandle connection_handle,
                                         OutBuffer<BufferAttr_HipcMapAlias> out_data) {
    LOG_WARNING(Service_HID, "(STUBBED) called, connection_handle={}", connection_handle.npad_id);

    R_RETURN(GetResourceManager()->GetPalma()->GetPalmaOperationInfo(
        connection_handle, *out_operation_type, out_data));
}

Result IHidServer::PlayPalmaActivity(Palma::PalmaConnectionHandle connection_handle,
                                     u64 palma_activity) {
    LOG_WARNING(Service_HID, "(STUBBED) called, connection_handle={}, palma_activity={}",
                connection_handle.npad_id, palma_activity);

    R_RETURN(
        GetResourceManager()->GetPalma()->PlayPalmaActivity(connection_handle, palma_activity));
}

Result IHidServer::SetPalmaFrModeType(Palma::PalmaConnectionHandle connection_handle,
                                      Palma::PalmaFrModeType fr_mode) {
    LOG_WARNING(Service_HID, "(STUBBED) called, connection_handle={}, fr_mode={}",
                connection_handle.npad_id, fr_mode);

    R_RETURN(GetResourceManager()->GetPalma()->SetPalmaFrModeType(connection_handle, fr_mode));
}

Result IHidServer::ReadPalmaStep(Palma::PalmaConnectionHandle connection_handle) {
    LOG_WARNING(Service_HID, "(STUBBED) called, connection_handle={}", connection_handle.npad_id);

    R_RETURN(GetResourceManager()->GetPalma()->ReadPalmaStep(connection_handle));
}

Result IHidServer::EnablePalmaStep(bool is_enabled,
                                   Palma::PalmaConnectionHandle connection_handle) {
    LOG_WARNING(Service_HID, "(STUBBED) called, connection_handle={}, is_enabled={}",
                connection_handle.npad_id, is_enabled);

    R_RETURN(GetResourceManager()->GetPalma()->EnablePalmaStep(connection_handle, is_enabled));
}

Result IHidServer::ResetPalmaStep(Palma::PalmaConnectionHandle connection_handle) {
    LOG_WARNING(Service_HID, "(STUBBED) called, connection_handle={}", connection_handle.npad_id);

    R_RETURN(GetResourceManager()->GetPalma()->ResetPalmaStep(connection_handle));
}

Result IHidServer::ReadPalmaApplicationSection(Palma::PalmaConnectionHandle connection_handle,
                                               u64 offset, u64 size) {
    LOG_WARNING(Service_HID, "(STUBBED) called, connection_handle={}, offset={}, size={}",
                connection_handle.npad_id, offset, size);
    R_SUCCEED();
}

Result IHidServer::WritePalmaApplicationSection(
    Palma::PalmaConnectionHandle connection_handle, u64 offset, u64 size,
    InLargeData<Palma::PalmaApplicationSection, BufferAttr_HipcPointer> data) {
    LOG_WARNING(Service_HID, "(STUBBED) called, connection_handle={}, offset={}, size={}",
                connection_handle.npad_id, offset, size);
    R_SUCCEED();
}

Result IHidServer::ReadPalmaUniqueCode(Palma::PalmaConnectionHandle connection_handle) {
    LOG_WARNING(Service_HID, "(STUBBED) called, connection_handle={}", connection_handle.npad_id);

    GetResourceManager()->GetPalma()->ReadPalmaUniqueCode(connection_handle);
    R_SUCCEED();
}

Result IHidServer::SetPalmaUniqueCodeInvalid(Palma::PalmaConnectionHandle connection_handle) {
    LOG_WARNING(Service_HID, "(STUBBED) called, connection_handle={}", connection_handle.npad_id);

    GetResourceManager()->GetPalma()->SetPalmaUniqueCodeInvalid(connection_handle);
    R_SUCCEED();
}

Result IHidServer::WritePalmaActivityEntry(Palma::PalmaConnectionHandle connection_handle,
                                           Palma::PalmaActivityEntry activity_entry) {
    LOG_WARNING(Service_HID, "(STUBBED) called, connection_handle={}", connection_handle.npad_id);
    R_SUCCEED();
}

Result IHidServer::WritePalmaRgbLedPatternEntry(Palma::PalmaConnectionHandle connection_handle,
                                                u64 unknown,
                                                InBuffer<BufferAttr_HipcMapAlias> led_pattern) {
    LOG_WARNING(Service_HID, "(STUBBED) called, connection_handle={}, unknown={}",
                connection_handle.npad_id, unknown);

    GetResourceManager()->GetPalma()->WritePalmaRgbLedPatternEntry(connection_handle, unknown);
    R_SUCCEED();
}

Result IHidServer::WritePalmaWaveEntry(Palma::PalmaConnectionHandle connection_handle,
                                       Palma::PalmaWaveSet wave_set, u64 unknown, u64 t_mem_size,
                                       u64 size, InCopyHandle<Kernel::KTransferMemory> t_mem) {
    ASSERT_MSG(t_mem->GetSize() == t_mem_size, "t_mem has incorrect size");

    LOG_WARNING(
        Service_HID,
        "(STUBBED) called, connection_handle={}, wave_set={}, unknown={}, t_mem_size={}, size={}",
        connection_handle.npad_id, wave_set, unknown, t_mem_size, size);

    GetResourceManager()->GetPalma()->WritePalmaWaveEntry(connection_handle, wave_set,
                                                          t_mem->GetSourceAddress(), t_mem_size);
    R_SUCCEED();
}

Result IHidServer::SetPalmaDataBaseIdentificationVersion(
    s32 database_id_version, Palma::PalmaConnectionHandle connection_handle) {
    LOG_WARNING(Service_HID, "(STUBBED) called, connection_handle={}, database_id_version={}",
                connection_handle.npad_id, database_id_version);

    GetResourceManager()->GetPalma()->SetPalmaDataBaseIdentificationVersion(connection_handle,
                                                                            database_id_version);
    R_SUCCEED();
}

Result IHidServer::GetPalmaDataBaseIdentificationVersion(
    Palma::PalmaConnectionHandle connection_handle) {
    LOG_WARNING(Service_HID, "(STUBBED) called, connection_handle={}", connection_handle.npad_id);

    R_RETURN(
        GetResourceManager()->GetPalma()->GetPalmaDataBaseIdentificationVersion(connection_handle));
}

Result IHidServer::SuspendPalmaFeature(Palma::PalmaFeature feature,
                                       Palma::PalmaConnectionHandle connection_handle) {
    LOG_WARNING(Service_HID, "(STUBBED) called, feature={}, connection_handle={}", feature,
                connection_handle.npad_id);
    R_SUCCEED();
}

Result IHidServer::GetPalmaOperationResult(Palma::PalmaConnectionHandle connection_handle) {
    LOG_WARNING(Service_HID, "(STUBBED) called, connection_handle={}", connection_handle.npad_id);

    R_RETURN(GetResourceManager()->GetPalma()->GetPalmaOperationResult(connection_handle));
}

Result IHidServer::ReadPalmaPlayLog(u16 unknown, Palma::PalmaConnectionHandle connection_handle) {
    LOG_WARNING(Service_HID, "(STUBBED) called, unknown={}, connection_handle={}", unknown,
                connection_handle.npad_id);
    R_SUCCEED();
}

Result IHidServer::ResetPalmaPlayLog(u16 unknown, Palma::PalmaConnectionHandle connection_handle) {
    LOG_WARNING(Service_HID, "(STUBBED) called, unknown={}, connection_handle={}", unknown,
                connection_handle.npad_id);
    R_SUCCEED();
}

Result IHidServer::SetIsPalmaAllConnectable(bool is_palma_all_connectable,
                                            ClientAppletResourceUserId aruid) {
    LOG_WARNING(Service_HID,
                "(STUBBED) called, is_palma_all_connectable={}, applet_resource_user_id={}",
                is_palma_all_connectable, aruid.pid);

    GetResourceManager()->GetPalma()->SetIsPalmaAllConnectable(is_palma_all_connectable);
    R_SUCCEED();
}

Result IHidServer::SetIsPalmaPairedConnectable(bool is_palma_paired_connectable,
                                               ClientAppletResourceUserId aruid) {
    LOG_WARNING(Service_HID,
                "(STUBBED) called, is_palma_paired_connectable={}, applet_resource_user_id={}",
                is_palma_paired_connectable, aruid.pid);
    R_SUCCEED();
}

Result IHidServer::PairPalma(Palma::PalmaConnectionHandle connection_handle) {
    LOG_WARNING(Service_HID, "(STUBBED) called, connection_handle={}", connection_handle.npad_id);

    GetResourceManager()->GetPalma()->PairPalma(connection_handle);
    R_SUCCEED();
}

Result IHidServer::SetPalmaBoostMode(bool is_enabled) {
    LOG_WARNING(Service_HID, "(STUBBED) called, is_enabled={}", is_enabled);

    GetResourceManager()->GetPalma()->SetPalmaBoostMode(is_enabled);
    R_SUCCEED();
}

Result IHidServer::CancelWritePalmaWaveEntry(Palma::PalmaConnectionHandle connection_handle) {
    LOG_WARNING(Service_HID, "(STUBBED) called, connection_handle={}", connection_handle.npad_id);
    R_SUCCEED();
}

Result IHidServer::EnablePalmaBoostMode(bool is_enabled, ClientAppletResourceUserId aruid) {
    LOG_WARNING(Service_HID, "(STUBBED) called, is_enabled={}, applet_resource_user_id={}",
                is_enabled, aruid.pid);
    R_SUCCEED();
}

Result IHidServer::GetPalmaBluetoothAddress(Out<Palma::Address> out_bt_address,
                                            Palma::PalmaConnectionHandle connection_handle) {
    LOG_WARNING(Service_HID, "(STUBBED) called, connection_handle={}", connection_handle.npad_id);
    R_SUCCEED();
}

Result IHidServer::SetDisallowedPalmaConnection(
    ClientAppletResourceUserId aruid,
    InArray<Palma::Address, BufferAttr_HipcPointer> disallowed_address) {
    LOG_DEBUG(Service_HID, "(STUBBED) called, applet_resource_user_id={}", aruid.pid);
    R_SUCCEED();
}

Result IHidServer::SetNpadCommunicationMode(ClientAppletResourceUserId aruid,
                                            NpadCommunicationMode communication_mode) {
    LOG_DEBUG(Service_HID, "called, applet_resource_user_id={}, communication_mode={}", aruid.pid,
              communication_mode);

    // This function has been stubbed since 2.0.0+
    R_SUCCEED();
}

Result IHidServer::GetNpadCommunicationMode(Out<NpadCommunicationMode> out_communication_mode,
                                            ClientAppletResourceUserId aruid) {
    LOG_DEBUG(Service_HID, "called, applet_resource_user_id={}", aruid.pid);

    // This function has been stubbed since 2.0.0+
    *out_communication_mode = NpadCommunicationMode::Default;
    R_SUCCEED();
}

Result IHidServer::SetTouchScreenConfiguration(
    Core::HID::TouchScreenConfigurationForNx touchscreen_config, ClientAppletResourceUserId aruid) {
    LOG_INFO(Service_HID, "called, touchscreen_config={}, applet_resource_user_id={}",
             touchscreen_config.mode, aruid.pid);

    if (touchscreen_config.mode != Core::HID::TouchScreenModeForNx::Heat2 &&
        touchscreen_config.mode != Core::HID::TouchScreenModeForNx::Finger) {
        touchscreen_config.mode = Core::HID::TouchScreenModeForNx::UseSystemSetting;
    }

    R_RETURN(GetResourceManager()->GetTouchScreen()->SetTouchScreenConfiguration(touchscreen_config,
                                                                                 aruid.pid));
}

Result IHidServer::IsFirmwareUpdateNeededForNotification(Out<bool> out_is_firmware_update_needed,
                                                         s32 unknown,
                                                         ClientAppletResourceUserId aruid) {
    LOG_WARNING(Service_HID, "(STUBBED) called, unknown={}, applet_resource_user_id={}", unknown,
                aruid.pid);

    *out_is_firmware_update_needed = false;
    R_SUCCEED();
}

Result IHidServer::SetTouchScreenResolution(u32 width, u32 height,
                                            ClientAppletResourceUserId aruid) {
    LOG_INFO(Service_HID, "called, width={}, height={}, applet_resource_user_id={}", width, height,
             aruid.pid);

    GetResourceManager()->GetTouchScreen()->SetTouchScreenResolution(width, height, aruid.pid);
    R_SUCCEED();
}

std::shared_ptr<ResourceManager> IHidServer::GetResourceManager() {
    resource_manager->Initialize();
    return resource_manager;
}

} // namespace Service::HID
