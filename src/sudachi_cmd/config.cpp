// SPDX-FileCopyrightText: 2014 Citra Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <memory>
#include <optional>
#include <sstream>
#include <INIReader.h>
#include <SDL.h>
#include "common/fs/file.h"
#include "common/fs/fs.h"
#include "common/fs/path_util.h"
#include "common/logging/log.h"
#include "common/settings.h"
#include "core/hle/service/acc/profile_manager.h"
#include "input_common/main.h"
#include "sudachi_cmd/config.h"
#include "sudachi_cmd/default_ini.h"

namespace FS = Common::FS;

const std::filesystem::path default_config_path =
    FS::GetSudachiPath(FS::SudachiPath::ConfigDir) / "sdl3-config.ini";

Config::Config(std::optional<std::filesystem::path> config_path)
    : sdl3_config_loc{config_path.value_or(default_config_path)},
      sdl3_config{std::make_unique<INIReader>(FS::PathToUTF8String(sdl3_config_loc))} {
    Reload();
}

Config::~Config() = default;

bool Config::LoadINI(const std::string& default_contents, bool retry) {
    const auto config_loc_str = FS::PathToUTF8String(sdl3_config_loc);
    if (sdl3_config->ParseError() < 0) {
        if (retry) {
            LOG_WARNING(Config, "Failed to load {}. Creating file from defaults...",
                        config_loc_str);

            void(FS::CreateParentDir(sdl3_config_loc));
            void(FS::WriteStringToFile(sdl3_config_loc, FS::FileType::TextFile, default_contents));

            sdl3_config = std::make_unique<INIReader>(config_loc_str);

            return LoadINI(default_contents, false);
        }
        LOG_ERROR(Config, "Failed.");
        return false;
    }
    LOG_INFO(Config, "Successfully loaded {}", config_loc_str);
    return true;
}

static const std::array<int, Settings::NativeButton::NumButtons> default_buttons = {
    SDL_SCANCODE_A, SDL_SCANCODE_S, SDL_SCANCODE_Z, SDL_SCANCODE_X, SDL_SCANCODE_T,
    SDL_SCANCODE_G, SDL_SCANCODE_F, SDL_SCANCODE_H, SDL_SCANCODE_Q, SDL_SCANCODE_W,
    SDL_SCANCODE_M, SDL_SCANCODE_N, SDL_SCANCODE_1, SDL_SCANCODE_2, SDL_SCANCODE_B,
};

static const std::array<int, Settings::NativeMotion::NumMotions> default_motions = {
    SDL_SCANCODE_7,
    SDL_SCANCODE_8,
};

static const std::array<std::array<int, 5>, Settings::NativeAnalog::NumAnalogs> default_analogs{{
    {
        SDL_SCANCODE_UP,
        SDL_SCANCODE_DOWN,
        SDL_SCANCODE_LEFT,
        SDL_SCANCODE_RIGHT,
        SDL_SCANCODE_D,
    },
    {
        SDL_SCANCODE_I,
        SDL_SCANCODE_K,
        SDL_SCANCODE_J,
        SDL_SCANCODE_L,
        SDL_SCANCODE_D,
    },
}};

template <>
void Config::ReadSetting(const std::string& group, Settings::Setting<std::string>& setting) {
    std::string setting_value = sdl3_config->Get(group, setting.GetLabel(), setting.GetDefault());
    if (setting_value.empty()) {
        setting_value = setting.GetDefault();
    }
    setting = std::move(setting_value);
}

template <>
void Config::ReadSetting(const std::string& group, Settings::Setting<bool>& setting) {
    setting = sdl3_config->GetBoolean(group, setting.GetLabel(), setting.GetDefault());
}

template <typename Type, bool ranged>
void Config::ReadSetting(const std::string& group, Settings::Setting<Type, ranged>& setting) {
    setting = static_cast<Type>(sdl3_config->GetInteger(group, setting.GetLabel(),
                                                        static_cast<long>(setting.GetDefault())));
}

