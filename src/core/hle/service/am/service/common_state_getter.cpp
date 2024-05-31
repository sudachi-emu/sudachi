// SPDX-FileCopyrightText: Copyright 2024 sudachi Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/settings.h"
#include "core/hle/service/am/am_results.h"
#include "core/hle/service/am/applet.h"
#include "core/hle/service/am/service/common_state_getter.h"
#include "core/hle/service/am/service/lock_accessor.h"
#include "core/hle/service/apm/apm_interface.h"
#include "core/hle/service/cmif_serialization.h"
#include "core/hle/service/pm/pm.h"
#include "core/hle/service/sm/sm.h"
#include "core/hle/service/vi/vi.h"
#include "core/hle/service/vi/vi_types.h"

namespace Service::AM {

ICommonStateGetter::ICommonStateGetter(Core::System& system_, std::shared_ptr<Applet> applet)
    : ServiceFramework{system_, "ICommonStateGetter"}, m_applet{std::move(applet)} {
    // clang-format off
    static const FunctionInfo functions[] = {
        {0, D<&ICommonStateGetter::GetEventHandle>, "GetEventHandle"},
        {1, D<&ICommonStateGetter::ReceiveMessage>, "ReceiveMessage"},
        {2, nullptr, "GetThisAppletKind"},
        {3, nullptr, "AllowToEnterSleep"},
        {4, nullptr, "DisallowToEnterSleep"},
        {5, D<&ICommonStateGetter::GetOperationMode>, "GetOperationMode"},
        {6, D<&ICommonStateGetter::GetPerformanceMode>, "GetPerformanceMode"},
        {7, nullptr, "GetCradleStatus"},
        {8, D<&ICommonStateGetter::GetBootMode>, "GetBootMode"},
        {9, D<&ICommonStateGetter::GetCurrentFocusState>, "GetCurrentFocusState"},
        {10, D<&ICommonStateGetter::RequestToAcquireSleepLock>, "RequestToAcquireSleepLock"},
        {11, nullptr, "ReleaseSleepLock"},
        {12, nullptr, "ReleaseSleepLockTransiently"},
        {13, D<&ICommonStateGetter::GetAcquiredSleepLockEvent>, "GetAcquiredSleepLockEvent"},
        {14, nullptr, "GetWakeupCount"},
        {20, nullptr, "PushToGeneralChannel"},
        {30, nullptr, "GetHomeButtonReaderLockAccessor"},
        {31, D<&ICommonStateGetter::GetReaderLockAccessorEx>, "GetReaderLockAccessorEx"},
        {32, D<&ICommonStateGetter::GetWriterLockAccessorEx>, "GetWriterLockAccessorEx"},
        {40, D<&ICommonStateGetter::GetCradleFwVersion>, "GetCradleFwVersion"},
        {50, D<&ICommonStateGetter::IsVrModeEnabled>, "IsVrModeEnabled"},
        {51, D<&ICommonStateGetter::SetVrModeEnabled>, "SetVrModeEnabled"},
        {52, D<&ICommonStateGetter::SetLcdBacklighOffEnabled>, "SetLcdBacklighOffEnabled"},
        {53, D<&ICommonStateGetter::BeginVrModeEx>, "BeginVrModeEx"},
        {54, D<&ICommonStateGetter::EndVrModeEx>, "EndVrModeEx"},
        {55, D<&ICommonStateGetter::IsInControllerFirmwareUpdateSection>, "IsInControllerFirmwareUpdateSection"},
        {59, nullptr, "SetVrPositionForDebug"},
        {60, D<&ICommonStateGetter::GetDefaultDisplayResolution>, "GetDefaultDisplayResolution"},
        {61, D<&ICommonStateGetter::GetDefaultDisplayResolutionChangeEvent>, "GetDefaultDisplayResolutionChangeEvent"},
        {62, D<&ICommonStateGetter::GetHdcpAuthenticationState>, "GetHdcpAuthenticationState"},
        {63, D<&ICommonStateGetter::GetHdcpAuthenticationStateChangeEvent>, "GetHdcpAuthenticationStateChangeEvent"},
        {64, nullptr, "SetTvPowerStateMatchingMode"},
        {65, nullptr, "GetApplicationIdByContentActionName"},
        {66, &ICommonStateGetter::SetCpuBoostMode, "SetCpuBoostMode"},
        {67, nullptr, "CancelCpuBoostMode"},
        {68, D<&ICommonStateGetter::GetBuiltInDisplayType>, "GetBuiltInDisplayType"},
        {80, D<&ICommonStateGetter::PerformSystemButtonPressingIfInFocus>, "PerformSystemButtonPressingIfInFocus"},
        {90, nullptr, "SetPerformanceConfigurationChangedNotification"},
        {91, nullptr, "GetCurrentPerformanceConfiguration"},
        {100, nullptr, "SetHandlingHomeButtonShortPressedEnabled"},
        {110, nullptr, "OpenMyGpuErrorHandler"},
        {120, D<&ICommonStateGetter::GetAppletLaunchedHistory>, "GetAppletLaunchedHistory"},
        {200, D<&ICommonStateGetter::GetOperationModeSystemInfo>, "GetOperationModeSystemInfo"},
        {300, D<&ICommonStateGetter::GetSettingsPlatformRegion>, "GetSettingsPlatformRegion"},
        {400, nullptr, "ActivateMigrationService"},
        {401, nullptr, "DeactivateMigrationService"},
        {500, nullptr, "DisableSleepTillShutdown"},
        {501, nullptr, "SuppressDisablingSleepTemporarily"},
        {502, nullptr, "IsSleepEnabled"},
        {503, nullptr, "IsDisablingSleepSuppressed"},
        {600, nullptr, "OpenNamedChannelAsChild"}, // 17.0.0+
        {900, D<&ICommonStateGetter::SetRequestExitToLibraryAppletAtExecuteNextProgramEnabled>, "SetRequestExitToLibraryAppletAtExecuteNextProgramEnabled"},
        {910, nullptr, "GetLaunchRequiredTick"}, // 17.0.0+
    };
    // clang-format on

    RegisterHandlers(functions);
}

ICommonStateGetter::~ICommonStateGetter() = default;

Result ICommonStateGetter::GetEventHandle(OutCopyHandle<Kernel::KReadableEvent> out_event) {
    LOG_DEBUG(Service_AM, "called");
    *out_event = m_applet->lifecycle_manager.GetSystemEvent().GetHandle();
    R_SUCCEED();
}

Result ICommonStateGetter::ReceiveMessage(Out<AppletMessage> out_applet_message) {
    LOG_DEBUG(Service_AM, "called");

    if (!m_applet->lifecycle_manager.PopMessage(out_applet_message)) {
        LOG_ERROR(Service_AM, "Tried to pop message but none was available!");
        R_THROW(AM::ResultNoMessages);
    }

    R_SUCCEED();
}

Result ICommonStateGetter::GetCurrentFocusState(Out<FocusState> out_focus_state) {
    LOG_DEBUG(Service_AM, "called");

    std::scoped_lock lk{m_applet->lock};
    *out_focus_state = m_applet->lifecycle_manager.GetAndClearFocusState();

    R_SUCCEED();
}

Result ICommonStateGetter::RequestToAcquireSleepLock() {
    LOG_WARNING(Service_AM, "(STUBBED) called");

    // Sleep lock is acquired immediately.
    m_applet->sleep_lock_event.Signal();
    R_SUCCEED();
}

Result ICommonStateGetter::GetAcquiredSleepLockEvent(
    OutCopyHandle<Kernel::KReadableEvent> out_event) {
    LOG_WARNING(Service_AM, "called");
    *out_event = m_applet->sleep_lock_event.GetHandle();
    R_SUCCEED();
}

Result ICommonStateGetter::GetReaderLockAccessorEx(
    Out<SharedPointer<ILockAccessor>> out_lock_accessor, u32 button_type) {
    LOG_INFO(Service_AM, "called, button_type={}", button_type);
    *out_lock_accessor = std::make_shared<ILockAccessor>(system);
    R_SUCCEED();
}

Result ICommonStateGetter::GetWriterLockAccessorEx(
    Out<SharedPointer<ILockAccessor>> out_lock_accessor, u32 button_type) {
    LOG_INFO(Service_AM, "called, button_type={}", button_type);
    *out_lock_accessor = std::make_shared<ILockAccessor>(system);
    R_SUCCEED();
}

Result ICommonStateGetter::GetDefaultDisplayResolutionChangeEvent(
    OutCopyHandle<Kernel::KReadableEvent> out_event) {
    LOG_DEBUG(Service_AM, "called");
    *out_event = m_applet->lifecycle_manager.GetOperationModeChangedSystemEvent().GetHandle();
    R_SUCCEED();
}

Result ICommonStateGetter::GetHdcpAuthenticationState(Out<s32> out_state) {
    LOG_DEBUG(Service_AM, "called");
    *out_state = 1;
    R_SUCCEED();
}

Result ICommonStateGetter::GetHdcpAuthenticationStateChangeEvent(
    OutCopyHandle<Kernel::KReadableEvent> out_event) {
    LOG_DEBUG(Service_AM, "called");
    *out_event = m_applet->lifecycle_manager.GetHDCPStateChangedEvent().GetHandle();
    R_SUCCEED();
}

Result ICommonStateGetter::GetOperationMode(Out<OperationMode> out_operation_mode) {
    const bool use_docked_mode{Settings::IsDockedMode()};
    LOG_DEBUG(Service_AM, "called, use_docked_mode={}", use_docked_mode);
    *out_operation_mode = use_docked_mode ? OperationMode::Docked : OperationMode::Handheld;
    R_SUCCEED();
}

Result ICommonStateGetter::GetPerformanceMode(Out<APM::PerformanceMode> out_performance_mode) {
    LOG_DEBUG(Service_AM, "called");
    *out_performance_mode = system.GetAPMController().GetCurrentPerformanceMode();
    R_SUCCEED();
}

Result ICommonStateGetter::GetBootMode(Out<PM::SystemBootMode> out_boot_mode) {
    LOG_DEBUG(Service_AM, "called");
    *out_boot_mode = Service::PM::SystemBootMode::Normal;
    R_SUCCEED();
}

Result ICommonStateGetter::GetCradleFwVersion() {
    LOG_WARNING(Service_AM, "(STUBBED) called");
    R_SUCCEED();
}

Result ICommonStateGetter::IsVrModeEnabled(Out<bool> out_is_vr_mode_enabled) {
    LOG_DEBUG(Service_AM, "called");

    std::scoped_lock lk{m_applet->lock};
    *out_is_vr_mode_enabled = m_applet->vr_mode_enabled;
    R_SUCCEED();
}

Result ICommonStateGetter::SetVrModeEnabled(bool is_vr_mode_enabled) {
    std::scoped_lock lk{m_applet->lock};
    m_applet->vr_mode_enabled = is_vr_mode_enabled;
    LOG_WARNING(Service_AM, "VR Mode is {}", m_applet->vr_mode_enabled ? "on" : "off");
    R_SUCCEED();
}

Result ICommonStateGetter::SetLcdBacklighOffEnabled(bool is_lcd_backlight_off_enabled) {
    LOG_WARNING(Service_AM, "(STUBBED) called. is_lcd_backlight_off_enabled={}",
                is_lcd_backlight_off_enabled);
    R_SUCCEED();
}

Result ICommonStateGetter::BeginVrModeEx() {
    LOG_WARNING(Service_AM, "(STUBBED) called");
    std::scoped_lock lk{m_applet->lock};
    m_applet->vr_mode_enabled = true;
    R_SUCCEED();
}

Result ICommonStateGetter::EndVrModeEx() {
    LOG_WARNING(Service_AM, "(STUBBED) called");
    std::scoped_lock lk{m_applet->lock};
    m_applet->vr_mode_enabled = false;
    R_SUCCEED();
}

Result ICommonStateGetter::IsInControllerFirmwareUpdateSection(
    Out<bool> out_is_in_controller_firmware_update_section) {
    LOG_INFO(Service_AM, "called");
    *out_is_in_controller_firmware_update_section = false;
    R_SUCCEED();
}

Result ICommonStateGetter::GetDefaultDisplayResolution(Out<s32> out_width, Out<s32> out_height) {
    LOG_DEBUG(Service_AM, "called");

    if (Settings::IsDockedMode()) {
        *out_width = static_cast<u32>(Service::VI::DisplayResolution::DockedWidth);
        *out_height = static_cast<u32>(Service::VI::DisplayResolution::DockedHeight);
    } else {
        *out_width = static_cast<u32>(Service::VI::DisplayResolution::UndockedWidth);
        *out_height = static_cast<u32>(Service::VI::DisplayResolution::UndockedHeight);
    }

    R_SUCCEED();
}

void ICommonStateGetter::SetCpuBoostMode(HLERequestContext& ctx) {
    LOG_DEBUG(Service_AM, "called, forwarding to APM:SYS");

    const auto& sm = system.ServiceManager();
    const auto apm_sys = sm.GetService<APM::APM_Sys>("apm:sys");
    ASSERT(apm_sys != nullptr);

    apm_sys->SetCpuBoostMode(ctx);
}

Result ICommonStateGetter::GetBuiltInDisplayType(Out<s32> out_display_type) {
    LOG_WARNING(Service_AM, "(STUBBED) called");
    *out_display_type = 0;
    R_SUCCEED();
}

Result ICommonStateGetter::PerformSystemButtonPressingIfInFocus(SystemButtonType type) {
    LOG_WARNING(Service_AM, "(STUBBED) called, type={}", type);
    R_SUCCEED();
}

Result ICommonStateGetter::GetOperationModeSystemInfo(Out<u32> out_operation_mode_system_info) {
    LOG_WARNING(Service_AM, "(STUBBED) called");
    *out_operation_mode_system_info = 0;
    R_SUCCEED();
}

Result ICommonStateGetter::GetAppletLaunchedHistory(
    Out<s32> out_count, OutArray<AppletId, BufferAttr_HipcMapAlias> out_applet_ids) {
    LOG_INFO(Service_AM, "called");

    std::shared_ptr<Applet> current_applet = m_applet;

    for (*out_count = 0;
         *out_count < static_cast<s32>(out_applet_ids.size()) && current_applet != nullptr;
         /* ... */) {
        out_applet_ids[(*out_count)++] = current_applet->applet_id;
        current_applet = current_applet->caller_applet.lock();
    }

    R_SUCCEED();
}

Result ICommonStateGetter::GetSettingsPlatformRegion(
    Out<Set::PlatformRegion> out_settings_platform_region) {
    LOG_INFO(Service_AM, "called");
    *out_settings_platform_region = Set::PlatformRegion::Global;
    R_SUCCEED();
}

Result ICommonStateGetter::SetRequestExitToLibraryAppletAtExecuteNextProgramEnabled() {
    LOG_WARNING(Service_AM, "(STUBBED) called");

    std::scoped_lock lk{m_applet->lock};
    m_applet->request_exit_to_library_applet_at_execute_next_program_enabled = true;

    R_SUCCEED();
}

} // namespace Service::AM
