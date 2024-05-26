// SPDX-FileCopyrightText: 2018 Citra Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/logging/log.h"
#include "common/math_util.h"
#include "common/param_package.h"
#include "common/settings.h"
#include "common/thread.h"
#include "common/vector_math.h"
#include "input_common/drivers/sdl_driver.h"

namespace InputCommon {

namespace {
Common::UUID GetGUID(SDL_Joystick* joystick) {
    const SDL_JoystickGUID guid = SDL_GetJoystickGUID(joystick);
    std::array<u8, 16> data{};
    std::memcpy(data.data(), guid.data, sizeof(data));
    // Clear controller name crc
    std::memset(data.data() + 2, 0, sizeof(u16));
    return Common::UUID{data};
}

Common::UUID GetGamepadGUID(SDL_Gamepad* gamepad) {
    const auto guid = SDL_GetGamepadInstanceGUID(SDL_GetGamepadInstanceID(gamepad));
    std::array<u8, 16> data{};
    std::memcpy(data.data(), guid.data, sizeof(data));
    // Clear controller name crc
    std::memset(data.data() + 2, 0, sizeof(u16));
    return Common::UUID{data};
}
} // Anonymous namespace

// static int SDLEventWatcher(void* user_data, SDL_Event* event) {
//     auto* const sdl_state = static_cast<SDLDriver*>(user_data);
//
//     sdl_state->HandleGameControllerEvent(*event);
//
//     return 0;
// }

static int SDLGamepadEventWatcher(void* user_data, SDL_Event* event) {
    auto* const sdl_state = static_cast<SDLDriver*>(user_data);

    sdl_state->HandleGamepadEvent(*event);

    return 0;
}

class SDLJoystick {
public:
    SDLJoystick(Common::UUID guid_, int port_, SDL_Joystick* joystick, SDL_Gamepad* gamepad)
        : guid{guid_}, port{port_}, sdl_joystick{joystick, &SDL_CloseJoystick},
          sdl_controller{gamepad, &SDL_CloseGamepad} {
        EnableMotion();
    }

    void EnableMotion() {
        if (!sdl_controller) {
            return;
        }
        SDL_Gamepad* gamepad = sdl_controller.get();
        if (HasMotion()) {
            SDL_SetGamepadSensorEnabled(gamepad, SDL_SENSOR_ACCEL, SDL_FALSE);
            SDL_SetGamepadSensorEnabled(gamepad, SDL_SENSOR_GYRO, SDL_FALSE);
        }
        has_accel = SDL_GamepadHasSensor(gamepad, SDL_SENSOR_ACCEL) == SDL_TRUE;
        has_gyro = SDL_GamepadHasSensor(gamepad, SDL_SENSOR_GYRO) == SDL_TRUE;
        if (has_accel) {
            SDL_SetGamepadSensorEnabled(gamepad, SDL_SENSOR_ACCEL, SDL_TRUE);
        }
        if (has_gyro) {
            SDL_SetGamepadSensorEnabled(gamepad, SDL_SENSOR_GYRO, SDL_TRUE);
        }
    }

    bool HasMotion() const {
        return has_gyro || has_accel;
    }

    bool UpdateMotion(SDL_GamepadSensorEvent event) {
        constexpr float gravity_constant = 9.80665f;
        std::scoped_lock lock{mutex};
        const u64 time_difference = event.timestamp - last_motion_update;
        last_motion_update = event.timestamp;
        switch (event.sensor) {
        case SDL_SENSOR_ACCEL: {
            motion.accel_x = -event.data[0] / gravity_constant;
            motion.accel_y = event.data[2] / gravity_constant;
            motion.accel_z = -event.data[1] / gravity_constant;
            break;
        }
        case SDL_SENSOR_GYRO: {
            motion.gyro_x = event.data[0] / (Common::PI * 2);
            motion.gyro_y = -event.data[2] / (Common::PI * 2);
            motion.gyro_z = event.data[1] / (Common::PI * 2);
            break;
        }
        }

        // Ignore duplicated timestamps
        if (time_difference == 0) {
            return false;
        }

        // Motion data is invalid
        if (motion.accel_x == 0 && motion.gyro_x == 0 && motion.accel_y == 0 &&
            motion.gyro_y == 0 && motion.accel_z == 0 && motion.gyro_z == 0) {
            if (motion_error_count++ < 200) {
                return false;
            }
            // Try restarting the sensor
            motion_error_count = 0;
            EnableMotion();
            return false;
        }

        motion_error_count = 0;
        motion.delta_timestamp = time_difference * 1000;
        return true;
    }

    const BasicMotion& GetMotion() const {
        return motion;
    }

    bool RumblePlay(const Common::Input::VibrationStatus vibration) {
        constexpr u32 rumble_max_duration_ms = 2000;
        constexpr f32 low_start_sensitivity_limit = 140.0;
        constexpr f32 low_width_sensitivity_limit = 400.0;
        constexpr f32 high_start_sensitivity_limit = 200.0;
        constexpr f32 high_width_sensitivity_limit = 700.0;
        // Try to provide some feeling of the frequency by reducing the amplitude depending on it.
        f32 low_frequency_scale = 1.0;
        if (vibration.low_frequency > low_start_sensitivity_limit) {
            low_frequency_scale =
                std::max(1.0f - (vibration.low_frequency - low_start_sensitivity_limit) /
                                    low_width_sensitivity_limit,
                         0.3f);
        }
        f32 low_amplitude = vibration.low_amplitude * low_frequency_scale;

        f32 high_frequency_scale = 1.0;
        if (vibration.high_frequency > high_start_sensitivity_limit) {
            high_frequency_scale =
                std::max(1.0f - (vibration.high_frequency - high_start_sensitivity_limit) /
                                    high_width_sensitivity_limit,
                         0.3f);
        }
        f32 high_amplitude = vibration.high_amplitude * high_frequency_scale;

        if (sdl_controller) {
            return SDL_RumbleGamepad(sdl_controller.get(), static_cast<u16>(low_amplitude),
                                     static_cast<u16>(high_amplitude),
                                     rumble_max_duration_ms) != -1;
        } else if (sdl_joystick) {
            return SDL_RumbleJoystick(sdl_joystick.get(), static_cast<u16>(low_amplitude),
                                      static_cast<u16>(high_amplitude),
                                      rumble_max_duration_ms) != -1;
        }

        return false;
    }

    bool HasHDRumble() const {
        if (sdl_controller) {
            const auto type = SDL_GetGamepadType(sdl_controller.get());
            return (type == SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_PRO) ||
                   (type == SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_LEFT) ||
                   (type == SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_RIGHT) ||
                   (type == SDL_GAMEPAD_TYPE_PS5);
        }
        return false;
    }

    void EnableVibration(bool is_enabled) {
        has_vibration = is_enabled;
        is_vibration_tested = true;
    }

    bool HasVibration() const {
        return has_vibration;
    }

    bool IsVibrationTested() const {
        return is_vibration_tested;
    }

    /**
     * The Pad identifier of the joystick
     */
    const PadIdentifier GetPadIdentifier() const {
        return {
            .guid = guid,
            .port = static_cast<std::size_t>(port),
            .pad = 0,
        };
    }

    /**
     * The guid of the joystick
     */
    const Common::UUID& GetGUID() const {
        return guid;
    }

    /**
     * The number of joystick from the same type that were connected before this joystick
     */
    int GetPort() const {
        return port;
    }

    SDL_Joystick* GetSDLJoystick() const {
        return sdl_joystick.get();
    }

    SDL_Gamepad* GetSDLGamepad() const {
        return sdl_controller.get();
    }

    void SetSDLJoystick(SDL_Joystick* joystick, SDL_Gamepad* gamepad) {
        sdl_joystick.reset(joystick);
        sdl_controller.reset(gamepad);
    }

    bool IsJoyconLeft() const {
        const std::string controller_name = GetControllerName();
        if (std::strstr(controller_name.c_str(), "Joy-Con Left") != nullptr) {
            return true;
        }
        if (std::strstr(controller_name.c_str(), "Joy-Con (L)") != nullptr) {
            return true;
        }
        return false;
    }

    bool IsJoyconRight() const {
        const std::string controller_name = GetControllerName();
        if (std::strstr(controller_name.c_str(), "Joy-Con Right") != nullptr) {
            return true;
        }
        if (std::strstr(controller_name.c_str(), "Joy-Con (R)") != nullptr) {
            return true;
        }
        return false;
    }

