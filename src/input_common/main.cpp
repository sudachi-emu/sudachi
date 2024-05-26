// SPDX-FileCopyrightText: 2017 Citra Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <memory>
#include "common/input.h"
#include "common/param_package.h"
#include "input_common/drivers/camera.h"
#include "input_common/drivers/keyboard.h"
#include "input_common/drivers/mouse.h"
#include "input_common/drivers/tas_input.h"
#include "input_common/drivers/touch_screen.h"
#include "input_common/drivers/udp_client.h"
#include "input_common/drivers/virtual_amiibo.h"
#include "input_common/drivers/virtual_gamepad.h"
#include "input_common/helpers/stick_from_buttons.h"
#include "input_common/helpers/touch_from_buttons.h"
#include "input_common/input_engine.h"
#include "input_common/input_mapping.h"
#include "input_common/input_poller.h"
#include "input_common/main.h"

#ifdef HAVE_LIBUSB
#include "input_common/drivers/gc_adapter.h"
#endif
#ifdef HAVE_SDL3
#include "input_common/drivers/joycon.h"
#include "input_common/drivers/sdl_driver.h"
#endif

#ifdef ANDROID
#include "input_common/drivers/android.h"
#endif

namespace InputCommon {

/// Dummy engine to get periodic updates
class UpdateEngine final : public InputEngine {
public:
    explicit UpdateEngine(std::string input_engine_) : InputEngine(std::move(input_engine_)) {
        PreSetController(identifier);
    }

    void PumpEvents() {
        SetButton(identifier, 0, last_state);
        last_state = !last_state;
    }

private:
    static constexpr PadIdentifier identifier = {
        .guid = Common::UUID{},
        .port = 0,
        .pad = 0,
    };

    bool last_state{};
};

struct InputSubsystem::Impl {
    template <typename Engine>
    void RegisterEngine(std::string name, std::shared_ptr<Engine>& engine) {
        MappingCallback mapping_callback{[this](const MappingData& data) { RegisterInput(data); }};

        engine = std::make_shared<Engine>(name);
        engine->SetMappingCallback(mapping_callback);

        std::shared_ptr<InputFactory> input_factory = std::make_shared<InputFactory>(engine);
        std::shared_ptr<OutputFactory> output_factory = std::make_shared<OutputFactory>(engine);
        Common::Input::RegisterInputFactory(engine->GetEngineName(), std::move(input_factory));
        Common::Input::RegisterOutputFactory(engine->GetEngineName(), std::move(output_factory));
    }

    void Initialize() {
        mapping_factory = std::make_shared<MappingFactory>();

        RegisterEngine("updater", update_engine);
        RegisterEngine("keyboard", keyboard);
        RegisterEngine("mouse", mouse);
        RegisterEngine("touch", touch_screen);
#ifdef HAVE_LIBUSB
        RegisterEngine("gcpad", gcadapter);
#endif
        RegisterEngine("cemuhookudp", udp_client);
        RegisterEngine("tas", tas_input);
        RegisterEngine("camera", camera);
#ifdef ANDROID
        RegisterEngine("android", android);
#endif
        RegisterEngine("virtual_amiibo", virtual_amiibo);
        RegisterEngine("virtual_gamepad", virtual_gamepad);
#ifdef HAVE_SDL3
        RegisterEngine("sdl", sdl);
        RegisterEngine("joycon", joycon);
#endif

        Common::Input::RegisterInputFactory("touch_from_button",
                                            std::make_shared<TouchFromButton>());
        Common::Input::RegisterInputFactory("analog_from_button",
                                            std::make_shared<StickFromButton>());
    }

    template <typename Engine>
    void UnregisterEngine(std::shared_ptr<Engine>& engine) {
        Common::Input::UnregisterInputFactory(engine->GetEngineName());
        Common::Input::UnregisterOutputFactory(engine->GetEngineName());
        engine.reset();
    }

