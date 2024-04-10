// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "core/hle/service/kernel_helpers.h"
#include "core/hle/service/service.h"

namespace Core {
class System;
}

namespace Core::Timing {
struct EventType;
}

namespace Kernel {
class KSharedMemory;
}

namespace Service::HID {
class AppletResource;
class CaptureButton;
class Controller_Stubbed;
class ConsoleSixAxis;
class DebugMouse;
class DebugPad;
class Digitizer;
class Gesture;
class HomeButton;
class Keyboard;
class Mouse;
class NPad;
class Palma;
class SevenSixAxis;
class SixAxis;
class SleepButton;
class TouchScreen;
class UniquePad;

class ResourceManager {

public:
    explicit ResourceManager(Core::System& system_);
    ~ResourceManager();

    void Initialize();

    std::shared_ptr<AppletResource> GetAppletResource() const;
    std::shared_ptr<CaptureButton> GetCaptureButton() const;
    std::shared_ptr<ConsoleSixAxis> GetConsoleSixAxis() const;
    std::shared_ptr<DebugMouse> GetDebugMouse() const;
    std::shared_ptr<DebugPad> GetDebugPad() const;
    std::shared_ptr<Digitizer> GetDigitizer() const;
    std::shared_ptr<Gesture> GetGesture() const;
    std::shared_ptr<HomeButton> GetHomeButton() const;
    std::shared_ptr<Keyboard> GetKeyboard() const;
    std::shared_ptr<Mouse> GetMouse() const;
    std::shared_ptr<NPad> GetNpad() const;
    std::shared_ptr<Palma> GetPalma() const;
    std::shared_ptr<SevenSixAxis> GetSevenSixAxis() const;
    std::shared_ptr<SixAxis> GetSixAxis() const;
    std::shared_ptr<SleepButton> GetSleepButton() const;
    std::shared_ptr<TouchScreen> GetTouchScreen() const;
    std::shared_ptr<UniquePad> GetUniquePad() const;

    Result CreateAppletResource(u64 aruid);

    Result RegisterCoreAppletResource();
    Result UnregisterCoreAppletResource();
    Result RegisterAppletResourceUserId(u64 aruid, bool bool_value);
    void UnregisterAppletResourceUserId(u64 aruid);

    Result GetSharedMemoryHandle(Kernel::KSharedMemory** out_handle, u64 aruid);
    void FreeAppletResourceId(u64 aruid);

    void EnableInput(u64 aruid, bool is_enabled);
    void EnableSixAxisSensor(u64 aruid, bool is_enabled);
    void EnablePadInput(u64 aruid, bool is_enabled);
    void EnableTouchScreen(u64 aruid, bool is_enabled);

    void UpdateControllers(std::chrono::nanoseconds ns_late);
    void UpdateNpad(std::chrono::nanoseconds ns_late);
    void UpdateMouseKeyboard(std::chrono::nanoseconds ns_late);
    void UpdateMotion(std::chrono::nanoseconds ns_late);

private:
    Result CreateAppletResourceImpl(u64 aruid);
    void InitializeHidCommonSampler();
    void InitializeTouchScreenSampler();
    void InitializeConsoleSixAxisSampler();
    void InitializeAHidSampler();

    bool is_initialized{false};

    mutable std::recursive_mutex shared_mutex;
    std::shared_ptr<AppletResource> applet_resource = nullptr;

    std::shared_ptr<CaptureButton> capture_button = nullptr;
    std::shared_ptr<ConsoleSixAxis> console_six_axis = nullptr;
    std::shared_ptr<DebugMouse> debug_mouse = nullptr;
    std::shared_ptr<DebugPad> debug_pad = nullptr;
    std::shared_ptr<Digitizer> digitizer = nullptr;
    std::shared_ptr<Gesture> gesture = nullptr;
    std::shared_ptr<HomeButton> home_button = nullptr;
    std::shared_ptr<Keyboard> keyboard = nullptr;
    std::shared_ptr<Mouse> mouse = nullptr;
    std::shared_ptr<NPad> npad = nullptr;
    std::shared_ptr<Palma> palma = nullptr;
    std::shared_ptr<SevenSixAxis> seven_six_axis = nullptr;
    std::shared_ptr<SixAxis> six_axis = nullptr;
    std::shared_ptr<SleepButton> sleep_button = nullptr;
    std::shared_ptr<TouchScreen> touch_screen = nullptr;
    std::shared_ptr<UniquePad> unique_pad = nullptr;

    // TODO: Create these resources
    // std::shared_ptr<AudioControl> audio_control = nullptr;
    // std::shared_ptr<ButtonConfig> button_config = nullptr;
    // std::shared_ptr<Config> config = nullptr;
    // std::shared_ptr<Connection> connection = nullptr;
    // std::shared_ptr<CustomConfig> custom_config = nullptr;
    // std::shared_ptr<Digitizer> digitizer = nullptr;
    // std::shared_ptr<Hdls> hdls = nullptr;
    // std::shared_ptr<PlayReport> play_report = nullptr;
    // std::shared_ptr<Rail> rail = nullptr;

    Core::System& system;
    KernelHelpers::ServiceContext service_context;
};

class IAppletResource final : public ServiceFramework<IAppletResource> {
public:
    explicit IAppletResource(Core::System& system_, std::shared_ptr<ResourceManager> resource,
                             u64 applet_resource_user_id);
    ~IAppletResource() override;

private:
    void GetSharedMemoryHandle(HLERequestContext& ctx);

    std::shared_ptr<Core::Timing::EventType> npad_update_event;
    std::shared_ptr<Core::Timing::EventType> default_update_event;
    std::shared_ptr<Core::Timing::EventType> mouse_keyboard_update_event;
    std::shared_ptr<Core::Timing::EventType> motion_update_event;

    u64 aruid;
    std::shared_ptr<ResourceManager> resource_manager;
};

} // namespace Service::HID