    Common::Input::BatteryLevel GetBatteryLevel(int battery_level) {
        int level = -1;
        if (battery_level <= 5)
            level = 0;
        else if (battery_level <= 33)
            level = 1;
        else if (battery_level <= 66)
            level = 2;
        else
            level = 3;

        switch (level) {
        case 0:
            return Common::Input::BatteryLevel::Empty;
        case 1:
            return Common::Input::BatteryLevel::Low;
        case 2:
            return Common::Input::BatteryLevel::Medium;
        case 3:
            return Common::Input::BatteryLevel::Full;
        // case SDL_JOYSTICK_POWER_WIRED:
        //     return Common::Input::BatteryLevel::Charging;
        case -1:
        default:
            return Common::Input::BatteryLevel::None;
        }
    }

    std::string GetControllerName() const {
        if (sdl_controller) {
            switch (SDL_GetGamepadType(sdl_controller.get())) {
            case SDL_GAMEPAD_TYPE_XBOX360:
                return "Xbox 360 Controller";
            case SDL_GAMEPAD_TYPE_XBOXONE:
                return "Xbox One Controller";
            case SDL_GAMEPAD_TYPE_PS3:
                return "DualShock 3 Controller";
            case SDL_GAMEPAD_TYPE_PS4:
                return "DualShock 4 Controller";
            case SDL_GAMEPAD_TYPE_PS5:
                return "DualSense Controller";
            default:
                break;
            }
            const auto name = SDL_GetGamepadName(sdl_controller.get());
            if (name) {
                return name;
            }
        }

        if (sdl_joystick) {
            const auto name = SDL_GetJoystickName(sdl_joystick.get());
            if (name) {
                return name;
            }
        }

        return "Unknown";
    }

private:
    Common::UUID guid;
    int port;
    std::unique_ptr<SDL_Joystick, decltype(&SDL_CloseJoystick)> sdl_joystick;
    std::unique_ptr<SDL_Gamepad, decltype(&SDL_CloseGamepad)> sdl_controller;
    mutable std::mutex mutex;

    u64 last_motion_update{};
    std::size_t motion_error_count{};
    bool has_gyro{false};
    bool has_accel{false};
    bool has_vibration{false};
    bool is_vibration_tested{false};
    BasicMotion motion;
};

class SDLGamepad {
public:
    SDLGamepad(Common::UUID guid_, int port_, SDL_Gamepad* gamepad)
        : guid{guid_}, port{port_}, sdl_gamepad{gamepad, &SDL_CloseGamepad} {
        EnableMotion();
    }

    void EnableMotion() {
        if (!sdl_gamepad)
            return;

        SDL_Gamepad* gamepad = sdl_gamepad.get();
        if (HasMotion()) {
            SDL_SetGamepadSensorEnabled(gamepad, SDL_SENSOR_ACCEL, SDL_FALSE);
            SDL_SetGamepadSensorEnabled(gamepad, SDL_SENSOR_GYRO, SDL_FALSE);
        }

        has_accel = SDL_GamepadHasSensor(gamepad, SDL_SENSOR_ACCEL) == SDL_TRUE;
        has_gyro = SDL_GamepadHasSensor(gamepad, SDL_SENSOR_GYRO) == SDL_TRUE;

        if (has_accel)
            SDL_SetGamepadSensorEnabled(gamepad, SDL_SENSOR_ACCEL, SDL_TRUE);

        if (has_gyro)
            SDL_SetGamepadSensorEnabled(gamepad, SDL_SENSOR_GYRO, SDL_TRUE);
    }

    bool HasMotion() const {
        return has_gyro || has_accel;
    }

    bool UpdateMotion(SDL_GamepadSensorEvent event) {
        constexpr float gravity_constant = 9.80665f;
        std::scoped_lock lock{mutex};
        const u64 time_difference = event.timestamp - last_motion_update;
        last_motion_update = event.timestamp;
        switch (event.sensor) {
        case SDL_SENSOR_ACCEL: {
            motion.accel_x = -event.data[0] / gravity_constant;
            motion.accel_y = event.data[2] / gravity_constant;
            motion.accel_z = -event.data[1] / gravity_constant;
            break;
        }
        case SDL_SENSOR_GYRO: {
            motion.gyro_x = event.data[0] / (Common::PI * 2);
            motion.gyro_y = -event.data[2] / (Common::PI * 2);
            motion.gyro_z = event.data[1] / (Common::PI * 2);
            break;
        }
        }

        // Ignore duplicated timestamps
        if (time_difference == 0)
            return false;

        // Motion data is invalid
        if (motion.accel_x == 0 && motion.gyro_x == 0 && motion.accel_y == 0 &&
            motion.gyro_y == 0 && motion.accel_z == 0 && motion.gyro_z == 0) {
            if (motion_error_count++ < 200) {
                return false;
            }
            // Try restarting the sensor
            motion_error_count = 0;
            EnableMotion();
            return false;
        }

        motion_error_count = 0;
        motion.delta_timestamp = time_difference * 1000;
        return true;
    }

    const BasicMotion& GetMotion() const {
        return motion;
    }

    bool RumblePlay(const Common::Input::VibrationStatus vibration) {
        constexpr u32 rumble_max_duration_ms = 2000;
        constexpr f32 low_start_sensitivity_limit = 140.0;
        constexpr f32 low_width_sensitivity_limit = 400.0;
        constexpr f32 high_start_sensitivity_limit = 200.0;
        constexpr f32 high_width_sensitivity_limit = 700.0;
        // Try to provide some feeling of the frequency by reducing the amplitude depending on it.
        f32 low_frequency_scale = 1.0;
        if (vibration.low_frequency > low_start_sensitivity_limit)
            low_frequency_scale =
                std::max(1.0f - (vibration.low_frequency - low_start_sensitivity_limit) /
                                    low_width_sensitivity_limit,
                         0.3f);

        f32 low_amplitude = vibration.low_amplitude * low_frequency_scale;

        f32 high_frequency_scale = 1.0;
        if (vibration.high_frequency > high_start_sensitivity_limit)
            high_frequency_scale =
                std::max(1.0f - (vibration.high_frequency - high_start_sensitivity_limit) /
                                    high_width_sensitivity_limit,
                         0.3f);

        f32 high_amplitude = vibration.high_amplitude * high_frequency_scale;

        if (sdl_gamepad)
            return SDL_RumbleGamepad(sdl_gamepad.get(), static_cast<u16>(low_amplitude),
                                     static_cast<u16>(high_amplitude),
                                     rumble_max_duration_ms) != -1;

        return false;
    }

    bool HasHDRumble() const {
        if (sdl_gamepad) {
            const auto type = SDL_GetGamepadType(sdl_gamepad.get());
            return (type == SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_PRO) ||
                   (type == SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_LEFT) ||
                   (type == SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_RIGHT) ||
                   (type == SDL_GAMEPAD_TYPE_PS5);
        }
        return false;
    }

    void EnableVibration(bool is_enabled) {
        has_vibration = is_enabled;
        is_vibration_tested = true;
    }

    bool HasVibration() const {
        return has_vibration;
    }

    bool IsVibrationTested() const {
        return is_vibration_tested;
    }

    /**
     * The Pad identifier of the joystick
     */
    const PadIdentifier GetPadIdentifier() const {
        return {
            .guid = guid,
            .port = static_cast<std::size_t>(port),
            .pad = 0,
        };
    }

    /**
     * The guid of the joystick
     */
    const Common::UUID& GetGUID() const {
        return guid;
    }

    /**
     * The number of joystick from the same type that were connected before this joystick
     */
    int GetPort() const {
        return port;
    }

    SDL_Gamepad* GetSDLGamepad() const {
        return sdl_gamepad.get();
    }

    void SetSDLGamepad(SDL_Gamepad* gamepad) {
        sdl_gamepad.reset(gamepad);
    }

    bool IsJoyconLeft() const {
        const std::string controller_name = GetControllerName();
        if (std::strstr(controller_name.c_str(), "Joy-Con Left") != nullptr)
            return true;

        if (std::strstr(controller_name.c_str(), "Joy-Con (L)") != nullptr)
            return true;
        return false;
    }

    bool IsJoyconRight() const {
        const std::string controller_name = GetControllerName();
        if (std::strstr(controller_name.c_str(), "Joy-Con Right") != nullptr)
            return true;

        if (std::strstr(controller_name.c_str(), "Joy-Con (R)") != nullptr)
            return true;

        return false;
    }

    Common::Input::BatteryLevel GetBatteryLevel(int battery_level) {
        int level = -1;
        if (battery_level <= 5)
            level = 0;
        else if (battery_level <= 33)
            level = 1;
        else if (battery_level <= 66)
            level = 2;
        else
            level = 3;

        switch (level) {
        case 0:
            return Common::Input::BatteryLevel::Empty;
        case 1:
            return Common::Input::BatteryLevel::Low;
        case 2:
            return Common::Input::BatteryLevel::Medium;
        case 3:
            return Common::Input::BatteryLevel::Full;
        // case SDL_JOYSTICK_POWER_WIRED:
        //     return Common::Input::BatteryLevel::Charging;
        case -1:
        default:
            return Common::Input::BatteryLevel::None;
        }
    }

