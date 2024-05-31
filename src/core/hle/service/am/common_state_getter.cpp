// SPDX-FileCopyrightText: Copyright 2024 sudachi Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/settings.h"
#include "core/hle/service/am/am_results.h"
#include "core/hle/service/am/applet.h"
#include "core/hle/service/am/common_state_getter.h"
#include "core/hle/service/am/lock_accessor.h"
#include "core/hle/service/apm/apm_controller.h"
#include "core/hle/service/apm/apm_interface.h"
#include "core/hle/service/ipc_helpers.h"
#include "core/hle/service/pm/pm.h"
#include "core/hle/service/sm/sm.h"
#include "core/hle/service/vi/vi.h"

namespace Service::AM {

ICommonStateGetter::ICommonStateGetter(Core::System& system_, std::shared_ptr<Applet> applet_)
    : ServiceFramework{system_, "ICommonStateGetter"}, applet{std::move(applet_)} {
    // clang-format off
    static const FunctionInfo functions[] = {
        {0, &ICommonStateGetter::GetEventHandle, "GetEventHandle"},
        {1, &ICommonStateGetter::ReceiveMessage, "ReceiveMessage"},
        {2, nullptr, "GetThisAppletKind"},
        {3, nullptr, "AllowToEnterSleep"},
        {4, nullptr, "DisallowToEnterSleep"},
        {5, &ICommonStateGetter::GetOperationMode, "GetOperationMode"},
        {6, &ICommonStateGetter::GetPerformanceMode, "GetPerformanceMode"},
        {7, nullptr, "GetCradleStatus"},
        {8, &ICommonStateGetter::GetBootMode, "GetBootMode"},
        {9, &ICommonStateGetter::GetCurrentFocusState, "GetCurrentFocusState"},
        {10, &ICommonStateGetter::RequestToAcquireSleepLock, "RequestToAcquireSleepLock"},
        {11, nullptr, "ReleaseSleepLock"},
        {12, nullptr, "ReleaseSleepLockTransiently"},
        {13, &ICommonStateGetter::GetAcquiredSleepLockEvent, "GetAcquiredSleepLockEvent"},
        {14, nullptr, "GetWakeupCount"},
        {20, nullptr, "PushToGeneralChannel"},
        {30, nullptr, "GetHomeButtonReaderLockAccessor"},
        {31, &ICommonStateGetter::GetReaderLockAccessorEx, "GetReaderLockAccessorEx"},
        {32, nullptr, "GetWriterLockAccessorEx"},
        {40, &ICommonStateGetter::GetCradleFwVersion, "GetCradleFwVersion"},
        {50, &ICommonStateGetter::IsVrModeEnabled, "IsVrModeEnabled"},
        {51, &ICommonStateGetter::SetVrModeEnabled, "SetVrModeEnabled"},
        {52, &ICommonStateGetter::SetLcdBacklighOffEnabled, "SetLcdBacklighOffEnabled"},
        {53, &ICommonStateGetter::BeginVrModeEx, "BeginVrModeEx"},
        {54, &ICommonStateGetter::EndVrModeEx, "EndVrModeEx"},
        {55, nullptr, "IsInControllerFirmwareUpdateSection"},
        {59, nullptr, "SetVrPositionForDebug"},
        {60, &ICommonStateGetter::GetDefaultDisplayResolution, "GetDefaultDisplayResolution"},
        {61, &ICommonStateGetter::GetDefaultDisplayResolutionChangeEvent, "GetDefaultDisplayResolutionChangeEvent"},
        {62, &ICommonStateGetter::GetHdcpAuthenticationState, "GetHdcpAuthenticationState"},
        {63, &ICommonStateGetter::GetHdcpAuthenticationStateChangeEvent, "GetHdcpAuthenticationStateChangeEvent"},
        {64, nullptr, "SetTvPowerStateMatchingMode"},
        {65, nullptr, "GetApplicationIdByContentActionName"},
        {66, &ICommonStateGetter::SetCpuBoostMode, "SetCpuBoostMode"},
        {67, nullptr, "CancelCpuBoostMode"},
        {68, &ICommonStateGetter::GetBuiltInDisplayType, "GetBuiltInDisplayType"},
        {80, &ICommonStateGetter::PerformSystemButtonPressingIfInFocus, "PerformSystemButtonPressingIfInFocus"},
        {90, nullptr, "SetPerformanceConfigurationChangedNotification"},
        {91, nullptr, "GetCurrentPerformanceConfiguration"},
        {100, nullptr, "SetHandlingHomeButtonShortPressedEnabled"},
        {110, nullptr, "OpenMyGpuErrorHandler"},
        {120, &ICommonStateGetter::GetAppletLaunchedHistory, "GetAppletLaunchedHistory"},
        {200, nullptr, "GetOperationModeSystemInfo"},
        {300, &ICommonStateGetter::GetSettingsPlatformRegion, "GetSettingsPlatformRegion"},
        {400, nullptr, "ActivateMigrationService"},
        {401, nullptr, "DeactivateMigrationService"},
        {500, nullptr, "DisableSleepTillShutdown"},
        {501, nullptr, "SuppressDisablingSleepTemporarily"},
        {502, nullptr, "IsSleepEnabled"},
        {503, nullptr, "IsDisablingSleepSuppressed"},
        {600, nullptr, "OpenNamedChannelAsChild"}, // 17.0.0+
        {900, &ICommonStateGetter::SetRequestExitToLibraryAppletAtExecuteNextProgramEnabled, "SetRequestExitToLibraryAppletAtExecuteNextProgramEnabled"},
        {910, nullptr, "GetLaunchRequiredTick"}, // 17.0.0+
    };
    // clang-format on

    RegisterHandlers(functions);
}

ICommonStateGetter::~ICommonStateGetter() = default;

void ICommonStateGetter::GetBootMode(HLERequestContext& ctx) {
    LOG_DEBUG(Service_AM, "called");

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.Push<u8>(static_cast<u8>(Service::PM::SystemBootMode::Normal)); // Normal boot mode
}

void ICommonStateGetter::GetEventHandle(HLERequestContext& ctx) {
    LOG_DEBUG(Service_AM, "(STUBBED) called");

    IPC::ResponseBuilder rb{ctx, 2, 1};
    rb.Push(ResultSuccess);
    rb.PushCopyObjects(applet->message_queue.GetMessageReceiveEvent());
}

void ICommonStateGetter::ReceiveMessage(HLERequestContext& ctx) {
    LOG_DEBUG(Service_AM, "called");

    const auto message = applet->message_queue.PopMessage();
    IPC::ResponseBuilder rb{ctx, 3};

    if (message == AppletMessageQueue::AppletMessage::None) {
        LOG_ERROR(Service_AM, "Message queue is empty");
        rb.Push(AM::ResultNoMessages);
        rb.PushEnum<AppletMessageQueue::AppletMessage>(message);
        return;
    }

    rb.Push(ResultSuccess);
    rb.PushEnum<AppletMessageQueue::AppletMessage>(message);
}

void ICommonStateGetter::GetCurrentFocusState(HLERequestContext& ctx) {
    LOG_DEBUG(Service_AM, "(STUBBED) called");

    std::scoped_lock lk{applet->lock};

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.Push(static_cast<u8>(applet->focus_state));
}

void ICommonStateGetter::GetOperationMode(HLERequestContext& ctx) {
    const bool use_docked_mode{Settings::IsDockedMode()};
    LOG_DEBUG(Service_AM, "called, use_docked_mode={}", use_docked_mode);

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.Push(static_cast<u8>(use_docked_mode ? OperationMode::Docked : OperationMode::Handheld));
}

void ICommonStateGetter::GetPerformanceMode(HLERequestContext& ctx) {
    LOG_DEBUG(Service_AM, "called");

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.PushEnum(system.GetAPMController().GetCurrentPerformanceMode());
}

void ICommonStateGetter::RequestToAcquireSleepLock(HLERequestContext& ctx) {
    LOG_WARNING(Service_AM, "(STUBBED) called");

    // Sleep lock is acquired immediately.
    applet->sleep_lock_event.Signal();

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void ICommonStateGetter::GetReaderLockAccessorEx(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto unknown = rp.Pop<u32>();

    LOG_INFO(Service_AM, "called, unknown={}", unknown);

    IPC::ResponseBuilder rb{ctx, 2, 0, 1};

    rb.Push(ResultSuccess);
    rb.PushIpcInterface<ILockAccessor>(system);
}

void ICommonStateGetter::GetAcquiredSleepLockEvent(HLERequestContext& ctx) {
    LOG_WARNING(Service_AM, "called");

    IPC::ResponseBuilder rb{ctx, 2, 1};
    rb.Push(ResultSuccess);
    rb.PushCopyObjects(applet->sleep_lock_event.GetHandle());
}

void ICommonStateGetter::GetCradleFwVersion(HLERequestContext& ctx) {
    LOG_WARNING(Service_AM, "(STUBBED) called");

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void ICommonStateGetter::IsVrModeEnabled(HLERequestContext& ctx) {
    LOG_DEBUG(Service_AM, "called");

    std::scoped_lock lk{applet->lock};

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.Push(applet->vr_mode_enabled);
}

void ICommonStateGetter::SetVrModeEnabled(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};

    std::scoped_lock lk{applet->lock};
    applet->vr_mode_enabled = rp.Pop<bool>();
    LOG_WARNING(Service_AM, "VR Mode is {}", applet->vr_mode_enabled ? "on" : "off");

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void ICommonStateGetter::SetLcdBacklighOffEnabled(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto is_lcd_backlight_off_enabled = rp.Pop<bool>();

    LOG_WARNING(Service_AM, "(STUBBED) called. is_lcd_backlight_off_enabled={}",
                is_lcd_backlight_off_enabled);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void ICommonStateGetter::BeginVrModeEx(HLERequestContext& ctx) {
    LOG_WARNING(Service_AM, "(STUBBED) called");

    std::scoped_lock lk{applet->lock};
    applet->vr_mode_enabled = true;

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void ICommonStateGetter::EndVrModeEx(HLERequestContext& ctx) {
    LOG_WARNING(Service_AM, "(STUBBED) called");

    std::scoped_lock lk{applet->lock};
    applet->vr_mode_enabled = false;

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void ICommonStateGetter::GetDefaultDisplayResolutionChangeEvent(HLERequestContext& ctx) {
    LOG_DEBUG(Service_AM, "called");

    IPC::ResponseBuilder rb{ctx, 2, 1};
    rb.Push(ResultSuccess);
    rb.PushCopyObjects(applet->message_queue.GetOperationModeChangedEvent());
}

void ICommonStateGetter::GetDefaultDisplayResolution(HLERequestContext& ctx) {
    LOG_DEBUG(Service_AM, "called");

    IPC::ResponseBuilder rb{ctx, 4};
    rb.Push(ResultSuccess);

    if (Settings::IsDockedMode()) {
        rb.Push(static_cast<u32>(Service::VI::DisplayResolution::DockedWidth));
        rb.Push(static_cast<u32>(Service::VI::DisplayResolution::DockedHeight));
    } else {
        rb.Push(static_cast<u32>(Service::VI::DisplayResolution::UndockedWidth));
        rb.Push(static_cast<u32>(Service::VI::DisplayResolution::UndockedHeight));
    }
}

void ICommonStateGetter::GetHdcpAuthenticationState(HLERequestContext& ctx) {
    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}
void ICommonStateGetter::GetHdcpAuthenticationStateChangeEvent(HLERequestContext& ctx) {
    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void ICommonStateGetter::SetCpuBoostMode(HLERequestContext& ctx) {
    LOG_DEBUG(Service_AM, "called, forwarding to APM:SYS");

    const auto& sm = system.ServiceManager();
    const auto apm_sys = sm.GetService<APM::APM_Sys>("apm:sys");
    ASSERT(apm_sys != nullptr);

    apm_sys->SetCpuBoostMode(ctx);
}

void ICommonStateGetter::GetBuiltInDisplayType(HLERequestContext& ctx) {
    LOG_WARNING(Service_AM, "(STUBBED) called");

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.Push(0);
}

void ICommonStateGetter::PerformSystemButtonPressingIfInFocus(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto system_button{rp.PopEnum<SystemButtonType>()};

    LOG_WARNING(Service_AM, "(STUBBED) called, system_button={}", system_button);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void ICommonStateGetter::GetAppletLaunchedHistory(HLERequestContext& ctx) {
    LOG_WARNING(Service_AM, "(STUBBED) called");

    std::shared_ptr<Applet> current_applet = applet;
    std::vector<AppletId> result;

    const size_t count = ctx.GetWriteBufferNumElements<AppletId>();
    size_t i;

    for (i = 0; i < count && current_applet != nullptr; i++) {
        result.push_back(current_applet->applet_id);
        current_applet = current_applet->caller_applet.lock();
    }

    ctx.WriteBuffer(result);

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.Push(static_cast<u32>(i));
}

void ICommonStateGetter::GetSettingsPlatformRegion(HLERequestContext& ctx) {
    LOG_WARNING(Service_AM, "(STUBBED) called");

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.PushEnum(SysPlatformRegion::Global);
}

void ICommonStateGetter::SetRequestExitToLibraryAppletAtExecuteNextProgramEnabled(
    HLERequestContext& ctx) {
    LOG_WARNING(Service_AM, "(STUBBED) called");

    std::scoped_lock lk{applet->lock};
    applet->request_exit_to_library_applet_at_execute_next_program_enabled = true;

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

} // namespace Service::AM
