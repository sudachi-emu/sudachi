// SPDX-FileCopyrightText: 2023 sudachi Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <array>
#include "common/fs/fs.h"
#include "common/fs/path_util.h"
#include "common/logging/log.h"
#include "common/settings.h"
#include "common/settings_common.h"
#include "common/settings_enums.h"
#include "config.h"
#include "core/core.h"
#include "core/hle/service/acc/profile_manager.h"
#include "hid_core/resources/npad/npad.h"
#include "network/network.h"

#include <boost/algorithm/string/replace.hpp>

#include "common/string_util.h"

namespace FS = Common::FS;

Config::Config(const ConfigType config_type)
    : type(config_type), global{config_type == ConfigType::GlobalConfig} {}

void Config::Initialize(const std::string& config_name) {
    const std::filesystem::path fs_config_loc = FS::GetSudachiPath(FS::SudachiPath::ConfigDir);
    const auto config_file = fmt::format("{}.ini", config_name);

    switch (type) {
    case ConfigType::GlobalConfig:
        config_loc = FS::PathToUTF8String(fs_config_loc / config_file);
        void(FS::CreateParentDir(config_loc));
        SetUpIni();
        Reload();
        break;
    case ConfigType::PerGameConfig:
        config_loc = FS::PathToUTF8String(fs_config_loc / "custom" / FS::ToU8String(config_file));
        void(FS::CreateParentDir(config_loc));
        SetUpIni();
        Reload();
        break;
    case ConfigType::InputProfile:
        config_loc = FS::PathToUTF8String(fs_config_loc / "input" / config_file);
        void(FS::CreateParentDir(config_loc));
        SetUpIni();
        break;
    }
}

void Config::Initialize(const std::optional<std::string> config_path) {
    const std::filesystem::path default_sdl_config_path =
        FS::GetSudachiPath(FS::SudachiPath::ConfigDir) / "sdl3-config.ini";
    config_loc = config_path.value_or(FS::PathToUTF8String(default_sdl_config_path));
    void(FS::CreateParentDir(config_loc));
    SetUpIni();
    Reload();
}

void Config::WriteToIni() const {
    std::string config_type;
    switch (type) {
    case ConfigType::GlobalConfig:
        config_type = "Global";
        break;
    case ConfigType::PerGameConfig:
        config_type = "Game Specific";
        break;
    case ConfigType::InputProfile:
        config_type = "Input Profile";
        break;
    }
    LOG_INFO(Config, "Writing {} configuration to: {}", config_type, config_loc);
    FILE* fp = nullptr;
#ifdef _WIN32
    fp = _wfopen(Common::UTF8ToUTF16W(config_loc).data(), L"wb");
#else
    fp = fopen(config_loc.c_str(), "wb");
#endif

    if (fp == nullptr) {
        LOG_ERROR(Frontend, "Config file could not be saved!");
        return;
    }

    CSimpleIniA::FileWriter writer(fp);
    const SI_Error rc = config->Save(writer, false);
    if (rc < 0) {
        LOG_ERROR(Frontend, "Config file could not be saved!");
    }
    fclose(fp);
}

void Config::SetUpIni() {
    config = std::make_unique<CSimpleIniA>();
    config->SetUnicode(true);
    config->SetSpaces(false);

    FILE* fp = nullptr;
#ifdef _WIN32
    _wfopen_s(&fp, Common::UTF8ToUTF16W(config_loc).data(), L"rb, ccs=UTF-8");
    if (fp == nullptr) {
        fp = _wfopen(Common::UTF8ToUTF16W(config_loc).data(), L"wb, ccs=UTF-8");
    }
#else
    fp = fopen(config_loc.c_str(), "rb");
    if (fp == nullptr) {
        fp = fopen(config_loc.c_str(), "wb");
    }
#endif

    if (fp == nullptr) {
        LOG_ERROR(Frontend, "Config file could not be loaded!");
        return;
    }

    if (SI_Error rc = config->LoadFile(fp); rc < 0) {
        LOG_ERROR(Frontend, "Config file could not be loaded!");
    }
    fclose(fp);
}

bool Config::IsCustomConfig() const {
    return type == ConfigType::PerGameConfig;
}