    std::string GetControllerName() const {
        if (sdl_gamepad) {
            switch (SDL_GetGamepadType(sdl_gamepad.get())) {
            case SDL_GAMEPAD_TYPE_XBOX360:
                return "Xbox 360 Controller";
            case SDL_GAMEPAD_TYPE_XBOXONE:
                return "Xbox One Controller";
            case SDL_GAMEPAD_TYPE_PS3:
                return "DualShock 3 Controller";
            case SDL_GAMEPAD_TYPE_PS4:
                return "DualShock 4 Controller";
            case SDL_GAMEPAD_TYPE_PS5:
                return "DualSense Controller";
            default:
                break;
            }
            const auto name = SDL_GetGamepadName(sdl_gamepad.get());
            if (name)
                return name;
        }

        return "Unknown";
    }

private:
    Common::UUID guid;
    int port;
    std::unique_ptr<SDL_Gamepad, decltype(&SDL_CloseGamepad)> sdl_gamepad;
    mutable std::mutex mutex;

    u64 last_motion_update{};
    std::size_t motion_error_count{};
    bool has_gyro{false};
    bool has_accel{false};
    bool has_vibration{false};
    bool is_vibration_tested{false};
    BasicMotion motion;
};

std::shared_ptr<SDLJoystick> SDLDriver::GetSDLJoystickByGUID(const Common::UUID& guid, int port) {
    std::scoped_lock lock{joystick_map_mutex};
    const auto it = joystick_map.find(guid);

    if (it != joystick_map.end()) {
        while (it->second.size() <= static_cast<std::size_t>(port)) {
            auto joystick = std::make_shared<SDLJoystick>(guid, static_cast<int>(it->second.size()),
                                                          nullptr, nullptr);
            it->second.emplace_back(std::move(joystick));
        }

        return it->second[static_cast<std::size_t>(port)];
    }

    auto joystick = std::make_shared<SDLJoystick>(guid, 0, nullptr, nullptr);

    return joystick_map[guid].emplace_back(std::move(joystick));
}

std::shared_ptr<SDLGamepad> SDLDriver::GetSDLGamepadByGUID(const Common::UUID& guid, int port) {
    std::scoped_lock lock{gamepad_map_mutex};
    const auto it = gamepad_map.find(guid);

    if (it != gamepad_map.end()) {
        while (it->second.size() <= static_cast<std::size_t>(port)) {
            auto gamepad =
                std::make_shared<SDLGamepad>(guid, static_cast<int>(it->second.size()), nullptr);
            it->second.emplace_back(std::move(gamepad));
        }

        return it->second[static_cast<std::size_t>(port)];
    }

    auto gamepad = std::make_shared<SDLGamepad>(guid, 0, nullptr);

    return gamepad_map[guid].emplace_back(std::move(gamepad));
}

std::shared_ptr<SDLJoystick> SDLDriver::GetSDLJoystickByGUID(const std::string& guid, int port) {
    return GetSDLJoystickByGUID(Common::UUID{guid}, port);
}

std::shared_ptr<SDLGamepad> SDLDriver::GetSDLGamepadByGUID(const std::string& guid, int port) {
    return GetSDLGamepadByGUID(Common::UUID{guid}, port);
}

std::shared_ptr<SDLJoystick> SDLDriver::GetSDLJoystickBySDLID(SDL_JoystickID sdl_id) {
    auto sdl_joystick = SDL_GetJoystickFromInstanceID(sdl_id);
    const auto guid = GetGUID(sdl_joystick);

    std::scoped_lock lock{joystick_map_mutex};
    const auto map_it = joystick_map.find(guid);

    if (map_it == joystick_map.end()) {
        return nullptr;
    }

    const auto vec_it = std::find_if(map_it->second.begin(), map_it->second.end(),
                                     [&sdl_joystick](const auto& joystick) {
                                         return joystick->GetSDLJoystick() == sdl_joystick;
                                     });

    if (vec_it == map_it->second.end()) {
        return nullptr;
    }

    return *vec_it;
}

std::shared_ptr<SDLGamepad> SDLDriver::GetSDLGamepadBySDLID(SDL_JoystickID sdl_id) {
    auto sdl_gamepad = SDL_GetGamepadFromInstanceID(sdl_id);
    const auto guid = GetGamepadGUID(sdl_gamepad);

    std::scoped_lock lock{gamepad_map_mutex};
    const auto map_it = gamepad_map.find(guid);

    if (map_it == gamepad_map.end()) {
        return nullptr;
    }

    const auto vec_it = std::find_if(
        map_it->second.begin(), map_it->second.end(),
        [&sdl_gamepad](const auto& gamepad) { return gamepad->GetSDLGamepad() == sdl_gamepad; });

    if (vec_it == map_it->second.end()) {
        return nullptr;
    }

    return *vec_it;
}

void SDLDriver::InitJoystick(int joystick_index) {
    // SDL_Joystick* sdl_joystick = SDL_OpenJoystick(joystick_index);
    // LOG_ERROR(Input, "error={}", SDL_GetError());
    SDL_Gamepad* sdl_gamepad = nullptr;

    if (SDL_IsGamepad(joystick_index)) {
        sdl_gamepad = SDL_OpenGamepad(joystick_index);
    }

    // if (!sdl_joystick) {
    //     LOG_ERROR(Input, "Failed to open joystick {}, error={}", joystick_index, SDL_GetError());
    //     return;
    // }

    auto gamepad = SDL_GetGamepadJoystick(sdl_gamepad);
    const auto guid = GetGUID(gamepad);

    if (Settings::values.enable_joycon_driver) {
        if (guid.uuid[5] == 0x05 && guid.uuid[4] == 0x7e &&
            (guid.uuid[8] == 0x06 || guid.uuid[8] == 0x07)) {
            LOG_WARNING(Input, "Preferring joycon driver for device index {}", joystick_index);
            CloseJoystick(gamepad);
            return;
        }
    }

    if (Settings::values.enable_procon_driver) {
        if (guid.uuid[5] == 0x05 && guid.uuid[4] == 0x7e && guid.uuid[8] == 0x09) {
            LOG_WARNING(Input, "Preferring joycon driver for device index {}", joystick_index);
            CloseJoystick(gamepad);
            return;
        }
    }

    std::scoped_lock lock{joystick_map_mutex};
    if (joystick_map.find(guid) == joystick_map.end()) {
        auto joystick = std::make_shared<SDLJoystick>(guid, 0, gamepad, sdl_gamepad);
        PreSetController(joystick->GetPadIdentifier());
        joystick->EnableMotion();
        joystick_map[guid].emplace_back(std::move(joystick));
        return;
    }

    auto& joystick_guid_list = joystick_map[guid];
    const auto joystick_it =
        std::find_if(joystick_guid_list.begin(), joystick_guid_list.end(),
                     [](const auto& joystick) { return !joystick->GetSDLJoystick(); });

    if (joystick_it != joystick_guid_list.end()) {
        (*joystick_it)->SetSDLJoystick(gamepad, sdl_gamepad);
        (*joystick_it)->EnableMotion();
        return;
    }

    const int port = static_cast<int>(joystick_guid_list.size());
    auto joystick = std::make_shared<SDLJoystick>(guid, port, gamepad, sdl_gamepad);
    PreSetController(joystick->GetPadIdentifier());
    joystick->EnableMotion();
    joystick_guid_list.emplace_back(std::move(joystick));
}

void SDLDriver::InitGamepad(int gamepad_index) {
    SDL_Gamepad* sdl_gamepad = nullptr;
    // if (SDL_IsGamepad(gamepad_index)) {
    sdl_gamepad = SDL_OpenGamepad(gamepad_index);
    LOG_ERROR(Input, "SDL_OpenGamepad.error={}, name={}", SDL_GetError(),
              SDL_GetGamepadName(sdl_gamepad));
    //}

    if (!sdl_gamepad) {
        LOG_ERROR(Input, "Failed to open gamepad {}, error={}", gamepad_index, SDL_GetError());
        return;
    }

    const auto guid = GetGamepadGUID(sdl_gamepad);

    if (Settings::values.enable_joycon_driver) {
        if (guid.uuid[5] == 0x05 && guid.uuid[4] == 0x7e &&
            (guid.uuid[8] == 0x06 || guid.uuid[8] == 0x07)) {
            LOG_WARNING(Input, "Preferring joycon driver for device index {}", gamepad_index);
            CloseGamepad(sdl_gamepad);
            return;
        }
    }

    if (Settings::values.enable_procon_driver) {
        if (guid.uuid[5] == 0x05 && guid.uuid[4] == 0x7e && guid.uuid[8] == 0x09) {
            LOG_WARNING(Input, "Preferring joycon driver for device index {}", gamepad_index);
            CloseGamepad(sdl_gamepad);
            return;
        }
    }

    std::scoped_lock lock{gamepad_map_mutex};
    if (gamepad_map.find(guid) == gamepad_map.end()) {
        auto gamepad = std::make_shared<SDLGamepad>(guid, 0, sdl_gamepad);
        PreSetController(gamepad->GetPadIdentifier());
        gamepad->EnableMotion();
        gamepad_map[guid].emplace_back(std::move(gamepad));
        return;
    }

    auto& gamepad_guid_list = gamepad_map[guid];
    const auto gamepad_it =
        std::find_if(gamepad_guid_list.begin(), gamepad_guid_list.end(),
                     [](const auto& gamepad) { return !gamepad->GetSDLGamepad(); });

    if (gamepad_it != gamepad_guid_list.end()) {
        (*gamepad_it)->SetSDLGamepad(sdl_gamepad);
        (*gamepad_it)->EnableMotion();
        return;
    }

    const int port = static_cast<int>(gamepad_guid_list.size());
    auto gamepad = std::make_shared<SDLGamepad>(guid, port, sdl_gamepad);
    PreSetController(gamepad->GetPadIdentifier());
    gamepad->EnableMotion();
    gamepad_guid_list.emplace_back(std::move(gamepad));
}

void SDLDriver::CloseJoystick(SDL_Joystick* sdl_joystick) {
    const auto guid = GetGUID(sdl_joystick);

    std::scoped_lock lock{joystick_map_mutex};
    // This call to guid is safe since the joystick is guaranteed to be in the map
    const auto& joystick_guid_list = joystick_map[guid];
    const auto joystick_it = std::find_if(joystick_guid_list.begin(), joystick_guid_list.end(),
                                          [&sdl_joystick](const auto& joystick) {
                                              return joystick->GetSDLJoystick() == sdl_joystick;
                                          });

    if (joystick_it != joystick_guid_list.end()) {
        (*joystick_it)->SetSDLJoystick(nullptr, nullptr);
    }
}

void SDLDriver::CloseGamepad(SDL_Gamepad* sdl_gamepad) {
    const auto guid = GetGamepadGUID(sdl_gamepad);

    std::scoped_lock lock{gamepad_map_mutex};
    // This call to guid is safe since the joystick is guaranteed to be in the map
    const auto& gamepad_guid_list = gamepad_map[guid];
    const auto gamepad_it = std::find_if(
        gamepad_guid_list.begin(), gamepad_guid_list.end(),
        [&sdl_gamepad](const auto& gamepad) { return gamepad->GetSDLGamepad() == sdl_gamepad; });

    if (gamepad_it != gamepad_guid_list.end()) {
        (*gamepad_it)->SetSDLGamepad(nullptr);
    }
}

void SDLDriver::PumpEvents() const {
    if (initialized) {
        SDL_PumpEvents();
    }
}

void SDLDriver::HandleGameControllerEvent(const SDL_Event& event) {
    switch (event.type) {
    case SDL_EVENT_JOYSTICK_BUTTON_UP: {
        if (const auto joystick = GetSDLJoystickBySDLID(event.jbutton.which)) {
            const PadIdentifier identifier = joystick->GetPadIdentifier();
            SetButton(identifier, event.jbutton.button, false);
        }
        break;
    }
    case SDL_EVENT_JOYSTICK_BUTTON_DOWN: {
        if (const auto joystick = GetSDLJoystickBySDLID(event.jbutton.which)) {
            const PadIdentifier identifier = joystick->GetPadIdentifier();
            SetButton(identifier, event.jbutton.button, true);
        }
        break;
    }
    case SDL_EVENT_JOYSTICK_HAT_MOTION: {
        if (const auto joystick = GetSDLJoystickBySDLID(event.jhat.which)) {
            const PadIdentifier identifier = joystick->GetPadIdentifier();
            SetHatButton(identifier, event.jhat.hat, event.jhat.value);
        }
        break;
    }
    case SDL_EVENT_JOYSTICK_AXIS_MOTION: {
        if (const auto joystick = GetSDLJoystickBySDLID(event.jaxis.which)) {
            const PadIdentifier identifier = joystick->GetPadIdentifier();
            SetAxis(identifier, event.jaxis.axis, event.jaxis.value / 32767.0f);
        }
        break;
    }
    case SDL_EVENT_GAMEPAD_SENSOR_UPDATE: {
        if (auto joystick = GetSDLJoystickBySDLID(event.gsensor.which)) {
            if (joystick->UpdateMotion(event.gsensor)) {
                const PadIdentifier identifier = joystick->GetPadIdentifier();
                SetMotion(identifier, 0, joystick->GetMotion());
            }
        }
        break;
    }
    case SDL_EVENT_JOYSTICK_BATTERY_UPDATED: {
        if (auto joystick = GetSDLJoystickBySDLID(event.jbattery.which)) {
            const PadIdentifier identifier = joystick->GetPadIdentifier();
            int level = -1;
            SDL_GetJoystickPowerInfo(joystick.get()->GetSDLJoystick(), &level);
            SetBattery(identifier, joystick->GetBatteryLevel(level));
        }
        break;
    }
    case SDL_EVENT_JOYSTICK_REMOVED:
    case SDL_EVENT_GAMEPAD_REMOVED:
        if (event.type == SDL_EVENT_GAMEPAD_REMOVED) {
            LOG_DEBUG(Input, "Controller removed with Instance_ID {}", event.gdevice.which);
            CloseJoystick(
                SDL_GetGamepadJoystick(SDL_GetGamepadFromInstanceID(event.gdevice.which)));
        } else {
            LOG_DEBUG(Input, "Controller removed with Instance_ID {}", event.jdevice.which);
            CloseJoystick(SDL_GetJoystickFromInstanceID(event.jdevice.which));
        }
        break;
    case SDL_EVENT_JOYSTICK_ADDED:
    case SDL_EVENT_GAMEPAD_ADDED:
        if (event.type == SDL_EVENT_GAMEPAD_ADDED) {
            LOG_DEBUG(Input, "Controller connected with device index {}", event.gdevice.which);
            InitJoystick(event.gdevice.which);
        } else {
            LOG_DEBUG(Input, "Controller connected with device index {}", event.jdevice.which);
            InitJoystick(event.jdevice.which);
        }
        break;
    }
}

void SDLDriver::HandleGamepadEvent(const SDL_Event& event) {
    switch (event.type) {
    case SDL_EVENT_GAMEPAD_BUTTON_UP: {
        if (const auto gamepad = GetSDLGamepadBySDLID(event.gbutton.which)) {
            const PadIdentifier identifier = gamepad->GetPadIdentifier();
            SetButton(identifier, event.gbutton.button, false);
        }
        break;
    }
    case SDL_EVENT_GAMEPAD_BUTTON_DOWN: {
        if (const auto gamepad = GetSDLGamepadBySDLID(event.gbutton.which)) {
            const PadIdentifier identifier = gamepad->GetPadIdentifier();
            SetButton(identifier, event.gbutton.button, true);
        }
        break;
    }
    case SDL_EVENT_JOYSTICK_HAT_MOTION: {
        if (const auto gamepad = GetSDLGamepadBySDLID(event.gdevice.which)) {
            const PadIdentifier identifier = gamepad->GetPadIdentifier();
            SetHatButton(identifier, event.jhat.hat, event.jhat.value);
        }
        break;
    }
    case SDL_EVENT_JOYSTICK_AXIS_MOTION: {
        if (const auto gamepad = GetSDLGamepadBySDLID(event.gdevice.which)) {
            const PadIdentifier identifier = gamepad->GetPadIdentifier();
            SetAxis(identifier, event.jaxis.axis, event.jaxis.value / 32767.0f);
        }
        break;
    }
    case SDL_EVENT_GAMEPAD_SENSOR_UPDATE: {
        if (auto gamepad = GetSDLGamepadBySDLID(event.gsensor.which)) {
            if (gamepad->UpdateMotion(event.gsensor)) {
                const PadIdentifier identifier = gamepad->GetPadIdentifier();
                SetMotion(identifier, 0, gamepad->GetMotion());
            }
        }
        break;
    }
    case SDL_EVENT_JOYSTICK_BATTERY_UPDATED: {
        if (auto gamepad = GetSDLGamepadBySDLID(event.jbattery.which)) {
            const PadIdentifier identifier = gamepad->GetPadIdentifier();
            int level = -1;
            SDL_GetGamepadPowerInfo(gamepad.get()->GetSDLGamepad(), &level);
            SetBattery(identifier, gamepad->GetBatteryLevel(level));
        }
        break;
    }
    case SDL_EVENT_GAMEPAD_REMOVED:
        LOG_DEBUG(Input, "Controller removed with Instance_ID {}", event.gdevice.which);
        CloseGamepad(SDL_GetGamepadFromInstanceID(event.gdevice.which));
        break;
    case SDL_EVENT_GAMEPAD_ADDED:
        LOG_DEBUG(Input, "Controller connected with device index {}", event.gdevice.which);
        InitGamepad(event.gdevice.which);
        break;
    }
}

void SDLDriver::CloseJoysticks() {
    std::scoped_lock lock{joystick_map_mutex};
    joystick_map.clear();
}

void SDLDriver::CloseGamepads() {
    std::scoped_lock lock{gamepad_map_mutex};
    gamepad_map.clear();
}

SDLDriver::SDLDriver(std::string input_engine_) : InputEngine(std::move(input_engine_)) {
    // Set our application name. Currently passed to DBus by SDL and visible to the user through
    // their desktop environment.
    SDL_SetHint(SDL_HINT_APP_NAME, "sudachi");

    if (!Settings::values.enable_raw_input) {
        // Disable raw input. When enabled this setting causes SDL to die when a web applet opens
        SDL_SetHint(SDL_HINT_JOYSTICK_RAWINPUT, "0");
    }

    // Prevent SDL from adding undesired axis
    SDL_SetHint("SDL_HINT_ACCELEROMETER_AS_JOYSTICK", "0");

    // Enable HIDAPI rumble. This prevents SDL from disabling motion on PS4 and PS5 controllers
    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_PS4_RUMBLE, "1");
    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_PS5_RUMBLE, "1");
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");

    // Disable hidapi drivers for joycon controllers when the custom joycon driver is enabled
    if (Settings::values.enable_joycon_driver) {
        SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_JOY_CONS, "0");
    } else {
        SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_JOY_CONS, "1");
        SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_JOYCON_HOME_LED, "0");
        SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_COMBINE_JOY_CONS, "0");
        SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_VERTICAL_JOY_CONS, "1");
    }

    // Disable hidapi drivers for pro controllers when the custom joycon driver is enabled
    if (Settings::values.enable_procon_driver) {
        SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_SWITCH, "0");
    } else {
        SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_SWITCH, "1");
        SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_SWITCH_HOME_LED, "0");
    }

    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_SWITCH_PLAYER_LED, "1");
    // Share the same button mapping with non-Nintendo controllers
    SDL_SetHint("SDL_HINT_GAMECONTROLLER_USE_BUTTON_LABELS", "0");

    // Disable hidapi driver for xbox. Already default on Windows, this causes conflict with native
    // driver on Linux.
    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_XBOX, "0");

    // If the frontend is going to manage the event loop, then we don't start one here
    start_thread = SDL_WasInit(SDL_INIT_JOYSTICK | SDL_INIT_GAMEPAD) == 0;
    if (start_thread && SDL_Init(SDL_INIT_JOYSTICK | SDL_INIT_GAMEPAD) < 0) {
        LOG_CRITICAL(Input, "SDL_Init failed with: {}", SDL_GetError());
        return;
    }

    // SDL_AddEventWatch(&SDLEventWatcher, this);
    SDL_AddEventWatch(&SDLGamepadEventWatcher, this);

    initialized = true;
    if (start_thread) {
        vibration_thread = std::thread([this] {
            Common::SetCurrentThreadName("SDL_Vibration");
            using namespace std::chrono_literals;
            while (initialized) {
                SendVibrations();
                std::this_thread::sleep_for(10ms);
            }
        });
    }
    // Because the events for joystick connection happens before we have our event watcher added, we
    // can just open all the joysticks right here
    // int count = 0;
    // SDL_GetJoysticks(&count);
    // for (int i = 0; i < count; ++i) {
    //     InitJoystick(i);
    // }

    // int count = 0;
    // SDL_GetGamepads(&count);
    // LOG_ERROR(Input, "SDL_Gamepads.count={}", count);
    // for (int i = 0; i < count; i++) {
    //     InitGamepad(i);
    // }
}

