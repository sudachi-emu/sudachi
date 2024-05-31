// SPDX-FileCopyrightText: Copyright 2024 sudachi Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "core/hle/service/am/am_types.h"
#include "core/hle/service/apm/apm_controller.h"
#include "core/hle/service/cmif_types.h"
#include "core/hle/service/pm/pm.h"
#include "core/hle/service/service.h"
#include "core/hle/service/set/settings_types.h"

namespace Kernel {
class KReadableEvent;
}

namespace Service::AM {

struct Applet;
class ILockAccessor;

class ICommonStateGetter final : public ServiceFramework<ICommonStateGetter> {
public:
    explicit ICommonStateGetter(Core::System& system_, std::shared_ptr<Applet> applet_);
    ~ICommonStateGetter() override;

private:
    Result GetEventHandle(OutCopyHandle<Kernel::KReadableEvent> out_event);
    Result ReceiveMessage(Out<AppletMessage> out_applet_message);
    Result GetCurrentFocusState(Out<FocusState> out_focus_state);
    Result RequestToAcquireSleepLock();
    Result GetAcquiredSleepLockEvent(OutCopyHandle<Kernel::KReadableEvent> out_event);
    Result GetReaderLockAccessorEx(Out<SharedPointer<ILockAccessor>> out_lock_accessor,
                                   u32 button_type);
    Result GetWriterLockAccessorEx(Out<SharedPointer<ILockAccessor>> out_lock_accessor,
                                   u32 button_type);
    Result GetDefaultDisplayResolutionChangeEvent(OutCopyHandle<Kernel::KReadableEvent> out_event);
    Result GetHdcpAuthenticationState(Out<s32> out_state);
    Result GetHdcpAuthenticationStateChangeEvent(OutCopyHandle<Kernel::KReadableEvent> out_event);
    Result GetOperationMode(Out<OperationMode> out_operation_mode);
    Result GetPerformanceMode(Out<APM::PerformanceMode> out_performance_mode);
    Result GetBootMode(Out<PM::SystemBootMode> out_boot_mode);
    Result GetCradleFwVersion();
    Result IsVrModeEnabled(Out<bool> out_is_vr_mode_enabled);
    Result SetVrModeEnabled(bool is_vr_mode_enabled);
    Result SetLcdBacklighOffEnabled(bool is_lcd_backlight_off_enabled);
    Result BeginVrModeEx();
    Result EndVrModeEx();
    Result IsInControllerFirmwareUpdateSection(
        Out<bool> out_is_in_controller_firmware_update_section);
    Result GetDefaultDisplayResolution(Out<s32> out_width, Out<s32> out_height);
    Result GetBuiltInDisplayType(Out<s32> out_display_type);
    Result PerformSystemButtonPressingIfInFocus(SystemButtonType type);
    Result GetOperationModeSystemInfo(Out<u32> out_operation_mode_system_info);
    Result GetAppletLaunchedHistory(Out<s32> out_count,
                                    OutArray<AppletId, BufferAttr_HipcMapAlias> out_applet_ids);
    Result GetSettingsPlatformRegion(Out<Set::PlatformRegion> out_settings_platform_region);
    Result SetRequestExitToLibraryAppletAtExecuteNextProgramEnabled();

    void SetCpuBoostMode(HLERequestContext& ctx);

    const std::shared_ptr<Applet> m_applet;
};

} // namespace Service::AM
