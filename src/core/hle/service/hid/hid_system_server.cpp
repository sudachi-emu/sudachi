// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/hle/service/hid/hid_system_server.h"
#include "core/hle/service/ipc_helpers.h"
#include "core/hle/service/set/settings_types.h"
#include "hid_core/hid_result.h"
#include "hid_core/resource_manager.h"
#include "hid_core/resources/hid_firmware_settings.h"
#include "hid_core/resources/npad/npad.h"
#include "hid_core/resources/npad/npad_types.h"
#include "hid_core/resources/npad/npad_vibration.h"
#include "hid_core/resources/palma/palma.h"
#include "hid_core/resources/touch_screen/touch_screen.h"

namespace Service::HID {

IHidSystemServer::IHidSystemServer(Core::System& system_, std::shared_ptr<ResourceManager> resource,
                                   std::shared_ptr<HidFirmwareSettings> settings)
    : ServiceFramework{system_, "hid:sys"}, service_context{system_, service_name},
      resource_manager{resource}, firmware_settings{settings} {
    // clang-format off
    static const FunctionInfo functions[] = {
        {31, nullptr, "SendKeyboardLockKeyEvent"},
        {101, nullptr, "AcquireHomeButtonEventHandle"},
        {111, nullptr, "ActivateHomeButton"},
        {121, nullptr, "AcquireSleepButtonEventHandle"},
        {131, nullptr, "ActivateSleepButton"},
        {141, nullptr, "AcquireCaptureButtonEventHandle"},
        {151, nullptr, "ActivateCaptureButton"},
        {161, &IHidSystemServer::GetPlatformConfig, "GetPlatformConfig"},
        {210, nullptr, "AcquireNfcDeviceUpdateEventHandle"},
        {211, nullptr, "GetNpadsWithNfc"},
        {212, nullptr, "AcquireNfcActivateEventHandle"},
        {213, nullptr, "ActivateNfc"},
        {214, nullptr, "GetXcdHandleForNpadWithNfc"},
        {215, nullptr, "IsNfcActivated"},
        {230, nullptr, "AcquireIrSensorEventHandle"},
        {231, nullptr, "ActivateIrSensor"},
        {232, nullptr, "GetIrSensorState"},
        {233, nullptr, "GetXcdHandleForNpadWithIrSensor"},
        {301, nullptr, "ActivateNpadSystem"},
        {303, &IHidSystemServer::ApplyNpadSystemCommonPolicy, "ApplyNpadSystemCommonPolicy"},
        {304, &IHidSystemServer::EnableAssigningSingleOnSlSrPress, "EnableAssigningSingleOnSlSrPress"},
        {305, &IHidSystemServer::DisableAssigningSingleOnSlSrPress, "DisableAssigningSingleOnSlSrPress"},
        {306, &IHidSystemServer::GetLastActiveNpad, "GetLastActiveNpad"},
        {307, nullptr, "GetNpadSystemExtStyle"},
        {308, &IHidSystemServer::ApplyNpadSystemCommonPolicyFull, "ApplyNpadSystemCommonPolicyFull"},
        {309, &IHidSystemServer::GetNpadFullKeyGripColor, "GetNpadFullKeyGripColor"},
        {310, &IHidSystemServer::GetMaskedSupportedNpadStyleSet, "GetMaskedSupportedNpadStyleSet"},
        {311, nullptr, "SetNpadPlayerLedBlinkingDevice"},
        {312, &IHidSystemServer::SetSupportedNpadStyleSetAll, "SetSupportedNpadStyleSetAll"},
        {313, &IHidSystemServer::GetNpadCaptureButtonAssignment, "GetNpadCaptureButtonAssignment"},
        {314, nullptr, "GetAppletFooterUiType"},
        {315, &IHidSystemServer::GetAppletDetailedUiType, "GetAppletDetailedUiType"},
        {316, &IHidSystemServer::GetNpadInterfaceType, "GetNpadInterfaceType"},
        {317, &IHidSystemServer::GetNpadLeftRightInterfaceType, "GetNpadLeftRightInterfaceType"},
        {318, &IHidSystemServer::HasBattery, "HasBattery"},
        {319, &IHidSystemServer::HasLeftRightBattery, "HasLeftRightBattery"},
        {321, &IHidSystemServer::GetUniquePadsFromNpad, "GetUniquePadsFromNpad"},
        {322, &IHidSystemServer::SetNpadSystemExtStateEnabled, "SetNpadSystemExtStateEnabled"},
        {323, nullptr, "GetLastActiveUniquePad"},
        {324, nullptr, "GetUniquePadButtonSet"},
        {325, nullptr, "GetUniquePadColor"},
        {326, nullptr, "GetUniquePadAppletDetailedUiType"},
        {327, nullptr, "GetAbstractedPadIdDataFromNpad"},
        {328, nullptr, "AttachAbstractedPadToNpad"},
        {329, nullptr, "DetachAbstractedPadAll"},
        {330, nullptr, "CheckAbstractedPadConnection"},
        {500, nullptr, "SetAppletResourceUserId"},
        {501, &IHidSystemServer::RegisterAppletResourceUserId, "RegisterAppletResourceUserId"},
        {502, &IHidSystemServer::UnregisterAppletResourceUserId, "UnregisterAppletResourceUserId"},
        {503, &IHidSystemServer::EnableAppletToGetInput, "EnableAppletToGetInput"},
        {504, &IHidSystemServer::SetAruidValidForVibration, "SetAruidValidForVibration"},
        {505, &IHidSystemServer::EnableAppletToGetSixAxisSensor, "EnableAppletToGetSixAxisSensor"},
        {506, &IHidSystemServer::EnableAppletToGetPadInput, "EnableAppletToGetPadInput"},
        {507, &IHidSystemServer::EnableAppletToGetTouchScreen, "EnableAppletToGetTouchScreen"},
        {510, &IHidSystemServer::SetVibrationMasterVolume, "SetVibrationMasterVolume"},
        {511, &IHidSystemServer::GetVibrationMasterVolume, "GetVibrationMasterVolume"},
        {512, &IHidSystemServer::BeginPermitVibrationSession, "BeginPermitVibrationSession"},
        {513, &IHidSystemServer::EndPermitVibrationSession, "EndPermitVibrationSession"},
        {514, nullptr, "Unknown514"},
        {520, nullptr, "EnableHandheldHids"},
        {521, nullptr, "DisableHandheldHids"},
        {522, nullptr, "SetJoyConRailEnabled"},
        {523, &IHidSystemServer::IsJoyConRailEnabled, "IsJoyConRailEnabled"},
        {524, nullptr, "IsHandheldHidsEnabled"},
        {525, &IHidSystemServer::IsJoyConAttachedOnAllRail, "IsJoyConAttachedOnAllRail"},
        {540, nullptr, "AcquirePlayReportControllerUsageUpdateEvent"},
        {541, nullptr, "GetPlayReportControllerUsages"},
        {542, nullptr, "AcquirePlayReportRegisteredDeviceUpdateEvent"},
        {543, nullptr, "GetRegisteredDevicesOld"},
        {544, &IHidSystemServer::AcquireConnectionTriggerTimeoutEvent, "AcquireConnectionTriggerTimeoutEvent"},
        {545, nullptr, "SendConnectionTrigger"},
        {546, &IHidSystemServer::AcquireDeviceRegisteredEventForControllerSupport, "AcquireDeviceRegisteredEventForControllerSupport"},
        {547, nullptr, "GetAllowedBluetoothLinksCount"},
        {548, &IHidSystemServer::GetRegisteredDevices, "GetRegisteredDevices"},
        {549, nullptr, "GetConnectableRegisteredDevices"},
        {700, nullptr, "ActivateUniquePad"},
        {702, &IHidSystemServer::AcquireUniquePadConnectionEventHandle, "AcquireUniquePadConnectionEventHandle"},
        {703, &IHidSystemServer::GetUniquePadIds, "GetUniquePadIds"},
        {751, &IHidSystemServer::AcquireJoyDetachOnBluetoothOffEventHandle, "AcquireJoyDetachOnBluetoothOffEventHandle"},
        {800, nullptr, "ListSixAxisSensorHandles"},
        {801, nullptr, "IsSixAxisSensorUserCalibrationSupported"},
        {802, nullptr, "ResetSixAxisSensorCalibrationValues"},
        {803, nullptr, "StartSixAxisSensorUserCalibration"},
        {804, nullptr, "CancelSixAxisSensorUserCalibration"},
        {805, nullptr, "GetUniquePadBluetoothAddress"},
        {806, nullptr, "DisconnectUniquePad"},
        {807, nullptr, "GetUniquePadType"},
        {808, nullptr, "GetUniquePadInterface"},
        {809, nullptr, "GetUniquePadSerialNumber"},
        {810, nullptr, "GetUniquePadControllerNumber"},
        {811, nullptr, "GetSixAxisSensorUserCalibrationStage"},
        {812, nullptr, "GetConsoleUniqueSixAxisSensorHandle"},
        {821, nullptr, "StartAnalogStickManualCalibration"},
        {822, nullptr, "RetryCurrentAnalogStickManualCalibrationStage"},
        {823, nullptr, "CancelAnalogStickManualCalibration"},
        {824, nullptr, "ResetAnalogStickManualCalibration"},
        {825, nullptr, "GetAnalogStickState"},
        {826, nullptr, "GetAnalogStickManualCalibrationStage"},
        {827, nullptr, "IsAnalogStickButtonPressed"},
        {828, nullptr, "IsAnalogStickInReleasePosition"},
        {829, nullptr, "IsAnalogStickInCircumference"},
        {830, nullptr, "SetNotificationLedPattern"},
        {831, nullptr, "SetNotificationLedPatternWithTimeout"},
        {832, nullptr, "PrepareHidsForNotificationWake"},
        {850, &IHidSystemServer::IsUsbFullKeyControllerEnabled, "IsUsbFullKeyControllerEnabled"},
        {851, &IHidSystemServer::EnableUsbFullKeyController, "EnableUsbFullKeyController"},
        {852, nullptr, "IsUsbConnected"},
        {870, &IHidSystemServer::IsHandheldButtonPressedOnConsoleMode, "IsHandheldButtonPressedOnConsoleMode"},
        {900, nullptr, "ActivateInputDetector"},
        {901, nullptr, "NotifyInputDetector"},
        {1000, &IHidSystemServer::InitializeFirmwareUpdate, "InitializeFirmwareUpdate"},
        {1001, nullptr, "GetFirmwareVersion"},
        {1002, nullptr, "GetAvailableFirmwareVersion"},
        {1003, nullptr, "IsFirmwareUpdateAvailable"},
        {1004, &IHidSystemServer::CheckFirmwareUpdateRequired, "CheckFirmwareUpdateRequired"},
        {1005, nullptr, "StartFirmwareUpdate"},
        {1006, nullptr, "AbortFirmwareUpdate"},
        {1007, nullptr, "GetFirmwareUpdateState"},
        {1008, nullptr, "ActivateAudioControl"},
        {1009, nullptr, "AcquireAudioControlEventHandle"},
        {1010, nullptr, "GetAudioControlStates"},
        {1011, nullptr, "DeactivateAudioControl"},
        {1050, nullptr, "IsSixAxisSensorAccurateUserCalibrationSupported"},
        {1051, nullptr, "StartSixAxisSensorAccurateUserCalibration"},
        {1052, nullptr, "CancelSixAxisSensorAccurateUserCalibration"},
        {1053, nullptr, "GetSixAxisSensorAccurateUserCalibrationState"},
        {1100, nullptr, "GetHidbusSystemServiceObject"},
        {1120, &IHidSystemServer::SetFirmwareHotfixUpdateSkipEnabled, "SetFirmwareHotfixUpdateSkipEnabled"},
        {1130, &IHidSystemServer::InitializeUsbFirmwareUpdate, "InitializeUsbFirmwareUpdate"},
        {1131, &IHidSystemServer::FinalizeUsbFirmwareUpdate, "FinalizeUsbFirmwareUpdate"},
        {1132, &IHidSystemServer::CheckUsbFirmwareUpdateRequired, "CheckUsbFirmwareUpdateRequired"},
        {1133, nullptr, "StartUsbFirmwareUpdate"},
        {1134, nullptr, "GetUsbFirmwareUpdateState"},
        {1135, &IHidSystemServer::InitializeUsbFirmwareUpdateWithoutMemory, "InitializeUsbFirmwareUpdateWithoutMemory"},
        {1150, &IHidSystemServer::SetTouchScreenMagnification, "SetTouchScreenMagnification"},
        {1151, &IHidSystemServer::GetTouchScreenFirmwareVersion, "GetTouchScreenFirmwareVersion"},
        {1152, &IHidSystemServer::SetTouchScreenDefaultConfiguration, "SetTouchScreenDefaultConfiguration"},
        {1153, &IHidSystemServer::GetTouchScreenDefaultConfiguration, "GetTouchScreenDefaultConfiguration"},
        {1154, nullptr, "IsFirmwareAvailableForNotification"},
        {1155, &IHidSystemServer::SetForceHandheldStyleVibration, "SetForceHandheldStyleVibration"},
        {1156, nullptr, "SendConnectionTriggerWithoutTimeoutEvent"},
        {1157, nullptr, "CancelConnectionTrigger"},
        {1200, nullptr, "IsButtonConfigSupported"},
        {1201, nullptr, "IsButtonConfigEmbeddedSupported"},
        {1202, nullptr, "DeleteButtonConfig"},
        {1203, nullptr, "DeleteButtonConfigEmbedded"},
        {1204, nullptr, "SetButtonConfigEnabled"},
        {1205, nullptr, "SetButtonConfigEmbeddedEnabled"},
        {1206, nullptr, "IsButtonConfigEnabled"},
        {1207, nullptr, "IsButtonConfigEmbeddedEnabled"},
        {1208, nullptr, "SetButtonConfigEmbedded"},
        {1209, nullptr, "SetButtonConfigFull"},
        {1210, nullptr, "SetButtonConfigLeft"},
        {1211, nullptr, "SetButtonConfigRight"},
        {1212, nullptr, "GetButtonConfigEmbedded"},
        {1213, nullptr, "GetButtonConfigFull"},
        {1214, nullptr, "GetButtonConfigLeft"},
        {1215, nullptr, "GetButtonConfigRight"},
        {1250, nullptr, "IsCustomButtonConfigSupported"},
        {1251, nullptr, "IsDefaultButtonConfigEmbedded"},
        {1252, nullptr, "IsDefaultButtonConfigFull"},
        {1253, nullptr, "IsDefaultButtonConfigLeft"},
        {1254, nullptr, "IsDefaultButtonConfigRight"},
        {1255, nullptr, "IsButtonConfigStorageEmbeddedEmpty"},
        {1256, nullptr, "IsButtonConfigStorageFullEmpty"},
        {1257, nullptr, "IsButtonConfigStorageLeftEmpty"},
        {1258, nullptr, "IsButtonConfigStorageRightEmpty"},
        {1259, nullptr, "GetButtonConfigStorageEmbeddedDeprecated"},
        {1260, nullptr, "GetButtonConfigStorageFullDeprecated"},
        {1261, nullptr, "GetButtonConfigStorageLeftDeprecated"},
        {1262, nullptr, "GetButtonConfigStorageRightDeprecated"},
        {1263, nullptr, "SetButtonConfigStorageEmbeddedDeprecated"},
        {1264, nullptr, "SetButtonConfigStorageFullDeprecated"},
        {1265, nullptr, "SetButtonConfigStorageLeftDeprecated"},
        {1266, nullptr, "SetButtonConfigStorageRightDeprecated"},
        {1267, nullptr, "DeleteButtonConfigStorageEmbedded"},
        {1268, nullptr, "DeleteButtonConfigStorageFull"},
        {1269, nullptr, "DeleteButtonConfigStorageLeft"},
        {1270, nullptr, "DeleteButtonConfigStorageRight"},
        {1271, &IHidSystemServer::IsUsingCustomButtonConfig, "IsUsingCustomButtonConfig"},
        {1272, &IHidSystemServer::IsAnyCustomButtonConfigEnabled, "IsAnyCustomButtonConfigEnabled"},
        {1273, nullptr, "SetAllCustomButtonConfigEnabled"},
        {1274, nullptr, "SetDefaultButtonConfig"},
        {1275, nullptr, "SetAllDefaultButtonConfig"},
        {1276, nullptr, "SetHidButtonConfigEmbedded"},
        {1277, nullptr, "SetHidButtonConfigFull"},
        {1278, nullptr, "SetHidButtonConfigLeft"},
        {1279, nullptr, "SetHidButtonConfigRight"},
        {1280, nullptr, "GetHidButtonConfigEmbedded"},
        {1281, nullptr, "GetHidButtonConfigFull"},
        {1282, nullptr, "GetHidButtonConfigLeft"},
        {1283, nullptr, "GetHidButtonConfigRight"},
        {1284, nullptr, "GetButtonConfigStorageEmbedded"},
        {1285, nullptr, "GetButtonConfigStorageFull"},
        {1286, nullptr, "GetButtonConfigStorageLeft"},
        {1287, nullptr, "GetButtonConfigStorageRight"},
        {1288, nullptr, "SetButtonConfigStorageEmbedded"},
        {1289, nullptr, "SetButtonConfigStorageFull"},
        {1290, nullptr, "DeleteButtonConfigStorageRight"},
        {1291, nullptr, "DeleteButtonConfigStorageRight"},
        {1320, nullptr, "WakeTouchScreenUp"}, // 17.0.0+
        {1321, nullptr, "PutTouchScreenToSleep"}, // 17.0.0+
        {12010, nullptr, "SetButtonConfigLeft"} // 17.0.0+
    };
    // clang-format on

    RegisterHandlers(functions);

    joy_detach_event = service_context.CreateEvent("IHidSystemServer::JoyDetachEvent");
    acquire_device_registered_event =
        service_context.CreateEvent("IHidSystemServer::AcquireDeviceRegisteredEvent");
    acquire_connection_trigger_timeout_event =
        service_context.CreateEvent("IHidSystemServer::AcquireConnectionTriggerTimeoutEvent");
    unique_pad_connection_event =
        service_context.CreateEvent("IHidSystemServer::AcquireUniquePadConnectionEventHandle");
}

IHidSystemServer::~IHidSystemServer() {
    service_context.CloseEvent(joy_detach_event);
    service_context.CloseEvent(acquire_device_registered_event);
    service_context.CloseEvent(acquire_connection_trigger_timeout_event);
    service_context.CloseEvent(unique_pad_connection_event);
};

void IHidSystemServer::GetPlatformConfig(HLERequestContext& ctx) {
    const auto platform_config = firmware_settings->GetPlatformConfig();

    LOG_INFO(Service_HID, "called, platform_config={}", platform_config.raw);

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.PushRaw(platform_config);
}

void IHidSystemServer::ApplyNpadSystemCommonPolicy(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto applet_resource_user_id{rp.Pop<u64>()};

    LOG_INFO(Service_HID, "called, applet_resource_user_id={}", applet_resource_user_id);

    GetResourceManager()->GetNpad()->ApplyNpadSystemCommonPolicy(applet_resource_user_id);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void IHidSystemServer::EnableAssigningSingleOnSlSrPress(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto applet_resource_user_id{rp.Pop<u64>()};

    LOG_INFO(Service_HID, "called, applet_resource_user_id={}", applet_resource_user_id);

    GetResourceManager()->GetNpad()->AssigningSingleOnSlSrPress(applet_resource_user_id, true);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void IHidSystemServer::DisableAssigningSingleOnSlSrPress(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto applet_resource_user_id{rp.Pop<u64>()};

    LOG_INFO(Service_HID, "called, applet_resource_user_id={}", applet_resource_user_id);

    GetResourceManager()->GetNpad()->AssigningSingleOnSlSrPress(applet_resource_user_id, false);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void IHidSystemServer::GetLastActiveNpad(HLERequestContext& ctx) {
    Core::HID::NpadIdType npad_id{};
    const Result result = GetResourceManager()->GetNpad()->GetLastActiveNpad(npad_id);

    LOG_DEBUG(Service_HID, "called, npad_id={}", npad_id);

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(result);
    rb.PushEnum(npad_id);
}

void IHidSystemServer::ApplyNpadSystemCommonPolicyFull(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto applet_resource_user_id{rp.Pop<u64>()};

    LOG_INFO(Service_HID, "called, applet_resource_user_id={}", applet_resource_user_id);

    GetResourceManager()->GetNpad()->ApplyNpadSystemCommonPolicyFull(applet_resource_user_id);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void IHidSystemServer::GetNpadFullKeyGripColor(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto npad_id_type{rp.PopEnum<Core::HID::NpadIdType>()};

    LOG_DEBUG(Service_HID, "(STUBBED) called, npad_id_type={}",
              npad_id_type); // Spams a lot when controller applet is running

    Core::HID::NpadColor left_color{};
    Core::HID::NpadColor right_color{};
    // TODO: Get colors from Npad

    IPC::ResponseBuilder rb{ctx, 4};
    rb.Push(ResultSuccess);
    rb.PushRaw(left_color);
    rb.PushRaw(right_color);
}

void IHidSystemServer::GetMaskedSupportedNpadStyleSet(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto applet_resource_user_id{rp.Pop<u64>()};

    LOG_INFO(Service_HID, "called, applet_resource_user_id={}", applet_resource_user_id);

    Core::HID::NpadStyleSet supported_styleset{};
    const auto& npad = GetResourceManager()->GetNpad();
    const Result result =
        npad->GetMaskedSupportedNpadStyleSet(applet_resource_user_id, supported_styleset);

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(result);
    rb.PushEnum(supported_styleset);
}

void IHidSystemServer::SetSupportedNpadStyleSetAll(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto applet_resource_user_id{rp.Pop<u64>()};

    LOG_DEBUG(Service_HID, "called, applet_resource_user_id={}", applet_resource_user_id);

    const auto& npad = GetResourceManager()->GetNpad();
    const auto result =
        npad->SetSupportedNpadStyleSet(applet_resource_user_id, Core::HID::NpadStyleSet::All);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(result);
}

void IHidSystemServer::GetNpadCaptureButtonAssignment(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto applet_resource_user_id{rp.Pop<u64>()};
    const auto capture_button_list_size{ctx.GetWriteBufferNumElements<Core::HID::NpadButton>()};

    LOG_DEBUG(Service_HID, "called, applet_resource_user_id={}", applet_resource_user_id);

    std::vector<Core::HID::NpadButton> capture_button_list(capture_button_list_size);
    const auto& npad = GetResourceManager()->GetNpad();
    const u64 list_size =
        npad->GetNpadCaptureButtonAssignment(capture_button_list, applet_resource_user_id);

    if (list_size != 0) {
        ctx.WriteBuffer(capture_button_list);
    }

    IPC::ResponseBuilder rb{ctx, 4};
    rb.Push(ResultSuccess);
    rb.Push(list_size);
}

void IHidSystemServer::GetAppletDetailedUiType(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto npad_id_type{rp.PopEnum<Core::HID::NpadIdType>()};

    LOG_DEBUG(Service_HID, "called, npad_id_type={}",
              npad_id_type); // Spams a lot when controller applet is running

    const AppletDetailedUiType detailed_ui_type =
        GetResourceManager()->GetNpad()->GetAppletDetailedUiType(npad_id_type);

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.PushRaw(detailed_ui_type);
}

void IHidSystemServer::GetNpadInterfaceType(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto npad_id_type{rp.PopEnum<Core::HID::NpadIdType>()};

    LOG_DEBUG(Service_HID, "(STUBBED) called, npad_id_type={}",
              npad_id_type); // Spams a lot when controller applet is running

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.PushEnum(Core::HID::NpadInterfaceType::Bluetooth);
}

void IHidSystemServer::GetNpadLeftRightInterfaceType(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto npad_id_type{rp.PopEnum<Core::HID::NpadIdType>()};

    LOG_DEBUG(Service_HID, "(STUBBED) called, npad_id_type={}",
              npad_id_type); // Spams a lot when controller applet is running

    IPC::ResponseBuilder rb{ctx, 4};
    rb.Push(ResultSuccess);
    rb.PushEnum(Core::HID::NpadInterfaceType::Bluetooth);
    rb.PushEnum(Core::HID::NpadInterfaceType::Bluetooth);
}

void IHidSystemServer::HasBattery(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto npad_id_type{rp.PopEnum<Core::HID::NpadIdType>()};

    LOG_DEBUG(Service_HID, "(STUBBED) called, npad_id_type={}",
              npad_id_type); // Spams a lot when controller applet is running

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.Push(false);
}

void IHidSystemServer::HasLeftRightBattery(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto npad_id_type{rp.PopEnum<Core::HID::NpadIdType>()};

    LOG_DEBUG(Service_HID, "(STUBBED) called, npad_id_type={}",
              npad_id_type); // Spams a lot when controller applet is running

    struct LeftRightBattery {
        bool left;
        bool right;
    };

    LeftRightBattery left_right_battery{
        .left = false,
        .right = false,
    };

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.PushRaw(left_right_battery);
}

void IHidSystemServer::GetUniquePadsFromNpad(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto npad_id_type{rp.PopEnum<Core::HID::NpadIdType>()};

    LOG_DEBUG(Service_HID, "(STUBBED) called, npad_id_type={}",
              npad_id_type); // Spams a lot when controller applet is running

    const std::vector<Core::HID::UniquePadId> unique_pads{};

    if (!unique_pads.empty()) {
        ctx.WriteBuffer(unique_pads);
    }

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.Push(static_cast<u32>(unique_pads.size()));
}

void IHidSystemServer::SetNpadSystemExtStateEnabled(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    struct Parameters {
        bool is_enabled;
        INSERT_PADDING_BYTES_NOINIT(7);
        u64 applet_resource_user_id;
    };
    static_assert(sizeof(Parameters) == 0x10, "Parameters has incorrect size.");

    const auto parameters{rp.PopRaw<Parameters>()};

    LOG_INFO(Service_HID, "called, is_enabled={}, applet_resource_user_id={}",
             parameters.is_enabled, parameters.applet_resource_user_id);

    const auto result = GetResourceManager()->GetNpad()->SetNpadSystemExtStateEnabled(
        parameters.applet_resource_user_id, parameters.is_enabled);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(result);
}
void IHidSystemServer::RegisterAppletResourceUserId(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    struct Parameters {
        bool enable_input;
        INSERT_PADDING_WORDS_NOINIT(1);
        u64 applet_resource_user_id;
    };
    static_assert(sizeof(Parameters) == 0x10, "Parameters has incorrect size.");

    const auto parameters{rp.PopRaw<Parameters>()};

    LOG_INFO(Service_HID, "called, enable_input={}, applet_resource_user_id={}",
             parameters.enable_input, parameters.applet_resource_user_id);

    Result result = GetResourceManager()->RegisterAppletResourceUserId(
        parameters.applet_resource_user_id, parameters.enable_input);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(result);
}

void IHidSystemServer::UnregisterAppletResourceUserId(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    u64 applet_resource_user_id{rp.Pop<u64>()};

    LOG_INFO(Service_HID, "called, applet_resource_user_id={}", applet_resource_user_id);

    GetResourceManager()->UnregisterAppletResourceUserId(applet_resource_user_id);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void IHidSystemServer::EnableAppletToGetInput(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    struct Parameters {
        bool is_enabled;
        INSERT_PADDING_WORDS_NOINIT(1);
        u64 applet_resource_user_id;
    };
    static_assert(sizeof(Parameters) == 0x10, "Parameters has incorrect size.");

    const auto parameters{rp.PopRaw<Parameters>()};

    LOG_INFO(Service_HID, "called, is_enabled={}, applet_resource_user_id={}",
             parameters.is_enabled, parameters.applet_resource_user_id);

    GetResourceManager()->EnableInput(parameters.applet_resource_user_id, parameters.is_enabled);
    GetResourceManager()->GetNpad()->EnableAppletToGetInput(parameters.applet_resource_user_id);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void IHidSystemServer::SetAruidValidForVibration(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    struct Parameters {
        bool is_enabled;
        INSERT_PADDING_WORDS_NOINIT(1);
        u64 applet_resource_user_id;
    };
    static_assert(sizeof(Parameters) == 0x10, "Parameters has incorrect size.");

    const auto parameters{rp.PopRaw<Parameters>()};

    LOG_INFO(Service_HID, "called, is_enabled={}, applet_resource_user_id={}",
             parameters.is_enabled, parameters.applet_resource_user_id);

    GetResourceManager()->SetAruidValidForVibration(parameters.applet_resource_user_id,
                                                    parameters.is_enabled);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void IHidSystemServer::EnableAppletToGetSixAxisSensor(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    struct Parameters {
        bool is_enabled;
        INSERT_PADDING_WORDS_NOINIT(1);
        u64 applet_resource_user_id;
    };
    static_assert(sizeof(Parameters) == 0x10, "Parameters has incorrect size.");

    const auto parameters{rp.PopRaw<Parameters>()};

    LOG_INFO(Service_HID, "called, is_enabled={}, applet_resource_user_id={}",
             parameters.is_enabled, parameters.applet_resource_user_id);

    GetResourceManager()->EnableTouchScreen(parameters.applet_resource_user_id,
                                            parameters.is_enabled);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void IHidSystemServer::EnableAppletToGetPadInput(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    struct Parameters {
        bool is_enabled;
        INSERT_PADDING_WORDS_NOINIT(1);
        u64 applet_resource_user_id;
    };
    static_assert(sizeof(Parameters) == 0x10, "Parameters has incorrect size.");

    const auto parameters{rp.PopRaw<Parameters>()};

    LOG_INFO(Service_HID, "called, is_enabled={}, applet_resource_user_id={}",
             parameters.is_enabled, parameters.applet_resource_user_id);

    GetResourceManager()->EnablePadInput(parameters.applet_resource_user_id, parameters.is_enabled);
    GetResourceManager()->GetNpad()->EnableAppletToGetInput(parameters.applet_resource_user_id);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void IHidSystemServer::EnableAppletToGetTouchScreen(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    struct Parameters {
        bool is_enabled;
        INSERT_PADDING_WORDS_NOINIT(1);
        u64 applet_resource_user_id;
    };
    static_assert(sizeof(Parameters) == 0x10, "Parameters has incorrect size.");

    const auto parameters{rp.PopRaw<Parameters>()};

    LOG_INFO(Service_HID, "called, is_enabled={}, applet_resource_user_id={}",
             parameters.is_enabled, parameters.applet_resource_user_id);

    GetResourceManager()->EnableTouchScreen(parameters.applet_resource_user_id,
                                            parameters.is_enabled);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void IHidSystemServer::SetVibrationMasterVolume(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto master_volume{rp.Pop<f32>()};

    LOG_INFO(Service_HID, "called, volume={}", master_volume);

    const auto result =
        GetResourceManager()->GetNpad()->GetVibrationHandler()->SetVibrationMasterVolume(
            master_volume);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(result);
}

void IHidSystemServer::GetVibrationMasterVolume(HLERequestContext& ctx) {
    f32 master_volume{};
    const auto result =
        GetResourceManager()->GetNpad()->GetVibrationHandler()->GetVibrationMasterVolume(
            master_volume);

    LOG_INFO(Service_HID, "called, volume={}", master_volume);

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(result);
    rb.Push(master_volume);
}

void IHidSystemServer::BeginPermitVibrationSession(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto applet_resource_user_id{rp.Pop<u64>()};

    LOG_INFO(Service_HID, "called, applet_resource_user_id={}", applet_resource_user_id);

    const auto result =
        GetResourceManager()->GetNpad()->GetVibrationHandler()->BeginPermitVibrationSession(
            applet_resource_user_id);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(result);
}

void IHidSystemServer::EndPermitVibrationSession(HLERequestContext& ctx) {
    LOG_INFO(Service_HID, "called");

    const auto result =
        GetResourceManager()->GetNpad()->GetVibrationHandler()->EndPermitVibrationSession();

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(result);
}

void IHidSystemServer::IsJoyConRailEnabled(HLERequestContext& ctx) {
    const bool is_attached = true;

    LOG_WARNING(Service_HID, "(STUBBED) called, is_attached={}", is_attached);

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.Push(is_attached);
}

void IHidSystemServer::IsJoyConAttachedOnAllRail(HLERequestContext& ctx) {
    const bool is_attached = true;

    LOG_DEBUG(Service_HID, "(STUBBED) called, is_attached={}", is_attached);

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.Push(is_attached);
}

void IHidSystemServer::AcquireConnectionTriggerTimeoutEvent(HLERequestContext& ctx) {
    LOG_INFO(Service_AM, "(STUBBED) called");

    IPC::ResponseBuilder rb{ctx, 2, 1};
    rb.Push(ResultSuccess);
    rb.PushCopyObjects(acquire_device_registered_event->GetReadableEvent());
}

void IHidSystemServer::AcquireDeviceRegisteredEventForControllerSupport(HLERequestContext& ctx) {
    LOG_INFO(Service_HID, "(STUBBED) called");

    IPC::ResponseBuilder rb{ctx, 2, 1};
    rb.Push(ResultSuccess);
    rb.PushCopyObjects(acquire_device_registered_event->GetReadableEvent());
}

void IHidSystemServer::GetRegisteredDevices(HLERequestContext& ctx) {
    LOG_WARNING(Service_HID, "(STUBBED) called");

    struct RegisterData {
        std::array<u8, 0x68> data;
    };
    static_assert(sizeof(RegisterData) == 0x68, "RegisterData is an invalid size");
    std::vector<RegisterData> registered_devices{};

    if (!registered_devices.empty()) {
        ctx.WriteBuffer(registered_devices);
    }

    IPC::ResponseBuilder rb{ctx, 4};
    rb.Push(ResultSuccess);
    rb.Push<u64>(registered_devices.size());
}

void IHidSystemServer::AcquireUniquePadConnectionEventHandle(HLERequestContext& ctx) {
    LOG_WARNING(Service_HID, "(STUBBED) called");

    IPC::ResponseBuilder rb{ctx, 2, 1};
    rb.PushCopyObjects(unique_pad_connection_event->GetReadableEvent());
    rb.Push(ResultSuccess);
}

void IHidSystemServer::GetUniquePadIds(HLERequestContext& ctx) {
    LOG_DEBUG(Service_HID, "(STUBBED) called");

    IPC::ResponseBuilder rb{ctx, 4};
    rb.Push(ResultSuccess);
    rb.Push<u64>(0);
}

void IHidSystemServer::AcquireJoyDetachOnBluetoothOffEventHandle(HLERequestContext& ctx) {
    LOG_INFO(Service_AM, "called");

    IPC::ResponseBuilder rb{ctx, 2, 1};
    rb.Push(ResultSuccess);
    rb.PushCopyObjects(joy_detach_event->GetReadableEvent());
}

void IHidSystemServer::IsUsbFullKeyControllerEnabled(HLERequestContext& ctx) {
    const bool is_enabled = false;

    LOG_WARNING(Service_HID, "(STUBBED) called, is_enabled={}", is_enabled);

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.Push(is_enabled);
}

void IHidSystemServer::EnableUsbFullKeyController(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto is_enabled{rp.Pop<bool>()};

    LOG_WARNING(Service_HID, "(STUBBED) called, is_enabled={}", is_enabled);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void IHidSystemServer::IsHandheldButtonPressedOnConsoleMode(HLERequestContext& ctx) {
    const bool button_pressed = false;

    LOG_DEBUG(Service_HID, "(STUBBED) called, is_enabled={}",
              button_pressed); // Spams a lot when controller applet is open

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.Push(button_pressed);
}

void IHidSystemServer::InitializeFirmwareUpdate(HLERequestContext& ctx) {
    LOG_WARNING(Service_HID, "(STUBBED) called");

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void IHidSystemServer::CheckFirmwareUpdateRequired(HLERequestContext& ctx) {
    LOG_WARNING(Service_HID, "(STUBBED) called");

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void IHidSystemServer::SetFirmwareHotfixUpdateSkipEnabled(HLERequestContext& ctx) {
    LOG_WARNING(Service_HID, "(STUBBED) called");

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void IHidSystemServer::InitializeUsbFirmwareUpdate(HLERequestContext& ctx) {
    LOG_WARNING(Service_HID, "(STUBBED) called");

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void IHidSystemServer::FinalizeUsbFirmwareUpdate(HLERequestContext& ctx) {
    LOG_WARNING(Service_HID, "(STUBBED) called");

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void IHidSystemServer::CheckUsbFirmwareUpdateRequired(HLERequestContext& ctx) {
    LOG_WARNING(Service_HID, "(STUBBED) called");

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void IHidSystemServer::InitializeUsbFirmwareUpdateWithoutMemory(HLERequestContext& ctx) {
    LOG_WARNING(Service_HID, "(STUBBED) called");

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void IHidSystemServer::SetTouchScreenMagnification(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto point1x{rp.Pop<f32>()};
    const auto point1y{rp.Pop<f32>()};
    const auto point2x{rp.Pop<f32>()};
    const auto point2y{rp.Pop<f32>()};

    LOG_INFO(Service_HID, "called, point1=-({},{}), point2=({},{})", point1x, point1y, point2x,
             point2y);

    const Result result = GetResourceManager()->GetTouchScreen()->SetTouchScreenMagnification(
        point1x, point1y, point2x, point2y);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(result);
}

void IHidSystemServer::GetTouchScreenFirmwareVersion(HLERequestContext& ctx) {
    LOG_INFO(Service_HID, "called");

    Core::HID::FirmwareVersion firmware{};
    const auto result = GetResourceManager()->GetTouchScreenFirmwareVersion(firmware);

    IPC::ResponseBuilder rb{ctx, 6};
    rb.Push(result);
    rb.PushRaw(firmware);
}

void IHidSystemServer::SetTouchScreenDefaultConfiguration(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    auto touchscreen_config{rp.PopRaw<Core::HID::TouchScreenConfigurationForNx>()};

    LOG_INFO(Service_HID, "called, touchscreen_config={}", touchscreen_config.mode);

    if (touchscreen_config.mode != Core::HID::TouchScreenModeForNx::Heat2 &&
        touchscreen_config.mode != Core::HID::TouchScreenModeForNx::Finger) {
        touchscreen_config.mode = Core::HID::TouchScreenModeForNx::UseSystemSetting;
    }

    const Result result =
        GetResourceManager()->GetTouchScreen()->SetTouchScreenDefaultConfiguration(
            touchscreen_config);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(result);
}

void IHidSystemServer::GetTouchScreenDefaultConfiguration(HLERequestContext& ctx) {
    LOG_INFO(Service_HID, "called");

    Core::HID::TouchScreenConfigurationForNx touchscreen_config{};
    const Result result =
        GetResourceManager()->GetTouchScreen()->GetTouchScreenDefaultConfiguration(
            touchscreen_config);

    if (touchscreen_config.mode != Core::HID::TouchScreenModeForNx::Heat2 &&
        touchscreen_config.mode != Core::HID::TouchScreenModeForNx::Finger) {
        touchscreen_config.mode = Core::HID::TouchScreenModeForNx::UseSystemSetting;
    }

    IPC::ResponseBuilder rb{ctx, 6};
    rb.Push(result);
    rb.PushRaw(touchscreen_config);
}

void IHidSystemServer::SetForceHandheldStyleVibration(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto is_forced{rp.Pop<bool>()};

    LOG_INFO(Service_HID, "called, is_forced={}", is_forced);

    GetResourceManager()->SetForceHandheldStyleVibration(is_forced);
    GetResourceManager()->GetNpad()->UpdateHandheldAbstractState();

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void IHidSystemServer::IsUsingCustomButtonConfig(HLERequestContext& ctx) {
    const bool is_enabled = false;

    LOG_DEBUG(Service_HID, "(STUBBED) called, is_enabled={}", is_enabled);

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.Push(is_enabled);
}

void IHidSystemServer::IsAnyCustomButtonConfigEnabled(HLERequestContext& ctx) {
    const bool is_enabled = false;

    LOG_DEBUG(Service_HID, "(STUBBED) called, is_enabled={}", is_enabled);

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.Push(is_enabled);
}

std::shared_ptr<ResourceManager> IHidSystemServer::GetResourceManager() {
    resource_manager->Initialize();
    return resource_manager;
}

} // namespace Service::HID