SDLDriver::~SDLDriver() {
    // CloseJoysticks();
    CloseGamepads();
    // SDL_DelEventWatch(&SDLEventWatcher, this);
    SDL_DelEventWatch(&SDLGamepadEventWatcher, this);

    initialized = false;
    if (start_thread) {
        vibration_thread.join();
        SDL_QuitSubSystem(SDL_INIT_JOYSTICK | SDL_INIT_GAMEPAD);
    }
}

std::vector<Common::ParamPackage> SDLDriver::GetInputDevices() const {
    std::vector<Common::ParamPackage> devices;
    std::unordered_map<int, std::shared_ptr<SDLJoystick>> joycon_pairs;
    for (const auto& [key, value] : joystick_map) {
        for (const auto& joystick : value) {
            if (!joystick->GetSDLJoystick()) {
                continue;
            }
            const std::string name =
                fmt::format("{} {}", joystick->GetControllerName(), joystick->GetPort());
            devices.emplace_back(Common::ParamPackage{
                {"engine", GetEngineName()},
                {"display", std::move(name)},
                {"guid", joystick->GetGUID().RawString()},
                {"port", std::to_string(joystick->GetPort())},
            });
            if (joystick->IsJoyconLeft()) {
                joycon_pairs.insert_or_assign(joystick->GetPort(), joystick);
            }
        }
    }

    // Add dual controllers
    for (const auto& [key, value] : joystick_map) {
        for (const auto& joystick : value) {
            if (joystick->IsJoyconRight()) {
                if (!joycon_pairs.contains(joystick->GetPort())) {
                    continue;
                }
                const auto joystick2 = joycon_pairs.at(joystick->GetPort());

                const std::string name =
                    fmt::format("{} {}", "Nintendo Dual Joy-Con", joystick->GetPort());
                devices.emplace_back(Common::ParamPackage{
                    {"engine", GetEngineName()},
                    {"display", std::move(name)},
                    {"guid", joystick->GetGUID().RawString()},
                    {"guid2", joystick2->GetGUID().RawString()},
                    {"port", std::to_string(joystick->GetPort())},
                });
            }
        }
    }
    return devices;
}