void Config::ReadPlayerValues(const std::size_t player_index) {
    std::string player_prefix;
    if (type != ConfigType::InputProfile) {
        player_prefix.append("player_").append(ToString(player_index)).append("_");
    }

    const auto profile_name = ReadStringSetting(std::string(player_prefix).append("profile_name"));

    auto& player = Settings::values.players.GetValue()[player_index];
    if (IsCustomConfig()) {
        if (profile_name.empty()) {
            // Use the global input config
            player = Settings::values.players.GetValue(true)[player_index];
            player.profile_name = "";
            return;
        }
        player.profile_name = profile_name;
    }

    if (player_prefix.empty() && Settings::IsConfiguringGlobal()) {
        const auto controller = static_cast<Settings::ControllerType>(
            ReadIntegerSetting(std::string(player_prefix).append("type"),
                               static_cast<u8>(Settings::ControllerType::ProController)));

        if (controller == Settings::ControllerType::LeftJoycon ||
            controller == Settings::ControllerType::RightJoycon) {
            player.controller_type = controller;
        }
    } else {
        if (global) {
            auto& player_global = Settings::values.players.GetValue(true)[player_index];
            player_global.profile_name = profile_name;
        }
        std::string connected_key = player_prefix;
        player.connected = ReadBooleanSetting(connected_key.append("connected"),
                                              std::make_optional(player_index == 0));

        player.controller_type = static_cast<Settings::ControllerType>(
            ReadIntegerSetting(std::string(player_prefix).append("type"),
                               static_cast<u8>(Settings::ControllerType::ProController)));

        player.vibration_enabled = ReadBooleanSetting(
            std::string(player_prefix).append("vibration_enabled"), std::make_optional(true));

        player.vibration_strength = static_cast<int>(
            ReadIntegerSetting(std::string(player_prefix).append("vibration_strength"), 100));

        player.body_color_left = static_cast<u32>(ReadIntegerSetting(
            std::string(player_prefix).append("body_color_left"), Settings::JOYCON_BODY_NEON_BLUE));
        player.body_color_right = static_cast<u32>(ReadIntegerSetting(
            std::string(player_prefix).append("body_color_right"), Settings::JOYCON_BODY_NEON_RED));
        player.button_color_left = static_cast<u32>(
            ReadIntegerSetting(std::string(player_prefix).append("button_color_left"),
                               Settings::JOYCON_BUTTONS_NEON_BLUE));
        player.button_color_right = static_cast<u32>(
            ReadIntegerSetting(std::string(player_prefix).append("button_color_right"),
                               Settings::JOYCON_BUTTONS_NEON_RED));
    }
}

void Config::ReadTouchscreenValues() {
    Settings::values.touchscreen.enabled =
        ReadBooleanSetting(std::string("touchscreen_enabled"), std::make_optional(true));
    Settings::values.touchscreen.rotation_angle =
        static_cast<u32>(ReadIntegerSetting(std::string("touchscreen_angle"), 0));
    Settings::values.touchscreen.diameter_x =
        static_cast<u32>(ReadIntegerSetting(std::string("touchscreen_diameter_x"), 90));
    Settings::values.touchscreen.diameter_y =
        static_cast<u32>(ReadIntegerSetting(std::string("touchscreen_diameter_y"), 90));
}

void Config::ReadAudioValues() {
    BeginGroup(Settings::TranslateCategory(Settings::Category::Audio));

    ReadCategory(Settings::Category::Audio);
    ReadCategory(Settings::Category::UiAudio);

    EndGroup();
}

void Config::ReadControlValues() {
    BeginGroup(Settings::TranslateCategory(Settings::Category::Controls));

    ReadCategory(Settings::Category::Controls);

    Settings::values.players.SetGlobal(!IsCustomConfig());
    for (std::size_t p = 0; p < Settings::values.players.GetValue().size(); ++p) {
        ReadPlayerValues(p);
    }

    // Disable docked mode if handheld is selected
    const auto controller_type = Settings::values.players.GetValue()[0].controller_type;
    if (controller_type == Settings::ControllerType::Handheld) {
        Settings::values.use_docked_mode.SetGlobal(!IsCustomConfig());
        Settings::values.use_docked_mode.SetValue(Settings::ConsoleMode::Handheld);
    }

    if (IsCustomConfig()) {
        EndGroup();
        return;
    }
    ReadTouchscreenValues();
    ReadMotionTouchValues();

    EndGroup();
}

void Config::ReadMotionTouchValues() {
    Settings::values.touch_from_button_maps.clear();
    int num_touch_from_button_maps = BeginArray(std::string("touch_from_button_maps"));

    if (num_touch_from_button_maps > 0) {
        for (int i = 0; i < num_touch_from_button_maps; ++i) {
            SetArrayIndex(i);

            Settings::TouchFromButtonMap map;
            map.name = ReadStringSetting(std::string("name"), std::string("default"));

            const int num_touch_maps = BeginArray(std::string("entries"));
            map.buttons.reserve(num_touch_maps);
            for (int j = 0; j < num_touch_maps; j++) {
                SetArrayIndex(j);
                std::string touch_mapping = ReadStringSetting(std::string("bind"));
                map.buttons.emplace_back(std::move(touch_mapping));
            }
            EndArray(); // entries
            Settings::values.touch_from_button_maps.emplace_back(std::move(map));
        }
    } else {
        Settings::values.touch_from_button_maps.emplace_back(
            Settings::TouchFromButtonMap{"default", {}});
        num_touch_from_button_maps = 1;
    }
    EndArray(); // touch_from_button_maps

    Settings::values.touch_from_button_map_index = std::clamp(
        Settings::values.touch_from_button_map_index.GetValue(), 0, num_touch_from_button_maps - 1);
}