    void Shutdown() {
        UnregisterEngine(update_engine);
        UnregisterEngine(keyboard);
        UnregisterEngine(mouse);
        UnregisterEngine(touch_screen);
#ifdef HAVE_LIBUSB
        UnregisterEngine(gcadapter);
#endif
        UnregisterEngine(udp_client);
        UnregisterEngine(tas_input);
        UnregisterEngine(camera);
#ifdef ANDROID
        UnregisterEngine(android);
#endif
        UnregisterEngine(virtual_amiibo);
        UnregisterEngine(virtual_gamepad);
#ifdef HAVE_SDL3
        UnregisterEngine(sdl);
        UnregisterEngine(joycon);
#endif

        Common::Input::UnregisterInputFactory("touch_from_button");
        Common::Input::UnregisterInputFactory("analog_from_button");
    }

    [[nodiscard]] std::vector<Common::ParamPackage> GetInputDevices() const {
        std::vector<Common::ParamPackage> devices = {
            Common::ParamPackage{{"display", "Any"}, {"engine", "any"}},
        };

#ifndef ANDROID
        auto keyboard_devices = keyboard->GetInputDevices();
        devices.insert(devices.end(), keyboard_devices.begin(), keyboard_devices.end());
        auto mouse_devices = mouse->GetInputDevices();
        devices.insert(devices.end(), mouse_devices.begin(), mouse_devices.end());
#endif
#ifdef ANDROID
        auto android_devices = android->GetInputDevices();
        devices.insert(devices.end(), android_devices.begin(), android_devices.end());
#endif
#ifdef HAVE_LIBUSB
        auto gcadapter_devices = gcadapter->GetInputDevices();
        devices.insert(devices.end(), gcadapter_devices.begin(), gcadapter_devices.end());
#endif
        auto udp_devices = udp_client->GetInputDevices();
        devices.insert(devices.end(), udp_devices.begin(), udp_devices.end());
#ifdef HAVE_SDL3
        auto joycon_devices = joycon->GetInputDevices();
        devices.insert(devices.end(), joycon_devices.begin(), joycon_devices.end());
        auto sdl_devices = sdl->GetInputDevices();
        devices.insert(devices.end(), sdl_devices.begin(), sdl_devices.end());
#endif

        return devices;
    }

    [[nodiscard]] std::shared_ptr<InputEngine> GetInputEngine(
        const Common::ParamPackage& params) const {
        if (!params.Has("engine") || params.Get("engine", "") == "any") {
            return nullptr;
        }
        const std::string engine = params.Get("engine", "");
        if (engine == keyboard->GetEngineName()) {
            return keyboard;
        }
        if (engine == mouse->GetEngineName()) {
            return mouse;
        }
#ifdef ANDROID
        if (engine == android->GetEngineName()) {
            return android;
        }
#endif
#ifdef HAVE_LIBUSB
        if (engine == gcadapter->GetEngineName()) {
            return gcadapter;
        }
#endif
        if (engine == udp_client->GetEngineName()) {
            return udp_client;
        }
#ifdef HAVE_SDL3
        if (engine == sdl->GetEngineName()) {
            return sdl;
        }
        if (engine == joycon->GetEngineName()) {
            return joycon;
        }
#endif
        return nullptr;
    }

    [[nodiscard]] AnalogMapping GetAnalogMappingForDevice(
        const Common::ParamPackage& params) const {
        const auto input_engine = GetInputEngine(params);

        if (input_engine == nullptr) {
            return {};
        }

        return input_engine->GetAnalogMappingForDevice(params);
    }

    [[nodiscard]] ButtonMapping GetButtonMappingForDevice(
        const Common::ParamPackage& params) const {
        const auto input_engine = GetInputEngine(params);

        if (input_engine == nullptr) {
            return {};
        }

        return input_engine->GetButtonMappingForDevice(params);
    }

    [[nodiscard]] MotionMapping GetMotionMappingForDevice(
        const Common::ParamPackage& params) const {
        const auto input_engine = GetInputEngine(params);

        if (input_engine == nullptr) {
            return {};
        }

        return input_engine->GetMotionMappingForDevice(params);
    }