Common::Input::DriverResult SDLDriver::SetVibration(
    const PadIdentifier& identifier, const Common::Input::VibrationStatus& vibration) {
    const auto joystick =
        GetSDLJoystickByGUID(identifier.guid.RawString(), static_cast<int>(identifier.port));
    const auto process_amplitude_exp = [](f32 amplitude, f32 factor) {
        return (amplitude + std::pow(amplitude, factor)) * 0.5f * 0xFFFF;
    };

    // Default exponential curve for rumble
    f32 factor = 0.35f;

    // If vibration is set as a linear output use a flatter value
    if (vibration.type == Common::Input::VibrationAmplificationType::Linear) {
        factor = 0.5f;
    }

    // Amplitude for HD rumble needs no modification
    if (joystick->HasHDRumble()) {
        factor = 1.0f;
    }

    const Common::Input::VibrationStatus new_vibration{
        .low_amplitude = process_amplitude_exp(vibration.low_amplitude, factor),
        .low_frequency = vibration.low_frequency,
        .high_amplitude = process_amplitude_exp(vibration.high_amplitude, factor),
        .high_frequency = vibration.high_frequency,
        .type = Common::Input::VibrationAmplificationType::Exponential,
    };

    vibration_queue.Push(VibrationRequest{
        .identifier = identifier,
        .vibration = new_vibration,
    });

    return Common::Input::DriverResult::Success;
}

bool SDLDriver::IsVibrationEnabled(const PadIdentifier& identifier) {
    const auto joystick =
        GetSDLJoystickByGUID(identifier.guid.RawString(), static_cast<int>(identifier.port));

    static constexpr Common::Input::VibrationStatus test_vibration{
        .low_amplitude = 1,
        .low_frequency = 160.0f,
        .high_amplitude = 1,
        .high_frequency = 320.0f,
        .type = Common::Input::VibrationAmplificationType::Exponential,
    };

    static constexpr Common::Input::VibrationStatus zero_vibration{
        .low_amplitude = 0,
        .low_frequency = 160.0f,
        .high_amplitude = 0,
        .high_frequency = 320.0f,
        .type = Common::Input::VibrationAmplificationType::Exponential,
    };

    if (joystick->IsVibrationTested()) {
        return joystick->HasVibration();
    }

    // First vibration might fail
    joystick->RumblePlay(test_vibration);

    // Wait for about 15ms to ensure the controller is ready for the stop command
    std::this_thread::sleep_for(std::chrono::milliseconds(15));

    if (!joystick->RumblePlay(zero_vibration)) {
        joystick->EnableVibration(false);
        return false;
    }

    joystick->EnableVibration(true);
    return true;
}