void Config::ReadCoreValues() {
    BeginGroup(Settings::TranslateCategory(Settings::Category::Core));

    ReadCategory(Settings::Category::Core);

    EndGroup();
}

void Config::ReadDataStorageValues() {
    BeginGroup(Settings::TranslateCategory(Settings::Category::DataStorage));

    FS::SetSudachiPath(FS::SudachiPath::NANDDir, ReadStringSetting(std::string("nand_directory")));
    FS::SetSudachiPath(FS::SudachiPath::SDMCDir, ReadStringSetting(std::string("sdmc_directory")));
    FS::SetSudachiPath(FS::SudachiPath::LoadDir, ReadStringSetting(std::string("load_directory")));
    FS::SetSudachiPath(FS::SudachiPath::DumpDir, ReadStringSetting(std::string("dump_directory")));
    FS::SetSudachiPath(FS::SudachiPath::TASDir, ReadStringSetting(std::string("tas_directory")));

    ReadCategory(Settings::Category::DataStorage);

    EndGroup();
}

void Config::ReadDebuggingValues() {
    BeginGroup(Settings::TranslateCategory(Settings::Category::Debugging));

    // Intentionally not using the QT default setting as this is intended to be changed in the ini
    Settings::values.record_frame_times =
        ReadBooleanSetting(std::string("record_frame_times"), std::make_optional(false));

    ReadCategory(Settings::Category::Debugging);
    ReadCategory(Settings::Category::DebuggingGraphics);

    EndGroup();
}

#ifdef __unix__
void Config::ReadLinuxValues() {
    BeginGroup(Settings::TranslateCategory(Settings::Category::Linux));

    ReadCategory(Settings::Category::Linux);

    EndGroup();
}
#endif

void Config::ReadServiceValues() {
    BeginGroup(Settings::TranslateCategory(Settings::Category::Services));

    ReadCategory(Settings::Category::Services);

    EndGroup();
}

void Config::ReadDisabledAddOnValues() {
    // Custom config section
    BeginGroup(std::string("DisabledAddOns"));

    const int size = BeginArray(std::string(""));
    for (int i = 0; i < size; ++i) {
        SetArrayIndex(i);
        const auto title_id = ReadUnsignedIntegerSetting(std::string("title_id"), 0);
        std::vector<std::string> out;
        const int d_size = BeginArray("disabled");
        for (int j = 0; j < d_size; ++j) {
            SetArrayIndex(j);
            out.push_back(ReadStringSetting(std::string("d"), std::string("")));
        }
        EndArray(); // d
        Settings::values.disabled_addons.insert_or_assign(title_id, out);
    }
    EndArray(); // Base disabled addons array - Has no base key

    EndGroup();
}

void Config::ReadMiscellaneousValues() {
    BeginGroup(Settings::TranslateCategory(Settings::Category::Miscellaneous));

    ReadCategory(Settings::Category::Miscellaneous);

    EndGroup();
}

void Config::ReadCpuValues() {
    BeginGroup(Settings::TranslateCategory(Settings::Category::Cpu));

    ReadCategory(Settings::Category::Cpu);
    ReadCategory(Settings::Category::CpuDebug);
    ReadCategory(Settings::Category::CpuUnsafe);

    EndGroup();
}

void Config::ReadRendererValues() {
    BeginGroup(Settings::TranslateCategory(Settings::Category::Renderer));

    ReadCategory(Settings::Category::Renderer);
    ReadCategory(Settings::Category::RendererAdvanced);
    ReadCategory(Settings::Category::RendererDebug);

    EndGroup();
}

void Config::ReadScreenshotValues() {
    BeginGroup(Settings::TranslateCategory(Settings::Category::Screenshots));

    ReadCategory(Settings::Category::Screenshots);
    FS::SetSudachiPath(FS::SudachiPath::ScreenshotsDir,
                       ReadStringSetting(std::string("screenshot_path")));

    EndGroup();
}

void Config::ReadSystemValues() {
    BeginGroup(Settings::TranslateCategory(Settings::Category::System));

    ReadCategory(Settings::Category::System);
    ReadCategory(Settings::Category::SystemAudio);

    EndGroup();
}

void Config::ReadWebServiceValues() {
    BeginGroup(Settings::TranslateCategory(Settings::Category::WebService));

    ReadCategory(Settings::Category::WebService);

    EndGroup();
}

