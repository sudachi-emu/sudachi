// SPDX-FileCopyrightText: Copyright 2024 sudachi Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "core/hle/service/kernel_helpers.h"
#include "core/hle/service/service.h"

#include "core/hle/service/am/applet_message_queue.h"

namespace Service::AM {

struct Applet;

class ICommonStateGetter final : public ServiceFramework<ICommonStateGetter> {
public:
    explicit ICommonStateGetter(Core::System& system_, std::shared_ptr<Applet> applet_);
    ~ICommonStateGetter() override;

private:
    // This is nn::oe::FocusState
    enum class FocusState : u8 {
        InFocus = 1,
        NotInFocus = 2,
        Background = 3,
    };

    // This is nn::oe::OperationMode
    enum class OperationMode : u8 {
        Handheld = 0,
        Docked = 1,
    };

    // This is nn::am::service::SystemButtonType
    enum class SystemButtonType {
        None,
        HomeButtonShortPressing,
        HomeButtonLongPressing,
        PowerButtonShortPressing,
        PowerButtonLongPressing,
        ShutdownSystem,
        CaptureButtonShortPressing,
        CaptureButtonLongPressing,
    };

    enum class SysPlatformRegion : s32 {
        Global = 1,
        Terra = 2,
    };

    void GetEventHandle(HLERequestContext& ctx);
    void ReceiveMessage(HLERequestContext& ctx);
    void GetCurrentFocusState(HLERequestContext& ctx);
    void RequestToAcquireSleepLock(HLERequestContext& ctx);
    void GetAcquiredSleepLockEvent(HLERequestContext& ctx);
    void GetReaderLockAccessorEx(HLERequestContext& ctx);
    void GetDefaultDisplayResolutionChangeEvent(HLERequestContext& ctx);
    void GetHdcpAuthenticationState(HLERequestContext& ctx);
    void GetHdcpAuthenticationStateChangeEvent(HLERequestContext& ctx);
    void GetOperationMode(HLERequestContext& ctx);
    void GetPerformanceMode(HLERequestContext& ctx);
    void GetBootMode(HLERequestContext& ctx);
    void GetCradleFwVersion(HLERequestContext& ctx);
    void IsVrModeEnabled(HLERequestContext& ctx);
    void SetVrModeEnabled(HLERequestContext& ctx);
    void SetLcdBacklighOffEnabled(HLERequestContext& ctx);
    void BeginVrModeEx(HLERequestContext& ctx);
    void EndVrModeEx(HLERequestContext& ctx);
    void GetDefaultDisplayResolution(HLERequestContext& ctx);
    void SetCpuBoostMode(HLERequestContext& ctx);
    void GetBuiltInDisplayType(HLERequestContext& ctx);
    void PerformSystemButtonPressingIfInFocus(HLERequestContext& ctx);
    void GetAppletLaunchedHistory(HLERequestContext& ctx);
    void GetSettingsPlatformRegion(HLERequestContext& ctx);
    void SetRequestExitToLibraryAppletAtExecuteNextProgramEnabled(HLERequestContext& ctx);

    const std::shared_ptr<Applet> applet;
};

} // namespace Service::AM