    Common::Input::ButtonNames GetButtonName(const Common::ParamPackage& params) const {
        if (!params.Has("engine") || params.Get("engine", "") == "any") {
            return Common::Input::ButtonNames::Undefined;
        }
        const auto input_engine = GetInputEngine(params);

        if (input_engine == nullptr) {
            return Common::Input::ButtonNames::Invalid;
        }

        return input_engine->GetUIName(params);
    }

    bool IsStickInverted(const Common::ParamPackage& params) {
        const auto input_engine = GetInputEngine(params);

        if (input_engine == nullptr) {
            return false;
        }

        return input_engine->IsStickInverted(params);
    }

    bool IsController(const Common::ParamPackage& params) {
        const std::string engine = params.Get("engine", "");
        if (engine == mouse->GetEngineName()) {
            return true;
        }
#ifdef ANDROID
        if (engine == android->GetEngineName()) {
            return true;
        }
#endif
#ifdef HAVE_LIBUSB
        if (engine == gcadapter->GetEngineName()) {
            return true;
        }
#endif
        if (engine == udp_client->GetEngineName()) {
            return true;
        }
        if (engine == tas_input->GetEngineName()) {
            return true;
        }
        if (engine == virtual_gamepad->GetEngineName()) {
            return true;
        }
#ifdef HAVE_SDL3
        if (engine == sdl->GetEngineName()) {
            return true;
        }
        if (engine == joycon->GetEngineName()) {
            return true;
        }
#endif
        return false;
    }

    void BeginConfiguration() {
        keyboard->BeginConfiguration();
        mouse->BeginConfiguration();
#ifdef ANDROID
        android->BeginConfiguration();
#endif
#ifdef HAVE_LIBUSB
        gcadapter->BeginConfiguration();
#endif
        udp_client->BeginConfiguration();
#ifdef HAVE_SDL3
        sdl->BeginConfiguration();
        joycon->BeginConfiguration();
#endif
    }

    void EndConfiguration() {
        keyboard->EndConfiguration();
        mouse->EndConfiguration();
#ifdef ANDROID
        android->EndConfiguration();
#endif
#ifdef HAVE_LIBUSB
        gcadapter->EndConfiguration();
#endif
        udp_client->EndConfiguration();
#ifdef HAVE_SDL3
        sdl->EndConfiguration();
        joycon->EndConfiguration();
#endif
    }

    void PumpEvents() const {
        update_engine->PumpEvents();
#ifdef HAVE_SDL3
        sdl->PumpEvents();
#endif
    }

    void RegisterInput(const MappingData& data) {
        mapping_factory->RegisterInput(data);
    }

    std::shared_ptr<MappingFactory> mapping_factory;