void SDLDriver::SendVibrations() {
    std::vector<VibrationRequest> filtered_vibrations{};
    while (!vibration_queue.Empty()) {
        VibrationRequest request;
        vibration_queue.Pop(request);
        const auto joystick = GetSDLJoystickByGUID(request.identifier.guid.RawString(),
                                                   static_cast<int>(request.identifier.port));
        const auto it = std::find_if(filtered_vibrations.begin(), filtered_vibrations.end(),
                                     [request](VibrationRequest vibration) {
                                         return vibration.identifier == request.identifier;
                                     });
        if (it == filtered_vibrations.end()) {
            filtered_vibrations.push_back(std::move(request));
            continue;
        }
        *it = request;
    }

    for (const auto& vibration : filtered_vibrations) {
        const auto joystick = GetSDLJoystickByGUID(vibration.identifier.guid.RawString(),
                                                   static_cast<int>(vibration.identifier.port));
        joystick->RumblePlay(vibration.vibration);
    }
}

Common::ParamPackage SDLDriver::BuildAnalogParamPackageForButton(int port, const Common::UUID& guid,
                                                                 s32 axis, float value) const {
    Common::ParamPackage params{};
    params.Set("engine", GetEngineName());
    params.Set("port", port);
    params.Set("guid", guid.RawString());
    params.Set("axis", axis);
    params.Set("threshold", "0.5");
    params.Set("invert", value < 0 ? "-" : "+");
    return params;
}

Common::ParamPackage SDLDriver::BuildButtonParamPackageForButton(int port, const Common::UUID& guid,
                                                                 s32 button) const {
    Common::ParamPackage params{};
    params.Set("engine", GetEngineName());
    params.Set("port", port);
    params.Set("guid", guid.RawString());
    params.Set("button", button);
    return params;
}

Common::ParamPackage SDLDriver::BuildHatParamPackageForButton(int port, const Common::UUID& guid,
                                                              s32 hat, u8 value) const {
    Common::ParamPackage params{};
    params.Set("engine", GetEngineName());
    params.Set("port", port);
    params.Set("guid", guid.RawString());
    params.Set("hat", hat);
    params.Set("direction", GetHatButtonName(value));
    return params;
}

Common::ParamPackage SDLDriver::BuildMotionParam(int port, const Common::UUID& guid) const {
    Common::ParamPackage params{};
    params.Set("engine", GetEngineName());
    params.Set("motion", 0);
    params.Set("port", port);
    params.Set("guid", guid.RawString());
    return params;
}

Common::ParamPackage SDLDriver::BuildParamPackageForBinding(
    int port, const Common::UUID& guid, const SDL_GamepadBinding& binding) const {
    switch (binding.input_type) {
    case SDL_GAMEPAD_BINDTYPE_NONE:
        break;
    case SDL_GAMEPAD_BINDTYPE_AXIS:
        return BuildAnalogParamPackageForButton(port, guid, binding.input.axis.axis);
    case SDL_GAMEPAD_BINDTYPE_BUTTON:
        return BuildButtonParamPackageForButton(port, guid, binding.input.button);
    case SDL_GAMEPAD_BINDTYPE_HAT:
        return BuildHatParamPackageForButton(port, guid, binding.input.hat.hat,
                                             static_cast<u8>(binding.input.hat.hat_mask));
    }
    return {};
}

Common::ParamPackage SDLDriver::BuildParamPackageForAnalog(PadIdentifier identifier, int axis_x,
                                                           int axis_y, float offset_x,
                                                           float offset_y) const {
    Common::ParamPackage params;
    params.Set("engine", GetEngineName());
    params.Set("port", static_cast<int>(identifier.port));
    params.Set("guid", identifier.guid.RawString());
    params.Set("axis_x", axis_x);
    params.Set("axis_y", axis_y);
    params.Set("offset_x", offset_x);
    params.Set("offset_y", offset_y);
    params.Set("invert_x", "+");
    params.Set("invert_y", "+");
    return params;
}

ButtonMapping SDLDriver::GetButtonMappingForDevice(const Common::ParamPackage& params) {
    if (!params.Has("guid") || !params.Has("port")) {
        return {};
    }
    const auto joystick = GetSDLJoystickByGUID(params.Get("guid", ""), params.Get("port", 0));

    auto* gamepad = joystick->GetSDLGamepad();
    if (gamepad == nullptr) {
        return {};
    }

    // This list is missing ZL/ZR since those are not considered buttons in SDL Gamepad.
    // We will add those afterwards
    ButtonBindings switch_to_sdl_button;

    switch_to_sdl_button = GetDefaultButtonBinding(joystick);

    // Add the missing bindings for ZL/ZR
    static constexpr ZButtonBindings switch_to_sdl_axis{{
        {Settings::NativeButton::ZL, SDL_GAMEPAD_AXIS_LEFT_TRIGGER},
        {Settings::NativeButton::ZR, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER},
    }};

    // Parameters contain two joysticks return dual
    if (params.Has("guid2")) {
        const auto joystick2 = GetSDLJoystickByGUID(params.Get("guid2", ""), params.Get("port", 0));

        if (joystick2->GetSDLGamepad() != nullptr) {
            return GetDualControllerMapping(joystick, joystick2, switch_to_sdl_button,
                                            switch_to_sdl_axis);
        }
    }

    return GetSingleControllerMapping(joystick, switch_to_sdl_button, switch_to_sdl_axis);
}

ButtonBindings SDLDriver::GetDefaultButtonBinding(
    const std::shared_ptr<SDLJoystick>& joystick) const {
    // Default SL/SR mapping for other controllers
    auto sll_button = SDL_GAMEPAD_BUTTON_LEFT_SHOULDER;
    auto srl_button = SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER;
    auto slr_button = SDL_GAMEPAD_BUTTON_LEFT_SHOULDER;
    auto srr_button = SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER;

    if (joystick->IsJoyconLeft()) {
        sll_button = SDL_GAMEPAD_BUTTON_LEFT_PADDLE1;
        srl_button = SDL_GAMEPAD_BUTTON_LEFT_PADDLE2;
    }
    if (joystick->IsJoyconRight()) {
        slr_button = SDL_GAMEPAD_BUTTON_RIGHT_PADDLE2;
        srr_button = SDL_GAMEPAD_BUTTON_RIGHT_PADDLE1;
    }

    return {
        std::pair{Settings::NativeButton::A, SDL_GAMEPAD_BUTTON_EAST},
        {Settings::NativeButton::B, SDL_GAMEPAD_BUTTON_SOUTH},
        {Settings::NativeButton::X, SDL_GAMEPAD_BUTTON_NORTH},
        {Settings::NativeButton::Y, SDL_GAMEPAD_BUTTON_WEST},
        {Settings::NativeButton::LStick, SDL_GAMEPAD_BUTTON_LEFT_STICK},
        {Settings::NativeButton::RStick, SDL_GAMEPAD_BUTTON_RIGHT_STICK},
        {Settings::NativeButton::L, SDL_GAMEPAD_BUTTON_LEFT_SHOULDER},
        {Settings::NativeButton::R, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER},
        {Settings::NativeButton::Plus, SDL_GAMEPAD_BUTTON_START},
        {Settings::NativeButton::Minus, SDL_GAMEPAD_BUTTON_BACK},
        {Settings::NativeButton::DLeft, SDL_GAMEPAD_BUTTON_DPAD_LEFT},
        {Settings::NativeButton::DUp, SDL_GAMEPAD_BUTTON_DPAD_UP},
        {Settings::NativeButton::DRight, SDL_GAMEPAD_BUTTON_DPAD_RIGHT},
        {Settings::NativeButton::DDown, SDL_GAMEPAD_BUTTON_DPAD_DOWN},
        {Settings::NativeButton::SLLeft, sll_button},
        {Settings::NativeButton::SRLeft, srl_button},
        {Settings::NativeButton::SLRight, slr_button},
        {Settings::NativeButton::SRRight, srr_button},
        {Settings::NativeButton::Home, SDL_GAMEPAD_BUTTON_GUIDE},
        {Settings::NativeButton::Screenshot, SDL_GAMEPAD_BUTTON_MISC1},
    };
}