void Config::ReadNetworkValues() {
    BeginGroup(Settings::TranslateCategory(Settings::Category::Services));

    ReadCategory(Settings::Category::Network);

    EndGroup();
}

void Config::ReadLibraryAppletValues() {
    BeginGroup(Settings::TranslateCategory(Settings::Category::LibraryApplet));

    ReadCategory(Settings::Category::LibraryApplet);

    EndGroup();
}

void Config::ReadValues() {
    if (global) {
        ReadDataStorageValues();
        ReadDebuggingValues();
        ReadDisabledAddOnValues();
        ReadNetworkValues();
        ReadServiceValues();
        ReadWebServiceValues();
        ReadMiscellaneousValues();
        ReadLibraryAppletValues();
    }
    ReadControlValues();
    ReadCoreValues();
    ReadCpuValues();
#ifdef __unix__
    ReadLinuxValues();
#endif
    ReadRendererValues();
    ReadAudioValues();
    ReadSystemValues();
}

void Config::SavePlayerValues(const std::size_t player_index) {
    std::string player_prefix;
    if (type != ConfigType::InputProfile) {
        player_prefix = std::string("player_").append(ToString(player_index)).append("_");
    }

    const auto& player = Settings::values.players.GetValue()[player_index];
    if (IsCustomConfig()) {
        if (player.profile_name.empty()) {
            // No custom profile selected
            return;
        }
        WriteStringSetting(std::string(player_prefix).append("profile_name"), player.profile_name,
                           std::make_optional(std::string("")));
    }

    WriteIntegerSetting(
        std::string(player_prefix).append("type"), static_cast<u8>(player.controller_type),
        std::make_optional(static_cast<u8>(Settings::ControllerType::ProController)));

    if (!player_prefix.empty() || !Settings::IsConfiguringGlobal()) {
        if (global) {
            const auto& player_global = Settings::values.players.GetValue(true)[player_index];
            WriteStringSetting(std::string(player_prefix).append("profile_name"),
                               player_global.profile_name, std::make_optional(std::string("")));
        }
        WriteBooleanSetting(std::string(player_prefix).append("connected"), player.connected,
                            std::make_optional(player_index == 0));
        WriteIntegerSetting(std::string(player_prefix).append("vibration_enabled"),
                            player.vibration_enabled, std::make_optional(true));
        WriteIntegerSetting(std::string(player_prefix).append("vibration_strength"),
                            player.vibration_strength, std::make_optional(100));
        WriteIntegerSetting(std::string(player_prefix).append("body_color_left"),
                            player.body_color_left,
                            std::make_optional(Settings::JOYCON_BODY_NEON_BLUE));
        WriteIntegerSetting(std::string(player_prefix).append("body_color_right"),
                            player.body_color_right,
                            std::make_optional(Settings::JOYCON_BODY_NEON_RED));
        WriteIntegerSetting(std::string(player_prefix).append("button_color_left"),
                            player.button_color_left,
                            std::make_optional(Settings::JOYCON_BUTTONS_NEON_BLUE));
        WriteIntegerSetting(std::string(player_prefix).append("button_color_right"),
                            player.button_color_right,
                            std::make_optional(Settings::JOYCON_BUTTONS_NEON_RED));
    }
}

void Config::SaveTouchscreenValues() {
    const auto& touchscreen = Settings::values.touchscreen;

    WriteBooleanSetting(std::string("touchscreen_enabled"), touchscreen.enabled,
                        std::make_optional(true));

    WriteIntegerSetting(std::string("touchscreen_angle"), touchscreen.rotation_angle,
                        std::make_optional(static_cast<u32>(0)));
    WriteIntegerSetting(std::string("touchscreen_diameter_x"), touchscreen.diameter_x,
                        std::make_optional(static_cast<u32>(90)));
    WriteIntegerSetting(std::string("touchscreen_diameter_y"), touchscreen.diameter_y,
                        std::make_optional(static_cast<u32>(90)));
}

void Config::SaveMotionTouchValues() {
    BeginArray(std::string("touch_from_button_maps"));
    for (std::size_t p = 0; p < Settings::values.touch_from_button_maps.size(); ++p) {
        SetArrayIndex(static_cast<int>(p));
        WriteStringSetting(std::string("name"), Settings::values.touch_from_button_maps[p].name,
                           std::make_optional(std::string("default")));

        BeginArray(std::string("entries"));
        for (std::size_t q = 0; q < Settings::values.touch_from_button_maps[p].buttons.size();
             ++q) {
            SetArrayIndex(static_cast<int>(q));
            WriteStringSetting(std::string("bind"),
                               Settings::values.touch_from_button_maps[p].buttons[q]);
        }
        EndArray(); // entries
    }
    EndArray(); // touch_from_button_maps
}