    std::shared_ptr<UpdateEngine> update_engine;
    std::shared_ptr<Keyboard> keyboard;
    std::shared_ptr<Mouse> mouse;
    std::shared_ptr<TouchScreen> touch_screen;
    std::shared_ptr<TasInput::Tas> tas_input;
    std::shared_ptr<CemuhookUDP::UDPClient> udp_client;
    std::shared_ptr<Camera> camera;
    std::shared_ptr<VirtualAmiibo> virtual_amiibo;
    std::shared_ptr<VirtualGamepad> virtual_gamepad;

#ifdef HAVE_LIBUSB
    std::shared_ptr<GCAdapter> gcadapter;
#endif

#ifdef HAVE_SDL3
    std::shared_ptr<SDLDriver> sdl;
    std::shared_ptr<Joycons> joycon;
#endif

#ifdef ANDROID
    std::shared_ptr<Android> android;
#endif
};

InputSubsystem::InputSubsystem() : impl{std::make_unique<Impl>()} {}

InputSubsystem::~InputSubsystem() = default;

void InputSubsystem::Initialize() {
    impl->Initialize();
}

void InputSubsystem::Shutdown() {
    impl->Shutdown();
}

Keyboard* InputSubsystem::GetKeyboard() {
    return impl->keyboard.get();
}

const Keyboard* InputSubsystem::GetKeyboard() const {
    return impl->keyboard.get();
}

Mouse* InputSubsystem::GetMouse() {
    return impl->mouse.get();
}

const Mouse* InputSubsystem::GetMouse() const {
    return impl->mouse.get();
}

TouchScreen* InputSubsystem::GetTouchScreen() {
    return impl->touch_screen.get();
}

const TouchScreen* InputSubsystem::GetTouchScreen() const {
    return impl->touch_screen.get();
}

TasInput::Tas* InputSubsystem::GetTas() {
    return impl->tas_input.get();
}

const TasInput::Tas* InputSubsystem::GetTas() const {
    return impl->tas_input.get();
}

Camera* InputSubsystem::GetCamera() {
    return impl->camera.get();
}

const Camera* InputSubsystem::GetCamera() const {
    return impl->camera.get();
}

#ifdef ANDROID
Android* InputSubsystem::GetAndroid() {
    return impl->android.get();
}

const Android* InputSubsystem::GetAndroid() const {
    return impl->android.get();
}
#endif

VirtualAmiibo* InputSubsystem::GetVirtualAmiibo() {
    return impl->virtual_amiibo.get();
}

const VirtualAmiibo* InputSubsystem::GetVirtualAmiibo() const {
    return impl->virtual_amiibo.get();
}

VirtualGamepad* InputSubsystem::GetVirtualGamepad() {
    return impl->virtual_gamepad.get();
}

const VirtualGamepad* InputSubsystem::GetVirtualGamepad() const {
    return impl->virtual_gamepad.get();
}

std::vector<Common::ParamPackage> InputSubsystem::GetInputDevices() const {
    return impl->GetInputDevices();
}

AnalogMapping InputSubsystem::GetAnalogMappingForDevice(const Common::ParamPackage& device) const {
    return impl->GetAnalogMappingForDevice(device);
}

ButtonMapping InputSubsystem::GetButtonMappingForDevice(const Common::ParamPackage& device) const {
    return impl->GetButtonMappingForDevice(device);
}

MotionMapping InputSubsystem::GetMotionMappingForDevice(const Common::ParamPackage& device) const {
    return impl->GetMotionMappingForDevice(device);
}

Common::Input::ButtonNames InputSubsystem::GetButtonName(const Common::ParamPackage& params) const {
    return impl->GetButtonName(params);
}

bool InputSubsystem::IsController(const Common::ParamPackage& params) const {
    return impl->IsController(params);
}

bool InputSubsystem::IsStickInverted(const Common::ParamPackage& params) const {
    if (params.Has("axis_x") && params.Has("axis_y")) {
        return impl->IsStickInverted(params);
    }
    return false;
}

void InputSubsystem::ReloadInputDevices() {
    impl->udp_client.get()->ReloadSockets();
}

void InputSubsystem::BeginMapping(Polling::InputType type) {
    impl->BeginConfiguration();
    impl->mapping_factory->BeginMapping(type);
}

Common::ParamPackage InputSubsystem::GetNextInput() const {
    return impl->mapping_factory->GetNextInput();
}

void InputSubsystem::StopMapping() const {
    impl->EndConfiguration();
    impl->mapping_factory->StopMapping();
}

void InputSubsystem::PumpEvents() const {
    impl->PumpEvents();
}

std::string GenerateKeyboardParam(int key_code) {
    Common::ParamPackage param;
    param.Set("engine", "keyboard");
    param.Set("code", key_code);
    param.Set("toggle", false);
    return param.Serialize();
}

std::string GenerateAnalogParamFromKeys(int key_up, int key_down, int key_left, int key_right,
                                        int key_modifier, float modifier_scale) {
    Common::ParamPackage circle_pad_param{
        {"engine", "analog_from_button"},
        {"up", GenerateKeyboardParam(key_up)},
        {"down", GenerateKeyboardParam(key_down)},
        {"left", GenerateKeyboardParam(key_left)},
        {"right", GenerateKeyboardParam(key_right)},
        {"modifier", GenerateKeyboardParam(key_modifier)},
        {"modifier_scale", std::to_string(modifier_scale)},
    };
    return circle_pad_param.Serialize();
}
} // namespace InputCommon