ButtonBindings SDLDriver::GetDefaultButtonBinding(
    const std::shared_ptr<SDLGamepad>& gamepad) const {
    // Default SL/SR mapping for other controllers
    auto sll_button = SDL_GAMEPAD_BUTTON_LEFT_SHOULDER;
    auto srl_button = SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER;
    auto slr_button = SDL_GAMEPAD_BUTTON_LEFT_SHOULDER;
    auto srr_button = SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER;

    if (gamepad->IsJoyconLeft()) {
        sll_button = SDL_GAMEPAD_BUTTON_LEFT_PADDLE1;
        srl_button = SDL_GAMEPAD_BUTTON_LEFT_PADDLE2;
    }
    if (gamepad->IsJoyconRight()) {
        slr_button = SDL_GAMEPAD_BUTTON_RIGHT_PADDLE2;
        srr_button = SDL_GAMEPAD_BUTTON_RIGHT_PADDLE1;
    }

    return {
        std::pair{Settings::NativeButton::A, SDL_GAMEPAD_BUTTON_EAST},
        {Settings::NativeButton::B, SDL_GAMEPAD_BUTTON_SOUTH},
        {Settings::NativeButton::X, SDL_GAMEPAD_BUTTON_NORTH},
        {Settings::NativeButton::Y, SDL_GAMEPAD_BUTTON_WEST},
        {Settings::NativeButton::LStick, SDL_GAMEPAD_BUTTON_LEFT_STICK},
        {Settings::NativeButton::RStick, SDL_GAMEPAD_BUTTON_RIGHT_STICK},
        {Settings::NativeButton::L, SDL_GAMEPAD_BUTTON_LEFT_SHOULDER},
        {Settings::NativeButton::R, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER},
        {Settings::NativeButton::Plus, SDL_GAMEPAD_BUTTON_START},
        {Settings::NativeButton::Minus, SDL_GAMEPAD_BUTTON_BACK},
        {Settings::NativeButton::DLeft, SDL_GAMEPAD_BUTTON_DPAD_LEFT},
        {Settings::NativeButton::DUp, SDL_GAMEPAD_BUTTON_DPAD_UP},
        {Settings::NativeButton::DRight, SDL_GAMEPAD_BUTTON_DPAD_RIGHT},
        {Settings::NativeButton::DDown, SDL_GAMEPAD_BUTTON_DPAD_DOWN},
        {Settings::NativeButton::SLLeft, sll_button},
        {Settings::NativeButton::SRLeft, srl_button},
        {Settings::NativeButton::SLRight, slr_button},
        {Settings::NativeButton::SRRight, srr_button},
        {Settings::NativeButton::Home, SDL_GAMEPAD_BUTTON_GUIDE},
        {Settings::NativeButton::Screenshot, SDL_GAMEPAD_BUTTON_MISC1},
    };
}

ButtonMapping SDLDriver::GetSingleControllerMapping(
    const std::shared_ptr<SDLJoystick>& joystick, const ButtonBindings& switch_to_sdl_button,
    const ZButtonBindings& switch_to_sdl_axis) const {
    ButtonMapping mapping;
    mapping.reserve(switch_to_sdl_button.size() + switch_to_sdl_axis.size());
    auto* controller = joystick->GetSDLGamepad();

    for (const auto& [switch_button, sdl_button] : switch_to_sdl_button) {
        int count = 0;
        const auto binding = SDL_GetGamepadBindings(controller, &count);

        // const auto& binding = SDL_GameControllerGetBindForButton(controller, sdl_button);
        mapping.insert_or_assign(
            switch_button,
            BuildParamPackageForBinding(joystick->GetPort(), joystick->GetGUID(), **binding));
    }
    for (const auto& [switch_button, sdl_axis] : switch_to_sdl_axis) {
        int count = 0;
        const auto binding = SDL_GetGamepadBindings(controller, &count);

        // const auto& binding = SDL_GameControllerGetBindForAxis(controller, sdl_axis);
        mapping.insert_or_assign(
            switch_button,
            BuildParamPackageForBinding(joystick->GetPort(), joystick->GetGUID(), **binding));
    }

    return mapping;
}

ButtonMapping SDLDriver::GetSingleControllerMapping(
    const std::shared_ptr<SDLGamepad>& gamepad, const ButtonBindings& switch_to_sdl_button,
    const ZButtonBindings& switch_to_sdl_axis) const {
    ButtonMapping mapping;
    mapping.reserve(switch_to_sdl_button.size() + switch_to_sdl_axis.size());
    auto* controller = gamepad->GetSDLGamepad();

    for (const auto& [switch_button, sdl_button] : switch_to_sdl_button) {
        int count = 0;
        const auto binding = SDL_GetGamepadBindings(controller, &count);

        // const auto& binding = SDL_GameControllerGetBindForButton(controller, sdl_button);
        mapping.insert_or_assign(
            switch_button,
            BuildParamPackageForBinding(gamepad->GetPort(), gamepad->GetGUID(), **binding));
    }
    for (const auto& [switch_button, sdl_axis] : switch_to_sdl_axis) {
        int count = 0;
        const auto binding = SDL_GetGamepadBindings(controller, &count);

        // const auto& binding = SDL_GameControllerGetBindForAxis(controller, sdl_axis);
        mapping.insert_or_assign(
            switch_button,
            BuildParamPackageForBinding(gamepad->GetPort(), gamepad->GetGUID(), **binding));
    }

    return mapping;
}

ButtonMapping SDLDriver::GetDualControllerMapping(const std::shared_ptr<SDLJoystick>& joystick,
                                                  const std::shared_ptr<SDLJoystick>& joystick2,
                                                  const ButtonBindings& switch_to_sdl_button,
                                                  const ZButtonBindings& switch_to_sdl_axis) const {
    ButtonMapping mapping;
    mapping.reserve(switch_to_sdl_button.size() + switch_to_sdl_axis.size());
    auto* controller = joystick->GetSDLGamepad();
    auto* controller2 = joystick2->GetSDLGamepad();

    for (const auto& [switch_button, sdl_button] : switch_to_sdl_button) {
        if (IsButtonOnLeftSide(switch_button)) {
            int count = 0;
            const auto binding = SDL_GetGamepadBindings(controller2, &count);

            // const auto& binding = SDL_GameControllerGetBindForButton(controller2, sdl_button);
            mapping.insert_or_assign(
                switch_button,
                BuildParamPackageForBinding(joystick2->GetPort(), joystick2->GetGUID(), **binding));
            continue;
        }
        int count = 0;
        const auto binding = SDL_GetGamepadBindings(controller, &count);
        // const auto& binding = SDL_GameControllerGetBindForButton(controller, sdl_button);
        mapping.insert_or_assign(
            switch_button,
            BuildParamPackageForBinding(joystick->GetPort(), joystick->GetGUID(), **binding));
    }
    for (const auto& [switch_button, sdl_axis] : switch_to_sdl_axis) {
        if (IsButtonOnLeftSide(switch_button)) {
            int count = 0;
            const auto binding = SDL_GetGamepadBindings(controller2, &count);
            // const auto& binding = SDL_GameControllerGetBindForAxis(controller2, sdl_axis);
            mapping.insert_or_assign(
                switch_button,
                BuildParamPackageForBinding(joystick2->GetPort(), joystick2->GetGUID(), **binding));
            continue;
        }
        int count = 0;
        const auto binding = SDL_GetGamepadBindings(controller, &count);
        // const auto& binding = SDL_GameControllerGetBindForAxis(controller, sdl_axis);
        mapping.insert_or_assign(
            switch_button,
            BuildParamPackageForBinding(joystick->GetPort(), joystick->GetGUID(), **binding));
    }

    return mapping;
}

ButtonMapping SDLDriver::GetDualControllerMapping(const std::shared_ptr<SDLGamepad>& gamepad,
                                                  const std::shared_ptr<SDLGamepad>& gamepad2,
                                                  const ButtonBindings& switch_to_sdl_button,
                                                  const ZButtonBindings& switch_to_sdl_axis) const {
    ButtonMapping mapping;
    mapping.reserve(switch_to_sdl_button.size() + switch_to_sdl_axis.size());
    auto* controller = gamepad->GetSDLGamepad();
    auto* controller2 = gamepad2->GetSDLGamepad();

    for (const auto& [switch_button, sdl_button] : switch_to_sdl_button) {
        if (IsButtonOnLeftSide(switch_button)) {
            int count = 0;
            const auto binding = SDL_GetGamepadBindings(controller2, &count);

            // const auto& binding = SDL_GameControllerGetBindForButton(controller2, sdl_button);
            mapping.insert_or_assign(
                switch_button,
                BuildParamPackageForBinding(gamepad2->GetPort(), gamepad2->GetGUID(), **binding));
            continue;
        }
        int count = 0;
        const auto binding = SDL_GetGamepadBindings(controller, &count);
        // const auto& binding = SDL_GameControllerGetBindForButton(controller, sdl_button);
        mapping.insert_or_assign(
            switch_button,
            BuildParamPackageForBinding(gamepad->GetPort(), gamepad->GetGUID(), **binding));
    }
    for (const auto& [switch_button, sdl_axis] : switch_to_sdl_axis) {
        if (IsButtonOnLeftSide(switch_button)) {
            int count = 0;
            const auto binding = SDL_GetGamepadBindings(controller2, &count);
            // const auto& binding = SDL_GameControllerGetBindForAxis(controller2, sdl_axis);
            mapping.insert_or_assign(
                switch_button,
                BuildParamPackageForBinding(gamepad2->GetPort(), gamepad2->GetGUID(), **binding));
            continue;
        }
        int count = 0;
        const auto binding = SDL_GetGamepadBindings(controller, &count);
        // const auto& binding = SDL_GameControllerGetBindForAxis(controller, sdl_axis);
        mapping.insert_or_assign(
            switch_button,
            BuildParamPackageForBinding(gamepad->GetPort(), gamepad->GetGUID(), **binding));
    }

    return mapping;
}