void Config::SaveValues() {
    if (global) {
        LOG_DEBUG(Config, "Saving global generic configuration values");
        SaveDataStorageValues();
        SaveDebuggingValues();
        SaveDisabledAddOnValues();
        SaveNetworkValues();
        SaveWebServiceValues();
        SaveMiscellaneousValues();
        SaveLibraryAppletValues();
    } else {
        LOG_DEBUG(Config, "Saving only generic configuration values");
    }
    SaveControlValues();
    SaveCoreValues();
    SaveCpuValues();
#ifdef __unix__
    SaveLinuxValues();
#endif
    SaveRendererValues();
    SaveAudioValues();
    SaveSystemValues();

    WriteToIni();
}

void Config::SaveAudioValues() {
    BeginGroup(Settings::TranslateCategory(Settings::Category::Audio));

    WriteCategory(Settings::Category::Audio);
    WriteCategory(Settings::Category::UiAudio);

    EndGroup();
}

void Config::SaveControlValues() {
    BeginGroup(Settings::TranslateCategory(Settings::Category::Controls));

    WriteCategory(Settings::Category::Controls);

    Settings::values.players.SetGlobal(!IsCustomConfig());
    for (std::size_t p = 0; p < Settings::values.players.GetValue().size(); ++p) {
        SavePlayerValues(p);
    }
    if (IsCustomConfig()) {
        EndGroup();
        return;
    }
    SaveTouchscreenValues();
    SaveMotionTouchValues();

    EndGroup();
}

void Config::SaveCoreValues() {
    BeginGroup(Settings::TranslateCategory(Settings::Category::Core));

    WriteCategory(Settings::Category::Core);

    EndGroup();
}

void Config::SaveDataStorageValues() {
    BeginGroup(Settings::TranslateCategory(Settings::Category::DataStorage));

    WriteStringSetting(std::string("nand_directory"),
                       FS::GetSudachiPathString(FS::SudachiPath::NANDDir),
                       std::make_optional(FS::GetSudachiPathString(FS::SudachiPath::NANDDir)));
    WriteStringSetting(std::string("sdmc_directory"),
                       FS::GetSudachiPathString(FS::SudachiPath::SDMCDir),
                       std::make_optional(FS::GetSudachiPathString(FS::SudachiPath::SDMCDir)));
    WriteStringSetting(std::string("load_directory"),
                       FS::GetSudachiPathString(FS::SudachiPath::LoadDir),
                       std::make_optional(FS::GetSudachiPathString(FS::SudachiPath::LoadDir)));
    WriteStringSetting(std::string("dump_directory"),
                       FS::GetSudachiPathString(FS::SudachiPath::DumpDir),
                       std::make_optional(FS::GetSudachiPathString(FS::SudachiPath::DumpDir)));
    WriteStringSetting(std::string("tas_directory"),
                       FS::GetSudachiPathString(FS::SudachiPath::TASDir),
                       std::make_optional(FS::GetSudachiPathString(FS::SudachiPath::TASDir)));

    WriteCategory(Settings::Category::DataStorage);

    EndGroup();
}

void Config::SaveDebuggingValues() {
    BeginGroup(Settings::TranslateCategory(Settings::Category::Debugging));

    // Intentionally not using the QT default setting as this is intended to be changed in the ini
    WriteBooleanSetting(std::string("record_frame_times"), Settings::values.record_frame_times);

    WriteCategory(Settings::Category::Debugging);
    WriteCategory(Settings::Category::DebuggingGraphics);

    EndGroup();
}

#ifdef __unix__
void Config::SaveLinuxValues() {
    BeginGroup(Settings::TranslateCategory(Settings::Category::Linux));

    WriteCategory(Settings::Category::Linux);

    EndGroup();
}
#endif

void Config::SaveNetworkValues() {
    BeginGroup(Settings::TranslateCategory(Settings::Category::Services));

    WriteCategory(Settings::Category::Network);

    EndGroup();
}

void Config::SaveDisabledAddOnValues() {
    // Custom config section
    BeginGroup(std::string("DisabledAddOns"));

    int i = 0;
    BeginArray(std::string(""));
    for (const auto& elem : Settings::values.disabled_addons) {
        SetArrayIndex(i);
        WriteIntegerSetting(std::string("title_id"), elem.first,
                            std::make_optional(static_cast<u64>(0)));
        BeginArray(std::string("disabled"));
        for (std::size_t j = 0; j < elem.second.size(); ++j) {
            SetArrayIndex(static_cast<int>(j));
            WriteStringSetting(std::string("d"), elem.second[j],
                               std::make_optional(std::string("")));
        }
        EndArray(); // disabled
        ++i;
    }
    EndArray(); // Base disabled addons array - Has no base key

    EndGroup();
}

void Config::SaveMiscellaneousValues() {
    BeginGroup(Settings::TranslateCategory(Settings::Category::Miscellaneous));

    WriteCategory(Settings::Category::Miscellaneous);

    EndGroup();
}

