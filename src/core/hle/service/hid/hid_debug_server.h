// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "core/hle/service/cmif_types.h"
#include "core/hle/service/service.h"
#include "hid_core/resources/touch_screen/touch_types.h"

namespace Core {
class System;
}

namespace Service::HID {
class ResourceManager;
class HidFirmwareSettings;

class IHidDebugServer final : public ServiceFramework<IHidDebugServer> {
public:
    explicit IHidDebugServer(Core::System& system_, std::shared_ptr<ResourceManager> resource,
                             std::shared_ptr<HidFirmwareSettings> settings);
    ~IHidDebugServer() override;

private:
    Result DeactivateTouchScreen();
    Result SetTouchScreenAutoPilotState(
        InArray<TouchState, BufferAttr_HipcMapAlias> auto_pilot_buffer);
    Result UnsetTouchScreenAutoPilotState();
    Result GetTouchScreenConfiguration(
        Out<Core::HID::TouchScreenConfigurationForNx> out_touchscreen_config,
        ClientAppletResourceUserId aruid);
    Result ProcessTouchScreenAutoTune();
    Result ForceStopTouchScreenManagement();
    Result ForceRestartTouchScreenManagement(u32 basic_gesture_id,
                                             ClientAppletResourceUserId aruid);
    Result IsTouchScreenManaged(Out<bool> out_is_managed);
    Result DeactivateGesture();

    std::shared_ptr<ResourceManager> GetResourceManager();

    std::shared_ptr<ResourceManager> resource_manager;
    std::shared_ptr<HidFirmwareSettings> firmware_settings;
};

} // namespace Service::HID
