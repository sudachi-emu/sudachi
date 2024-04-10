// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "common/logging/log.h"
#include "core/core.h"
#include "core/core_timing.h"
#include "core/hle/service/ipc_helpers.h"
#include "core/hle/service/set/system_settings_server.h"
#include "core/hle/service/sm/sm.h"
#include "hid_core/hid_core.h"
#include "hid_core/hid_util.h"
#include "hid_core/resource_manager.h"

#include "hid_core/resources/applet_resource.h"
#include "hid_core/resources/debug_pad/debug_pad.h"
#include "hid_core/resources/digitizer/digitizer.h"
#include "hid_core/resources/hid_firmware_settings.h"
#include "hid_core/resources/keyboard/keyboard.h"
#include "hid_core/resources/mouse/debug_mouse.h"
#include "hid_core/resources/mouse/mouse.h"
#include "hid_core/resources/npad/npad.h"
#include "hid_core/resources/palma/palma.h"
#include "hid_core/resources/shared_memory_format.h"
#include "hid_core/resources/six_axis/console_six_axis.h"
#include "hid_core/resources/six_axis/seven_six_axis.h"
#include "hid_core/resources/six_axis/six_axis.h"
#include "hid_core/resources/system_buttons/capture_button.h"
#include "hid_core/resources/system_buttons/home_button.h"
#include "hid_core/resources/system_buttons/sleep_button.h"
#include "hid_core/resources/touch_screen/gesture.h"
#include "hid_core/resources/touch_screen/touch_screen.h"
#include "hid_core/resources/touch_screen/touch_screen_driver.h"
#include "hid_core/resources/touch_screen/touch_screen_resource.h"
#include "hid_core/resources/unique_pad/unique_pad.h"
#include "hid_core/resources/vibration/gc_vibration_device.h"
#include "hid_core/resources/vibration/n64_vibration_device.h"
#include "hid_core/resources/vibration/vibration_base.h"
#include "hid_core/resources/vibration/vibration_device.h"