void Config::SaveCpuValues() {
    BeginGroup(Settings::TranslateCategory(Settings::Category::Cpu));

    WriteCategory(Settings::Category::Cpu);
    WriteCategory(Settings::Category::CpuDebug);
    WriteCategory(Settings::Category::CpuUnsafe);

    EndGroup();
}

void Config::SaveRendererValues() {
    BeginGroup(Settings::TranslateCategory(Settings::Category::Renderer));

    WriteCategory(Settings::Category::Renderer);
    WriteCategory(Settings::Category::RendererAdvanced);
    WriteCategory(Settings::Category::RendererDebug);

    EndGroup();
}

void Config::SaveScreenshotValues() {
    BeginGroup(Settings::TranslateCategory(Settings::Category::Screenshots));

    WriteStringSetting(std::string("screenshot_path"),
                       FS::GetSudachiPathString(FS::SudachiPath::ScreenshotsDir));
    WriteCategory(Settings::Category::Screenshots);

    EndGroup();
}

void Config::SaveSystemValues() {
    BeginGroup(Settings::TranslateCategory(Settings::Category::System));

    WriteCategory(Settings::Category::System);
    WriteCategory(Settings::Category::SystemAudio);

    EndGroup();
}

void Config::SaveWebServiceValues() {
    BeginGroup(Settings::TranslateCategory(Settings::Category::WebService));

    WriteCategory(Settings::Category::WebService);

    EndGroup();
}

void Config::SaveLibraryAppletValues() {
    BeginGroup(Settings::TranslateCategory(Settings::Category::LibraryApplet));

    WriteCategory(Settings::Category::LibraryApplet);

    EndGroup();
}

bool Config::ReadBooleanSetting(const std::string& key, const std::optional<bool> default_value) {
    std::string full_key = GetFullKey(key, false);
    if (!default_value.has_value()) {
        return config->GetBoolValue(GetSection().c_str(), full_key.c_str(), false);
    }

    if (config->GetBoolValue(GetSection().c_str(),
                             std::string(full_key).append("\\default").c_str(), false)) {
        return static_cast<bool>(default_value.value());
    } else {
        return config->GetBoolValue(GetSection().c_str(), full_key.c_str(),
                                    static_cast<bool>(default_value.value()));
    }
}

s64 Config::ReadIntegerSetting(const std::string& key, const std::optional<s64> default_value) {
    std::string full_key = GetFullKey(key, false);
    if (!default_value.has_value()) {
        try {
            return std::stoll(
                std::string(config->GetValue(GetSection().c_str(), full_key.c_str(), "0")));
        } catch (...) {
            return 0;
        }
    }

    s64 result = 0;
    if (config->GetBoolValue(GetSection().c_str(),
                             std::string(full_key).append("\\default").c_str(), true)) {
        result = default_value.value();
    } else {
        try {
            result = std::stoll(std::string(config->GetValue(
                GetSection().c_str(), full_key.c_str(), ToString(default_value.value()).c_str())));
        } catch (...) {
            result = default_value.value();
        }
    }
    return result;
}

u64 Config::ReadUnsignedIntegerSetting(const std::string& key,
                                       const std::optional<u64> default_value) {
    std::string full_key = GetFullKey(key, false);
    if (!default_value.has_value()) {
        try {
            return std::stoull(
                std::string(config->GetValue(GetSection().c_str(), full_key.c_str(), "0")));
        } catch (...) {
            return 0;
        }
    }

    u64 result = 0;
    if (config->GetBoolValue(GetSection().c_str(),
                             std::string(full_key).append("\\default").c_str(), true)) {
        result = default_value.value();
    } else {
        try {
            result = std::stoull(std::string(config->GetValue(
                GetSection().c_str(), full_key.c_str(), ToString(default_value.value()).c_str())));
        } catch (...) {
            result = default_value.value();
        }
    }
    return result;
}

double Config::ReadDoubleSetting(const std::string& key,
                                 const std::optional<double> default_value) {
    std::string full_key = GetFullKey(key, false);
    if (!default_value.has_value()) {
        return config->GetDoubleValue(GetSection().c_str(), full_key.c_str(), 0);
    }

    double result;
    if (config->GetBoolValue(GetSection().c_str(),
                             std::string(full_key).append("\\default").c_str(), true)) {
        result = default_value.value();
    } else {
        result =
            config->GetDoubleValue(GetSection().c_str(), full_key.c_str(), default_value.value());
    }
    return result;
}