void Config::ReadCategory(Settings::Category category) {
    for (const auto setting : Settings::values.linkage.by_category[category]) {
        const char* category_name = [&]() {
            if (category == Settings::Category::Controls) {
                // For compatibility with older configs
                return "ControlsGeneral";
            } else {
                return Settings::TranslateCategory(category);
            }
        }();
        std::string setting_value =
            sdl3_config->Get(category_name, setting->GetLabel(), setting->DefaultToString());
        setting->LoadString(setting_value);
    }
}

void Config::ReadValues() {
    // Controls
    ReadCategory(Settings::Category::Controls);

    for (std::size_t p = 0; p < Settings::values.players.GetValue().size(); ++p) {
        auto& player = Settings::values.players.GetValue()[p];

        const auto group = fmt::format("ControlsP{}", p);
        for (int i = 0; i < Settings::NativeButton::NumButtons; ++i) {
            std::string default_param = InputCommon::GenerateKeyboardParam(default_buttons[i]);
            player.buttons[i] =
                sdl3_config->Get(group, Settings::NativeButton::mapping[i], default_param);
            if (player.buttons[i].empty()) {
                player.buttons[i] = default_param;
            }
        }

        for (int i = 0; i < Settings::NativeAnalog::NumAnalogs; ++i) {
            std::string default_param = InputCommon::GenerateAnalogParamFromKeys(
                default_analogs[i][0], default_analogs[i][1], default_analogs[i][2],
                default_analogs[i][3], default_analogs[i][4], 0.5f);
            player.analogs[i] =
                sdl3_config->Get(group, Settings::NativeAnalog::mapping[i], default_param);
            if (player.analogs[i].empty()) {
                player.analogs[i] = default_param;
            }
        }

        for (int i = 0; i < Settings::NativeMotion::NumMotions; ++i) {
            const std::string default_param =
                InputCommon::GenerateKeyboardParam(default_motions[i]);
            auto& player_motions = player.motions[i];

            player_motions =
                sdl3_config->Get(group, Settings::NativeMotion::mapping[i], default_param);
            if (player_motions.empty()) {
                player_motions = default_param;
            }
        }

        player.connected = sdl3_config->GetBoolean(group, "connected", false);
    }

    for (int i = 0; i < Settings::NativeButton::NumButtons; ++i) {
        std::string default_param = InputCommon::GenerateKeyboardParam(default_buttons[i]);
        Settings::values.debug_pad_buttons[i] = sdl3_config->Get(
            "ControlsGeneral", std::string("debug_pad_") + Settings::NativeButton::mapping[i],
            default_param);
        if (Settings::values.debug_pad_buttons[i].empty())
            Settings::values.debug_pad_buttons[i] = default_param;
    }

    for (int i = 0; i < Settings::NativeAnalog::NumAnalogs; ++i) {
        std::string default_param = InputCommon::GenerateAnalogParamFromKeys(
            default_analogs[i][0], default_analogs[i][1], default_analogs[i][2],
            default_analogs[i][3], default_analogs[i][4], 0.5f);
        Settings::values.debug_pad_analogs[i] = sdl3_config->Get(
            "ControlsGeneral", std::string("debug_pad_") + Settings::NativeAnalog::mapping[i],
            default_param);
        if (Settings::values.debug_pad_analogs[i].empty())
            Settings::values.debug_pad_analogs[i] = default_param;
    }

    Settings::values.touchscreen.enabled =
        sdl3_config->GetBoolean("ControlsGeneral", "touch_enabled", true);
    Settings::values.touchscreen.rotation_angle =
        sdl3_config->GetInteger("ControlsGeneral", "touch_angle", 0);
    Settings::values.touchscreen.diameter_x =
        sdl3_config->GetInteger("ControlsGeneral", "touch_diameter_x", 15);
    Settings::values.touchscreen.diameter_y =
        sdl3_config->GetInteger("ControlsGeneral", "touch_diameter_y", 15);

    int num_touch_from_button_maps =
        sdl3_config->GetInteger("ControlsGeneral", "touch_from_button_map", 0);
    if (num_touch_from_button_maps > 0) {
        for (int i = 0; i < num_touch_from_button_maps; ++i) {
            Settings::TouchFromButtonMap map;
            map.name = sdl3_config->Get("ControlsGeneral",
                                        std::string("touch_from_button_maps_") + std::to_string(i) +
                                            std::string("_name"),
                                        "default");
            const int num_touch_maps = sdl3_config->GetInteger(
                "ControlsGeneral",
                std::string("touch_from_button_maps_") + std::to_string(i) + std::string("_count"),
                0);
            map.buttons.reserve(num_touch_maps);

            for (int j = 0; j < num_touch_maps; ++j) {
                std::string touch_mapping =
                    sdl3_config->Get("ControlsGeneral",
                                     std::string("touch_from_button_maps_") + std::to_string(i) +
                                         std::string("_bind_") + std::to_string(j),
                                     "");
                map.buttons.emplace_back(std::move(touch_mapping));
            }

            Settings::values.touch_from_button_maps.emplace_back(std::move(map));
        }
    } else {
        Settings::values.touch_from_button_maps.emplace_back(
            Settings::TouchFromButtonMap{"default", {}});
        num_touch_from_button_maps = 1;
    }
    Settings::values.touch_from_button_map_index = std::clamp(
        Settings::values.touch_from_button_map_index.GetValue(), 0, num_touch_from_button_maps - 1);

    ReadCategory(Settings::Category::Audio);
    ReadCategory(Settings::Category::Core);
    ReadCategory(Settings::Category::Cpu);
    ReadCategory(Settings::Category::CpuDebug);
    ReadCategory(Settings::Category::CpuUnsafe);
    ReadCategory(Settings::Category::Renderer);
    ReadCategory(Settings::Category::RendererAdvanced);
    ReadCategory(Settings::Category::RendererDebug);
    ReadCategory(Settings::Category::System);
    ReadCategory(Settings::Category::SystemAudio);
    ReadCategory(Settings::Category::DataStorage);
    ReadCategory(Settings::Category::Debugging);
    ReadCategory(Settings::Category::DebuggingGraphics);
    ReadCategory(Settings::Category::Miscellaneous);
    ReadCategory(Settings::Category::Network);
    ReadCategory(Settings::Category::WebService);

    // Data Storage
    FS::SetSudachiPath(FS::SudachiPath::NANDDir,
                       sdl3_config->Get("Data Storage", "nand_directory",
                                        FS::GetSudachiPathString(FS::SudachiPath::NANDDir)));
    FS::SetSudachiPath(FS::SudachiPath::SDMCDir,
                       sdl3_config->Get("Data Storage", "sdmc_directory",
                                        FS::GetSudachiPathString(FS::SudachiPath::SDMCDir)));
    FS::SetSudachiPath(FS::SudachiPath::LoadDir,
                       sdl3_config->Get("Data Storage", "load_directory",
                                        FS::GetSudachiPathString(FS::SudachiPath::LoadDir)));
    FS::SetSudachiPath(FS::SudachiPath::DumpDir,
                       sdl3_config->Get("Data Storage", "dump_directory",
                                        FS::GetSudachiPathString(FS::SudachiPath::DumpDir)));

    // Debugging
    Settings::values.record_frame_times =
        sdl3_config->GetBoolean("Debugging", "record_frame_times", false);

    const auto title_list = sdl3_config->Get("AddOns", "title_ids", "");
    std::stringstream ss(title_list);
    std::string line;
    while (std::getline(ss, line, '|')) {
        const auto title_id = std::strtoul(line.c_str(), nullptr, 16);
        const auto disabled_list = sdl3_config->Get("AddOns", "disabled_" + line, "");

        std::stringstream inner_ss(disabled_list);
        std::string inner_line;
        std::vector<std::string> out;
        while (std::getline(inner_ss, inner_line, '|')) {
            out.push_back(inner_line);
        }

        Settings::values.disabled_addons.insert_or_assign(title_id, out);
    }
}

void Config::Reload() {
    LoadINI(DefaultINI::sdl3_config_file);
    ReadValues();
}