bool SDLDriver::IsButtonOnLeftSide(Settings::NativeButton::Values button) const {
    switch (button) {
    case Settings::NativeButton::DDown:
    case Settings::NativeButton::DLeft:
    case Settings::NativeButton::DRight:
    case Settings::NativeButton::DUp:
    case Settings::NativeButton::L:
    case Settings::NativeButton::LStick:
    case Settings::NativeButton::Minus:
    case Settings::NativeButton::Screenshot:
    case Settings::NativeButton::ZL:
        return true;
    default:
        return false;
    }
}

AnalogMapping SDLDriver::GetAnalogMappingForDevice(const Common::ParamPackage& params) {
    if (!params.Has("guid") || !params.Has("port")) {
        return {};
    }
    const auto joystick = GetSDLJoystickByGUID(params.Get("guid", ""), params.Get("port", 0));
    const auto joystick2 = GetSDLJoystickByGUID(params.Get("guid2", ""), params.Get("port", 0));
    auto* controller = joystick->GetSDLGamepad();
    if (controller == nullptr) {
        return {};
    }

    AnalogMapping mapping = {};

    int count = 0;
    const auto binding = SDL_GetGamepadBindings(controller, &count);

    // const auto& binding_left_x =
    //     SDL_GameControllerGetBindForAxis(controller, SDL_GAMEPAD_AXIS_LEFTX);
    // const auto& binding_left_y =
    //     SDL_GameControllerGetBindForAxis(controller, SDL_GAMEPAD_AXIS_LEFTY);
    if (params.Has("guid2")) {
        const auto identifier = joystick2->GetPadIdentifier();
        PreSetController(identifier);
        PreSetAxis(identifier, (*binding)->input.axis.axis);
        PreSetAxis(identifier, (*binding)->input.axis.axis);
        const auto left_offset_x = -GetAxis(identifier, (*binding)->input.axis.axis);
        const auto left_offset_y = GetAxis(identifier, (*binding)->input.axis.axis);
        mapping.insert_or_assign(Settings::NativeAnalog::LStick,
                                 BuildParamPackageForAnalog(identifier, (*binding)->input.axis.axis,
                                                            (*binding)->input.axis.axis,
                                                            left_offset_x, left_offset_y));
    } else {
        const auto identifier = joystick->GetPadIdentifier();
        PreSetController(identifier);
        PreSetAxis(identifier, (*binding)->input.axis.axis);
        PreSetAxis(identifier, (*binding)->input.axis.axis);
        const auto left_offset_x = -GetAxis(identifier, (*binding)->input.axis.axis);
        const auto left_offset_y = GetAxis(identifier, (*binding)->input.axis.axis);
        mapping.insert_or_assign(Settings::NativeAnalog::LStick,
                                 BuildParamPackageForAnalog(identifier, (*binding)->input.axis.axis,
                                                            (*binding)->input.axis.axis,
                                                            left_offset_x, left_offset_y));
    }
    // const auto& binding_right_x =
    //     SDL_GameControllerGetBindForAxis(controller, SDL_GAMEPAD_AXIS_RIGHTX);
    // const auto& binding_right_y =
    //     SDL_GameControllerGetBindForAxis(controller, SDL_GAMEPAD_AXIS_RIGHTY);
    const auto identifier = joystick->GetPadIdentifier();
    PreSetController(identifier);
    PreSetAxis(identifier, (*binding)->input.axis.axis);
    PreSetAxis(identifier, (*binding)->input.axis.axis);
    const auto right_offset_x = -GetAxis(identifier, (*binding)->input.axis.axis);
    const auto right_offset_y = GetAxis(identifier, (*binding)->input.axis.axis);
    mapping.insert_or_assign(Settings::NativeAnalog::RStick,
                             BuildParamPackageForAnalog(identifier, (*binding)->input.axis.axis,
                                                        (*binding)->input.axis.axis, right_offset_x,
                                                        right_offset_y));
    return mapping;
}

MotionMapping SDLDriver::GetMotionMappingForDevice(const Common::ParamPackage& params) {
    if (!params.Has("guid") || !params.Has("port")) {
        return {};
    }
    const auto joystick = GetSDLJoystickByGUID(params.Get("guid", ""), params.Get("port", 0));
    const auto joystick2 = GetSDLJoystickByGUID(params.Get("guid2", ""), params.Get("port", 0));
    auto* controller = joystick->GetSDLGamepad();
    if (controller == nullptr) {
        return {};
    }

    MotionMapping mapping = {};
    joystick->EnableMotion();

    if (joystick->HasMotion()) {
        mapping.insert_or_assign(Settings::NativeMotion::MotionRight,
                                 BuildMotionParam(joystick->GetPort(), joystick->GetGUID()));
    }
    if (params.Has("guid2")) {
        joystick2->EnableMotion();
        if (joystick2->HasMotion()) {
            mapping.insert_or_assign(Settings::NativeMotion::MotionLeft,
                                     BuildMotionParam(joystick2->GetPort(), joystick2->GetGUID()));
        }
    } else {
        if (joystick->HasMotion()) {
            mapping.insert_or_assign(Settings::NativeMotion::MotionLeft,
                                     BuildMotionParam(joystick->GetPort(), joystick->GetGUID()));
        }
    }

    return mapping;
}

Common::Input::ButtonNames SDLDriver::GetUIName(const Common::ParamPackage& params) const {
    if (params.Has("button")) {
        // TODO(German77): Find how to substitute the values for real button names
        return Common::Input::ButtonNames::Value;
    }
    if (params.Has("hat")) {
        return Common::Input::ButtonNames::Value;
    }
    if (params.Has("axis")) {
        return Common::Input::ButtonNames::Value;
    }
    if (params.Has("axis_x") && params.Has("axis_y") && params.Has("axis_z")) {
        return Common::Input::ButtonNames::Value;
    }
    if (params.Has("motion")) {
        return Common::Input::ButtonNames::Engine;
    }

    return Common::Input::ButtonNames::Invalid;
}

std::string SDLDriver::GetHatButtonName(u8 direction_value) const {
    switch (direction_value) {
    case SDL_HAT_UP:
        return "up";
    case SDL_HAT_DOWN:
        return "down";
    case SDL_HAT_LEFT:
        return "left";
    case SDL_HAT_RIGHT:
        return "right";
    default:
        return {};
    }
}

u8 SDLDriver::GetHatButtonId(const std::string& direction_name) const {
    Uint8 direction;
    if (direction_name == "up") {
        direction = SDL_HAT_UP;
    } else if (direction_name == "down") {
        direction = SDL_HAT_DOWN;
    } else if (direction_name == "left") {
        direction = SDL_HAT_LEFT;
    } else if (direction_name == "right") {
        direction = SDL_HAT_RIGHT;
    } else {
        direction = 0;
    }
    return direction;
}

bool SDLDriver::IsStickInverted(const Common::ParamPackage& params) {
    if (!params.Has("guid") || !params.Has("port")) {
        return false;
    }
    const auto joystick = GetSDLJoystickByGUID(params.Get("guid", ""), params.Get("port", 0));
    if (joystick == nullptr) {
        return false;
    }
    auto* controller = joystick->GetSDLGamepad();
    if (controller == nullptr) {
        return false;
    }

    const auto& axis_x = params.Get("axis_x", 0);
    const auto& axis_y = params.Get("axis_y", 0);

    int count = 0;
    const auto binding = SDL_GetGamepadBindings(controller, &count);

    // const auto& binding_left_x =
    //     SDL_GameControllerGetBindForAxis(controller, SDL_GAMEPAD_AXIS_LEFTX);
    // const auto& binding_right_x =
    //     SDL_GameControllerGetBindForAxis(controller, SDL_GAMEPAD_AXIS_RIGHTX);
    // const auto& binding_left_y =
    //     SDL_GameControllerGetBindForAxis(controller, SDL_GAMEPAD_AXIS_LEFTY);
    // const auto& binding_right_y =
    //     SDL_GameControllerGetBindForAxis(controller, SDL_GAMEPAD_AXIS_RIGHTY);

    if (axis_x != (*binding)->input.axis.axis && axis_x != (*binding)->input.axis.axis) {
        return false;
    }
    if (axis_y != (*binding)->input.axis.axis && axis_y != (*binding)->input.axis.axis) {
        return false;
    }
    return true;
}

} // namespace InputCommon