namespace Service::HID {

// Updating period for each HID device.
// Period time is obtained by measuring the number of samples in a second on HW using a homebrew
// Correct npad_update_ns is 4ms this is overclocked to lower input lag
constexpr auto npad_update_ns = std::chrono::nanoseconds{1 * 1000 * 1000};    // (1ms, 1000Hz)
constexpr auto default_update_ns = std::chrono::nanoseconds{4 * 1000 * 1000}; // (4ms, 1000Hz)
constexpr auto mouse_keyboard_update_ns = std::chrono::nanoseconds{8 * 1000 * 1000}; // (8ms, 125Hz)
constexpr auto motion_update_ns = std::chrono::nanoseconds{5 * 1000 * 1000};         // (5ms, 200Hz)

ResourceManager::ResourceManager(Core::System& system_,
                                 std::shared_ptr<HidFirmwareSettings> settings)
    : firmware_settings{settings}, system{system_}, service_context{system_, "hid"} {
    applet_resource = std::make_shared<AppletResource>(system);

    // Register update callbacks
    npad_update_event = Core::Timing::CreateEvent("HID::UpdatePadCallback",
                                                  [this](s64 time, std::chrono::nanoseconds ns_late)
                                                      -> std::optional<std::chrono::nanoseconds> {
                                                      UpdateNpad(ns_late);
                                                      return std::nullopt;
                                                  });
    default_update_event = Core::Timing::CreateEvent(
        "HID::UpdateDefaultCallback",
        [this](s64 time,
               std::chrono::nanoseconds ns_late) -> std::optional<std::chrono::nanoseconds> {
            UpdateControllers(ns_late);
            return std::nullopt;
        });
    mouse_keyboard_update_event = Core::Timing::CreateEvent(
        "HID::UpdateMouseKeyboardCallback",
        [this](s64 time,
               std::chrono::nanoseconds ns_late) -> std::optional<std::chrono::nanoseconds> {
            UpdateMouseKeyboard(ns_late);
            return std::nullopt;
        });
    motion_update_event = Core::Timing::CreateEvent(
        "HID::UpdateMotionCallback",
        [this](s64 time,
               std::chrono::nanoseconds ns_late) -> std::optional<std::chrono::nanoseconds> {
            UpdateMotion(ns_late);
            return std::nullopt;
        });
}

ResourceManager::~ResourceManager() {
    system.CoreTiming().UnscheduleEvent(npad_update_event);
    system.CoreTiming().UnscheduleEvent(default_update_event);
    system.CoreTiming().UnscheduleEvent(mouse_keyboard_update_event);
    system.CoreTiming().UnscheduleEvent(motion_update_event);
    system.CoreTiming().UnscheduleEvent(touch_update_event);
    input_event->Finalize();
};

void ResourceManager::Initialize() {
    if (is_initialized) {
        return;
    }

    system.HIDCore().ReloadInputDevices();

    input_event = service_context.CreateEvent("ResourceManager:InputEvent");

    InitializeHandheldConfig();
    InitializeHidCommonSampler();
    InitializeTouchScreenSampler();
    InitializeConsoleSixAxisSampler();
    InitializeAHidSampler();

    is_initialized = true;
}

std::shared_ptr<AppletResource> ResourceManager::GetAppletResource() const {
    return applet_resource;
}

std::shared_ptr<CaptureButton> ResourceManager::GetCaptureButton() const {
    return capture_button;
}

std::shared_ptr<ConsoleSixAxis> ResourceManager::GetConsoleSixAxis() const {
    return console_six_axis;
}

std::shared_ptr<DebugMouse> ResourceManager::GetDebugMouse() const {
    return debug_mouse;
}

std::shared_ptr<DebugPad> ResourceManager::GetDebugPad() const {
    return debug_pad;
}

std::shared_ptr<Digitizer> ResourceManager::GetDigitizer() const {
    return digitizer;
}

std::shared_ptr<Gesture> ResourceManager::GetGesture() const {
    return gesture;
}

std::shared_ptr<HomeButton> ResourceManager::GetHomeButton() const {
    return home_button;
}

std::shared_ptr<Keyboard> ResourceManager::GetKeyboard() const {
    return keyboard;
}

std::shared_ptr<Mouse> ResourceManager::GetMouse() const {
    return mouse;
}

std::shared_ptr<NPad> ResourceManager::GetNpad() const {
    return npad;
}

std::shared_ptr<Palma> ResourceManager::GetPalma() const {
    return palma;
}

std::shared_ptr<SevenSixAxis> ResourceManager::GetSevenSixAxis() const {
    return seven_six_axis;
}

std::shared_ptr<SixAxis> ResourceManager::GetSixAxis() const {
    return six_axis;
}

std::shared_ptr<SleepButton> ResourceManager::GetSleepButton() const {
    return sleep_button;
}

std::shared_ptr<TouchScreen> ResourceManager::GetTouchScreen() const {
    return touch_screen;
}

std::shared_ptr<UniquePad> ResourceManager::GetUniquePad() const {
    return unique_pad;
}

Result ResourceManager::CreateAppletResource(u64 aruid) {
    if (aruid == SystemAruid) {
        const auto result = RegisterCoreAppletResource();
        if (result.IsError()) {
            return result;
        }
        return GetNpad()->ActivateNpadResource();
    }

    const auto result = CreateAppletResourceImpl(aruid);
    if (result.IsError()) {
        return result;
    }

    // Homebrew doesn't try to activate some controllers, so we activate them by default
    npad->Activate();
    six_axis->Activate();
    touch_screen->Activate();
    gesture->Activate();

    return GetNpad()->ActivateNpadResource(aruid);
}

Result ResourceManager::CreateAppletResourceImpl(u64 aruid) {
    std::scoped_lock lock{shared_mutex};
    return applet_resource->CreateAppletResource(aruid);
}

void ResourceManager::InitializeHandheldConfig() {
    handheld_config = std::make_shared<HandheldConfig>();
    handheld_config->is_handheld_hid_enabled = true;
    handheld_config->is_joycon_rail_enabled = true;
    handheld_config->is_force_handheld_style_vibration = false;
    handheld_config->is_force_handheld = false;
    if (firmware_settings->IsHandheldForced()) {
        handheld_config->is_joycon_rail_enabled = false;
    }
}

void ResourceManager::InitializeHidCommonSampler() {
    debug_pad = std::make_shared<DebugPad>(system.HIDCore());
    mouse = std::make_shared<Mouse>(system.HIDCore());
    debug_mouse = std::make_shared<DebugMouse>(system.HIDCore());
    keyboard = std::make_shared<Keyboard>(system.HIDCore());
    unique_pad = std::make_shared<UniquePad>(system.HIDCore());
    npad = std::make_shared<NPad>(system.HIDCore(), service_context);
    home_button = std::make_shared<HomeButton>(system.HIDCore());
    sleep_button = std::make_shared<SleepButton>(system.HIDCore());
    capture_button = std::make_shared<CaptureButton>(system.HIDCore());
    digitizer = std::make_shared<Digitizer>(system.HIDCore());

    palma = std::make_shared<Palma>(system.HIDCore(), service_context);
    six_axis = std::make_shared<SixAxis>(system.HIDCore(), npad);

    debug_pad->SetAppletResource(applet_resource, &shared_mutex);
    digitizer->SetAppletResource(applet_resource, &shared_mutex);
    unique_pad->SetAppletResource(applet_resource, &shared_mutex);
    keyboard->SetAppletResource(applet_resource, &shared_mutex);

    const auto settings =
        system.ServiceManager().GetService<Service::Set::ISystemSettingsServer>("set:sys", true);
    npad->SetNpadExternals(applet_resource, &shared_mutex, handheld_config, input_event,
                           &input_mutex, settings);

    six_axis->SetAppletResource(applet_resource, &shared_mutex);
    mouse->SetAppletResource(applet_resource, &shared_mutex);
    debug_mouse->SetAppletResource(applet_resource, &shared_mutex);
    home_button->SetAppletResource(applet_resource, &shared_mutex);
    sleep_button->SetAppletResource(applet_resource, &shared_mutex);
    capture_button->SetAppletResource(applet_resource, &shared_mutex);

    system.CoreTiming().ScheduleLoopingEvent(npad_update_ns, npad_update_ns, npad_update_event);
    system.CoreTiming().ScheduleLoopingEvent(default_update_ns, default_update_ns,
                                             default_update_event);
    system.CoreTiming().ScheduleLoopingEvent(mouse_keyboard_update_ns, mouse_keyboard_update_ns,
                                             mouse_keyboard_update_event);
    system.CoreTiming().ScheduleLoopingEvent(motion_update_ns, motion_update_ns,
                                             motion_update_event);
}

void ResourceManager::InitializeTouchScreenSampler() {
    // This is nn.hid.TouchScreenSampler
    touch_resource = std::make_shared<TouchResource>(system);
    touch_driver = std::make_shared<TouchDriver>(system.HIDCore());
    touch_screen = std::make_shared<TouchScreen>(touch_resource);
    gesture = std::make_shared<Gesture>(touch_resource);

    touch_update_event = Core::Timing::CreateEvent(
        "HID::TouchUpdateCallback",
        [this](s64 time,
               std::chrono::nanoseconds ns_late) -> std::optional<std::chrono::nanoseconds> {
            touch_resource->OnTouchUpdate(time);
            return std::nullopt;
        });

    touch_resource->SetTouchDriver(touch_driver);
    touch_resource->SetAppletResource(applet_resource, &shared_mutex);
    touch_resource->SetInputEvent(input_event, &input_mutex);
    touch_resource->SetHandheldConfig(handheld_config);
    touch_resource->SetTimerEvent(touch_update_event);
}

void ResourceManager::InitializeConsoleSixAxisSampler() {
    console_six_axis = std::make_shared<ConsoleSixAxis>(system.HIDCore());
    seven_six_axis = std::make_shared<SevenSixAxis>(system);

    console_six_axis->SetAppletResource(applet_resource, &shared_mutex);
}

void ResourceManager::InitializeAHidSampler() {
    // TODO
}

Result ResourceManager::RegisterCoreAppletResource() {
    std::scoped_lock lock{shared_mutex};
    return applet_resource->RegisterCoreAppletResource();
}

Result ResourceManager::UnregisterCoreAppletResource() {
    std::scoped_lock lock{shared_mutex};
    return applet_resource->UnregisterCoreAppletResource();
}

Result ResourceManager::RegisterAppletResourceUserId(u64 aruid, bool bool_value) {
    std::scoped_lock lock{shared_mutex};
    auto result = applet_resource->RegisterAppletResourceUserId(aruid, bool_value);
    if (result.IsSuccess()) {
        result = npad->RegisterAppletResourceUserId(aruid);
    }
    return result;
}

void ResourceManager::UnregisterAppletResourceUserId(u64 aruid) {
    std::scoped_lock lock{shared_mutex};
    applet_resource->UnregisterAppletResourceUserId(aruid);
    npad->UnregisterAppletResourceUserId(aruid);
    // palma->UnregisterAppletResourceUserId(aruid);
}

Result ResourceManager::GetSharedMemoryHandle(Kernel::KSharedMemory** out_handle, u64 aruid) {
    std::scoped_lock lock{shared_mutex};
    return applet_resource->GetSharedMemoryHandle(out_handle, aruid);
}

void ResourceManager::FreeAppletResourceId(u64 aruid) {
    std::scoped_lock lock{shared_mutex};
    applet_resource->FreeAppletResourceId(aruid);
    npad->FreeAppletResourceId(aruid);
}

void ResourceManager::EnableInput(u64 aruid, bool is_enabled) {
    std::scoped_lock lock{shared_mutex};
    applet_resource->EnableInput(aruid, is_enabled);
}

void ResourceManager::EnableSixAxisSensor(u64 aruid, bool is_enabled) {
    std::scoped_lock lock{shared_mutex};
    applet_resource->EnableSixAxisSensor(aruid, is_enabled);
}

void ResourceManager::EnablePadInput(u64 aruid, bool is_enabled) {
    std::scoped_lock lock{shared_mutex};
    applet_resource->EnablePadInput(aruid, is_enabled);
}

void ResourceManager::EnableTouchScreen(u64 aruid, bool is_enabled) {
    std::scoped_lock lock{shared_mutex};
    applet_resource->EnableTouchScreen(aruid, is_enabled);
}

NpadVibrationBase* ResourceManager::GetVibrationDevice(
    const Core::HID::VibrationDeviceHandle& handle) {
    return npad->GetVibrationDevice(handle);
}

NpadN64VibrationDevice* ResourceManager::GetN64VibrationDevice(
    const Core::HID::VibrationDeviceHandle& handle) {
    return npad->GetN64VibrationDevice(handle);
}

NpadVibrationDevice* ResourceManager::GetNSVibrationDevice(
    const Core::HID::VibrationDeviceHandle& handle) {
    return npad->GetNSVibrationDevice(handle);
}

NpadGcVibrationDevice* ResourceManager::GetGcVibrationDevice(
    const Core::HID::VibrationDeviceHandle& handle) {
    return npad->GetGcVibrationDevice(handle);
}

Result ResourceManager::SetAruidValidForVibration(u64 aruid, bool is_enabled) {
    std::scoped_lock lock{shared_mutex};
    const bool has_changed = applet_resource->SetAruidValidForVibration(aruid, is_enabled);

    if (has_changed) {
        auto devices = npad->GetAllVibrationDevices();
        for ([[maybe_unused]] auto* device : devices) {
            // TODO
        }
    }

    auto* vibration_handler = npad->GetVibrationHandler();
    if (aruid != vibration_handler->GetSessionAruid()) {
        vibration_handler->EndPermitVibrationSession();
    }

    return ResultSuccess;
}

void ResourceManager::SetForceHandheldStyleVibration(bool is_forced) {
    handheld_config->is_force_handheld_style_vibration = is_forced;
}

Result ResourceManager::IsVibrationAruidActive(u64 aruid, bool& is_active) const {
    std::scoped_lock lock{shared_mutex};
    is_active = applet_resource->IsVibrationAruidActive(aruid);
    return ResultSuccess;
}

Result ResourceManager::GetVibrationDeviceInfo(Core::HID::VibrationDeviceInfo& device_info,
                                               const Core::HID::VibrationDeviceHandle& handle) {
    bool check_device_index = false;

    const Result is_valid = IsVibrationHandleValid(handle);
    if (is_valid.IsError()) {
        return is_valid;
    }

    switch (handle.npad_type) {
    case Core::HID::NpadStyleIndex::Fullkey:
    case Core::HID::NpadStyleIndex::Handheld:
    case Core::HID::NpadStyleIndex::JoyconDual:
    case Core::HID::NpadStyleIndex::JoyconLeft:
    case Core::HID::NpadStyleIndex::JoyconRight:
        device_info.type = Core::HID::VibrationDeviceType::LinearResonantActuator;
        check_device_index = true;
        break;
    case Core::HID::NpadStyleIndex::GameCube:
        device_info.type = Core::HID::VibrationDeviceType::GcErm;
        break;
    case Core::HID::NpadStyleIndex::N64:
        device_info.type = Core::HID::VibrationDeviceType::N64;
        break;
    default:
        device_info.type = Core::HID::VibrationDeviceType::Unknown;
        break;
    }

    device_info.position = Core::HID::VibrationDevicePosition::None;
    if (check_device_index) {
        switch (handle.device_index) {
        case Core::HID::DeviceIndex::Left:
            device_info.position = Core::HID::VibrationDevicePosition::Left;
            break;
        case Core::HID::DeviceIndex::Right:
            device_info.position = Core::HID::VibrationDevicePosition::Right;
            break;
        case Core::HID::DeviceIndex::None:
        default:
            ASSERT_MSG(false, "DeviceIndex should never be None!");
            break;
        }
    }
    return ResultSuccess;
}

Result ResourceManager::SendVibrationValue(u64 aruid,
                                           const Core::HID::VibrationDeviceHandle& handle,
                                           const Core::HID::VibrationValue& value) {
    bool has_active_aruid{};
    NpadVibrationDevice* device{nullptr};
    Result result = IsVibrationAruidActive(aruid, has_active_aruid);

    if (result.IsSuccess() && has_active_aruid) {
        result = IsVibrationHandleValid(handle);
    }
    if (result.IsSuccess() && has_active_aruid) {
        device = GetNSVibrationDevice(handle);
    }
    if (device != nullptr) {
        // Prevent sending vibrations to an inactive vibration handle
        if (!device->IsActive()) {
            return ResultSuccess;
        }
        result = device->SendVibrationValue(value);
    }
    return result;
}

Result ResourceManager::GetTouchScreenFirmwareVersion(Core::HID::FirmwareVersion& firmware) const {
    return ResultSuccess;
}

void ResourceManager::UpdateControllers(std::chrono::nanoseconds ns_late) {
    auto& core_timing = system.CoreTiming();
    debug_pad->OnUpdate(core_timing);
    digitizer->OnUpdate(core_timing);
    unique_pad->OnUpdate(core_timing);
    palma->OnUpdate(core_timing);
    home_button->OnUpdate(core_timing);
    sleep_button->OnUpdate(core_timing);
    capture_button->OnUpdate(core_timing);
}

void ResourceManager::UpdateNpad(std::chrono::nanoseconds ns_late) {
    auto& core_timing = system.CoreTiming();
    npad->OnUpdate(core_timing);
}

void ResourceManager::UpdateMouseKeyboard(std::chrono::nanoseconds ns_late) {
    auto& core_timing = system.CoreTiming();
    mouse->OnUpdate(core_timing);
    debug_mouse->OnUpdate(core_timing);
    keyboard->OnUpdate(core_timing);
}

void ResourceManager::UpdateMotion(std::chrono::nanoseconds ns_late) {
    auto& core_timing = system.CoreTiming();
    six_axis->OnUpdate(core_timing);
    seven_six_axis->OnUpdate(core_timing);
    console_six_axis->OnUpdate(core_timing);
}

} // namespace Service::HID