std::string Config::ReadStringSetting(const std::string& key,
                                      const std::optional<std::string> default_value) {
    std::string result;
    std::string full_key = GetFullKey(key, false);
    if (!default_value.has_value()) {
        result = config->GetValue(GetSection().c_str(), full_key.c_str(), "");
        boost::replace_all(result, "\"", "");
        return result;
    }

    if (config->GetBoolValue(GetSection().c_str(),
                             std::string(full_key).append("\\default").c_str(), true)) {
        result = default_value.value();
    } else {
        result =
            config->GetValue(GetSection().c_str(), full_key.c_str(), default_value.value().c_str());
    }
    boost::replace_all(result, "\"", "");
    boost::replace_all(result, "//", "/");
    return result;
}

bool Config::Exists(const std::string& section, const std::string& key) const {
    const std::string value = config->GetValue(section.c_str(), key.c_str(), "");
    return !value.empty();
}

void Config::WriteBooleanSetting(const std::string& key, const bool& value,
                                 const std::optional<bool>& default_value,
                                 const std::optional<bool>& use_global) {
    std::optional<std::string> string_default = std::nullopt;
    if (default_value.has_value()) {
        string_default = std::make_optional(ToString(default_value.value()));
    }
    WritePreparedSetting(key, AdjustOutputString(ToString(value)), string_default, use_global);
}

void Config::WriteDoubleSetting(const std::string& key, const double& value,
                                const std::optional<double>& default_value,
                                const std::optional<bool>& use_global) {
    std::optional<std::string> string_default = std::nullopt;
    if (default_value.has_value()) {
        string_default = std::make_optional(ToString(default_value.value()));
    }
    WritePreparedSetting(key, AdjustOutputString(ToString(value)), string_default, use_global);
}

void Config::WriteStringSetting(const std::string& key, const std::string& value,
                                const std::optional<std::string>& default_value,
                                const std::optional<bool>& use_global) {
    std::optional string_default = default_value;
    if (default_value.has_value()) {
        string_default.value().append(AdjustOutputString(default_value.value()));
    }
    WritePreparedSetting(key, AdjustOutputString(value), string_default, use_global);
}

void Config::WritePreparedSetting(const std::string& key, const std::string& adjusted_value,
                                  const std::optional<std::string>& adjusted_default_value,
                                  const std::optional<bool>& use_global) {
    std::string full_key = GetFullKey(key, false);
    if (adjusted_default_value.has_value() && use_global.has_value()) {
        if (!global) {
            WriteString(std::string(full_key).append("\\global"), ToString(use_global.value()));
        }
        if (global || use_global.value() == false) {
            WriteString(std::string(full_key).append("\\default"),
                        ToString(adjusted_default_value == adjusted_value));
            WriteString(full_key, adjusted_value);
        }
    } else if (adjusted_default_value.has_value() && !use_global.has_value()) {
        WriteString(std::string(full_key).append("\\default"),
                    ToString(adjusted_default_value == adjusted_value));
        WriteString(full_key, adjusted_value);
    } else {
        WriteString(full_key, adjusted_value);
    }
}

void Config::WriteString(const std::string& key, const std::string& value) {
    config->SetValue(GetSection().c_str(), key.c_str(), value.c_str());
}

void Config::Reload() {
    ReadValues();
    // To apply default value changes
    SaveValues();
}

void Config::ClearControlPlayerValues() const {
    // Removes the entire [Controls] section
    const char* section = Settings::TranslateCategory(Settings::Category::Controls);
    config->Delete(section, nullptr, true);
}

const std::string& Config::GetConfigFilePath() const {
    return config_loc;
}

void Config::ReadCategory(const Settings::Category category) {
    const auto& settings = FindRelevantList(category);
    std::ranges::for_each(settings, [&](const auto& setting) { ReadSettingGeneric(setting); });
}

void Config::WriteCategory(const Settings::Category category) {
    const auto& settings = FindRelevantList(category);
    std::ranges::for_each(settings, [&](const auto& setting) { WriteSettingGeneric(setting); });
}

void Config::ReadSettingGeneric(Settings::BasicSetting* const setting) {
    if (!setting->Save() || (!setting->Switchable() && !global)) {
        return;
    }

    const std::string key = AdjustKey(setting->GetLabel());
    const std::string default_value(setting->DefaultToString());

    bool use_global = true;
    if (setting->Switchable() && !global) {
        use_global =
            ReadBooleanSetting(std::string(key).append("\\use_global"), std::make_optional(true));
        setting->SetGlobal(use_global);
    }

    if (global || !use_global) {
        const bool is_default =
            ReadBooleanSetting(std::string(key).append("\\default"), std::make_optional(true));
        if (!is_default) {
            const std::string setting_string = ReadStringSetting(key, default_value);
            setting->LoadString(setting_string);
        } else {
            // Empty string resets the Setting to default
            setting->LoadString("");
        }
    }
}

void Config::WriteSettingGeneric(const Settings::BasicSetting* const setting) {
    if (!setting->Save()) {
        return;
    }

    std::string key = AdjustKey(setting->GetLabel());
    if (setting->Switchable()) {
        if (!global) {
            WriteBooleanSetting(std::string(key).append("\\use_global"), setting->UsingGlobal());
        }
        if (global || !setting->UsingGlobal()) {
            auto value = global ? setting->ToStringGlobal() : setting->ToString();
            WriteBooleanSetting(std::string(key).append("\\default"),
                                value == setting->DefaultToString());
            WriteStringSetting(key, value);
        }
    } else if (global) {
        WriteBooleanSetting(std::string(key).append("\\default"),
                            setting->ToString() == setting->DefaultToString());
        WriteStringSetting(key, setting->ToString());
    }
}

void Config::BeginGroup(const std::string& group) {
    // You can't begin a group while reading/writing from a config array
    ASSERT(array_stack.empty());

    key_stack.push_back(AdjustKey(group));
}

void Config::EndGroup() {
    // You can't end a group if you haven't started one yet
    ASSERT(!key_stack.empty());

    // You can't end a group when reading/writing from a config array
    ASSERT(array_stack.empty());

    key_stack.pop_back();
}

std::string Config::GetSection() {
    if (key_stack.empty()) {
        return std::string{""};
    }

    return key_stack.front();
}

std::string Config::GetGroup() const {
    if (key_stack.size() <= 1) {
        return std::string{""};
    }

    std::string key;
    for (size_t i = 1; i < key_stack.size(); ++i) {
        key.append(key_stack[i]).append("\\");
    }
    return key;
}

std::string Config::AdjustKey(const std::string& key) {
    std::string adjusted_key(key);
    boost::replace_all(adjusted_key, "/", "\\");
    boost::replace_all(adjusted_key, " ", "%20");
    return adjusted_key;
}

std::string Config::AdjustOutputString(const std::string& string) {
    std::string adjusted_string(string);
    boost::replace_all(adjusted_string, "\\", "/");

    // Windows requires that two forward slashes are used at the start of a path for unmapped
    // network drives so we have to watch for that here
#ifndef ANDROID
    if (string.substr(0, 2) == "//") {
        boost::replace_all(adjusted_string, "//", "/");
        adjusted_string.insert(0, "/");
    } else {
        boost::replace_all(adjusted_string, "//", "/");
    }
#endif

    // Needed for backwards compatibility with QSettings deserialization
    for (const auto& special_character : special_characters) {
        if (adjusted_string.find(special_character) != std::string::npos) {
            adjusted_string.insert(0, "\"");
            adjusted_string.append("\"");
            break;
        }
    }
    return adjusted_string;
}

std::string Config::GetFullKey(const std::string& key, bool skipArrayIndex) {
    if (array_stack.empty()) {
        return std::string(GetGroup()).append(AdjustKey(key));
    }

    std::string array_key;
    for (size_t i = 0; i < array_stack.size(); ++i) {
        if (!array_stack[i].name.empty()) {
            array_key.append(array_stack[i].name).append("\\");
        }

        if (!skipArrayIndex || (array_stack.size() - 1 != i && array_stack.size() > 1)) {
            array_key.append(ToString(array_stack[i].index)).append("\\");
        }
    }
    std::string final_key = std::string(GetGroup()).append(array_key).append(AdjustKey(key));
    return final_key;
}

int Config::BeginArray(const std::string& array) {
    array_stack.push_back(ConfigArray{AdjustKey(array), 0, 0});
    const int size = config->GetLongValue(GetSection().c_str(),
                                          GetFullKey(std::string("size"), true).c_str(), 0);
    array_stack.back().size = size;
    return size;
}

void Config::EndArray() {
    // You can't end a config array before starting one
    ASSERT(!array_stack.empty());

    // Set the array size to 0 if the array is ended without changing the index
    int size = 0;
    if (array_stack.back().index != 0) {
        size = array_stack.back().size;
    }

    // Write out the size to config
    if (key_stack.size() == 1 && array_stack.back().name.empty()) {
        // Edge-case where the first array created doesn't have a name
        config->SetValue(GetSection().c_str(), std::string("size").c_str(), ToString(size).c_str());
    } else {
        const auto key = GetFullKey(std::string("size"), true);
        config->SetValue(GetSection().c_str(), key.c_str(), ToString(size).c_str());
    }

    array_stack.pop_back();
}

void Config::SetArrayIndex(const int index) {
    // You can't set the array index if you haven't started one yet
    ASSERT(!array_stack.empty());

    const int array_index = index + 1;

    // You can't exceed the known max size of the array by more than 1
    ASSERT(array_stack.front().size + 1 >= array_index);

    // Change the config array size to the current index since you may want
    // to reduce the number of elements that you read back from the config
    // in the future.
    array_stack.back().size = array_index;
    array_stack.back().index = array_index;
}
