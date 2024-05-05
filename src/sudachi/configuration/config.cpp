// SPDX-FileCopyrightText: 2014 Citra Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <array>
#include <QKeySequence>
#include <QSettings>
#include "common/fs/fs.h"
#include "common/fs/path_util.h"
#include "common/settings.h"
#include "common/settings_common.h"
#include "common/settings_enums.h"
#include "core/core.h"
#include "core/hle/service/acc/profile_manager.h"
#include "core/hle/service/hid/controllers/npad.h"
#include "input_common/main.h"
#include "network/network.h"
#include "sudachi/configuration/config.h"

namespace FS = Common::FS;

Config::Config(const std::string& config_name, ConfigType config_type)
    : type(config_type), global{config_type == ConfigType::GlobalConfig} {
    Initialize(config_name);
}

Config::~Config() {
    if (global) {
        Save();
    }
}

const std::array<int, Settings::NativeButton::NumButtons> Config::default_buttons = {
    Qt::Key_C,    Qt::Key_X, Qt::Key_V,    Qt::Key_Z,  Qt::Key_F,
    Qt::Key_G,    Qt::Key_Q, Qt::Key_E,    Qt::Key_R,  Qt::Key_T,
    Qt::Key_M,    Qt::Key_N, Qt::Key_Left, Qt::Key_Up, Qt::Key_Right,
    Qt::Key_Down, Qt::Key_Q, Qt::Key_E,    0,          0,
    Qt::Key_Q,    Qt::Key_E,
};

const std::array<int, Settings::NativeMotion::NumMotions> Config::default_motions = {
    Qt::Key_7,
    Qt::Key_8,
};

const std::array<std::array<int, 4>, Settings::NativeAnalog::NumAnalogs> Config::default_analogs{{
    {
        Qt::Key_W,
        Qt::Key_S,
        Qt::Key_A,
        Qt::Key_D,
    },
    {
        Qt::Key_I,
        Qt::Key_K,
        Qt::Key_J,
        Qt::Key_L,
    },
}};

const std::array<int, 2> Config::default_stick_mod = {
    Qt::Key_Shift,
    0,
};

const std::array<int, 2> Config::default_ringcon_analogs{{
    Qt::Key_A,
    Qt::Key_D,
}};

const std::map<Settings::AntiAliasing, QString> Config::anti_aliasing_texts_map = {
    {Settings::AntiAliasing::None, QStringLiteral(QT_TRANSLATE_NOOP("GMainWindow", "None"))},
    {Settings::AntiAliasing::Fxaa, QStringLiteral(QT_TRANSLATE_NOOP("GMainWindow", "FXAA"))},
    {Settings::AntiAliasing::Smaa, QStringLiteral(QT_TRANSLATE_NOOP("GMainWindow", "SMAA"))},
};

const std::map<Settings::ScalingFilter, QString> Config::scaling_filter_texts_map = {
    {Settings::ScalingFilter::NearestNeighbor,
     QStringLiteral(QT_TRANSLATE_NOOP("GMainWindow", "Nearest"))},
    {Settings::ScalingFilter::Bilinear,
     QStringLiteral(QT_TRANSLATE_NOOP("GMainWindow", "Bilinear"))},
    {Settings::ScalingFilter::Bicubic, QStringLiteral(QT_TRANSLATE_NOOP("GMainWindow", "Bicubic"))},
    {Settings::ScalingFilter::Gaussian,
     QStringLiteral(QT_TRANSLATE_NOOP("GMainWindow", "Gaussian"))},
    {Settings::ScalingFilter::ScaleForce,
     QStringLiteral(QT_TRANSLATE_NOOP("GMainWindow", "ScaleForce"))},
    {Settings::ScalingFilter::Fsr, QStringLiteral(QT_TRANSLATE_NOOP("GMainWindow", "FSR"))},
};

const std::map<Settings::ConsoleMode, QString> Config::use_docked_mode_texts_map = {
    {Settings::ConsoleMode::Docked, QStringLiteral(QT_TRANSLATE_NOOP("GMainWindow", "Docked"))},
    {Settings::ConsoleMode::Handheld, QStringLiteral(QT_TRANSLATE_NOOP("GMainWindow", "Handheld"))},
};

const std::map<Settings::GpuAccuracy, QString> Config::gpu_accuracy_texts_map = {
    {Settings::GpuAccuracy::Normal, QStringLiteral(QT_TRANSLATE_NOOP("GMainWindow", "Normal"))},
    {Settings::GpuAccuracy::High, QStringLiteral(QT_TRANSLATE_NOOP("GMainWindow", "High"))},
    {Settings::GpuAccuracy::Extreme, QStringLiteral(QT_TRANSLATE_NOOP("GMainWindow", "Extreme"))},
};

const std::map<Settings::RendererBackend, QString> Config::renderer_backend_texts_map = {
    {Settings::RendererBackend::Vulkan, QStringLiteral(QT_TRANSLATE_NOOP("GMainWindow", "Vulkan"))},
    {Settings::RendererBackend::OpenGL, QStringLiteral(QT_TRANSLATE_NOOP("GMainWindow", "OpenGL"))},
    {Settings::RendererBackend::Null, QStringLiteral(QT_TRANSLATE_NOOP("GMainWindow", "Null"))},
};

const std::map<Settings::ShaderBackend, QString> Config::shader_backend_texts_map = {
    {Settings::ShaderBackend::Glsl, QStringLiteral(QT_TRANSLATE_NOOP("GMainWindow", "GLSL"))},
    {Settings::ShaderBackend::Glasm, QStringLiteral(QT_TRANSLATE_NOOP("GMainWindow", "GLASM"))},
    {Settings::ShaderBackend::SpirV, QStringLiteral(QT_TRANSLATE_NOOP("GMainWindow", "SPIRV"))},
};

// This shouldn't have anything except static initializers (no functions). So
// QKeySequence(...).toString() is NOT ALLOWED HERE.
// This must be in alphabetical order according to action name as it must have the same order as
// UISetting::values.shortcuts, which is alphabetically ordered.
// clang-format off
const std::array<UISettings::Shortcut, 23> Config::default_hotkeys{{
    {QStringLiteral(QT_TRANSLATE_NOOP("Hotkeys", "Audio Mute/Unmute")),        QStringLiteral(QT_TRANSLATE_NOOP("Hotkeys", "Main Window")), {QStringLiteral("Ctrl+M"),  QStringLiteral("Home+Dpad_Right"), Qt::WindowShortcut, false}},
    {QStringLiteral(QT_TRANSLATE_NOOP("Hotkeys", "Audio Volume Down")),        QStringLiteral(QT_TRANSLATE_NOOP("Hotkeys", "Main Window")), {QStringLiteral("-"),       QStringLiteral("Home+Dpad_Down"), Qt::ApplicationShortcut, true}},
    {QStringLiteral(QT_TRANSLATE_NOOP("Hotkeys", "Audio Volume Up")),          QStringLiteral(QT_TRANSLATE_NOOP("Hotkeys", "Main Window")), {QStringLiteral("="),       QStringLiteral("Home+Dpad_Up"), Qt::ApplicationShortcut, true}},
    {QStringLiteral(QT_TRANSLATE_NOOP("Hotkeys", "Capture Screenshot")),       QStringLiteral(QT_TRANSLATE_NOOP("Hotkeys", "Main Window")), {QStringLiteral("Ctrl+P"),  QStringLiteral("Screenshot"), Qt::WidgetWithChildrenShortcut, false}},
    {QStringLiteral(QT_TRANSLATE_NOOP("Hotkeys", "Change Adapting Filter")),   QStringLiteral(QT_TRANSLATE_NOOP("Hotkeys", "Main Window")), {QStringLiteral("F8"),      QStringLiteral("Home+L"), Qt::ApplicationShortcut, false}},
    {QStringLiteral(QT_TRANSLATE_NOOP("Hotkeys", "Change Docked Mode")),       QStringLiteral(QT_TRANSLATE_NOOP("Hotkeys", "Main Window")), {QStringLiteral("F10"),     QStringLiteral("Home+X"), Qt::ApplicationShortcut, false}},
    {QStringLiteral(QT_TRANSLATE_NOOP("Hotkeys", "Change GPU Accuracy")),      QStringLiteral(QT_TRANSLATE_NOOP("Hotkeys", "Main Window")), {QStringLiteral("F9"),      QStringLiteral("Home+R"), Qt::ApplicationShortcut, false}},
    {QStringLiteral(QT_TRANSLATE_NOOP("Hotkeys", "Continue/Pause Emulation")), QStringLiteral(QT_TRANSLATE_NOOP("Hotkeys", "Main Window")), {QStringLiteral("F4"),      QStringLiteral("Home+Plus"), Qt::WindowShortcut, false}},
    {QStringLiteral(QT_TRANSLATE_NOOP("Hotkeys", "Exit Fullscreen")),          QStringLiteral(QT_TRANSLATE_NOOP("Hotkeys", "Main Window")), {QStringLiteral("Esc"),     QStringLiteral(""), Qt::WindowShortcut, false}},
    {QStringLiteral(QT_TRANSLATE_NOOP("Hotkeys", "Exit sudachi")),                QStringLiteral(QT_TRANSLATE_NOOP("Hotkeys", "Main Window")), {QStringLiteral("Ctrl+Q"),  QStringLiteral("Home+Minus"), Qt::WindowShortcut, false}},
    {QStringLiteral(QT_TRANSLATE_NOOP("Hotkeys", "Fullscreen")),               QStringLiteral(QT_TRANSLATE_NOOP("Hotkeys", "Main Window")), {QStringLiteral("F11"),     QStringLiteral("Home+B"), Qt::WindowShortcut, false}},
    {QStringLiteral(QT_TRANSLATE_NOOP("Hotkeys", "Load File")),                QStringLiteral(QT_TRANSLATE_NOOP("Hotkeys", "Main Window")), {QStringLiteral("Ctrl+O"),  QStringLiteral(""), Qt::WidgetWithChildrenShortcut, false}},
    {QStringLiteral(QT_TRANSLATE_NOOP("Hotkeys", "Load/Remove Amiibo")),       QStringLiteral(QT_TRANSLATE_NOOP("Hotkeys", "Main Window")), {QStringLiteral("F2"),      QStringLiteral("Home+A"), Qt::WidgetWithChildrenShortcut, false}},
    {QStringLiteral(QT_TRANSLATE_NOOP("Hotkeys", "Restart Emulation")),        QStringLiteral(QT_TRANSLATE_NOOP("Hotkeys", "Main Window")), {QStringLiteral("F6"),      QStringLiteral("R+Plus+Minus"), Qt::WindowShortcut, false}},
    {QStringLiteral(QT_TRANSLATE_NOOP("Hotkeys", "Stop Emulation")),           QStringLiteral(QT_TRANSLATE_NOOP("Hotkeys", "Main Window")), {QStringLiteral("F5"),      QStringLiteral("L+Plus+Minus"), Qt::WindowShortcut, false}},
    {QStringLiteral(QT_TRANSLATE_NOOP("Hotkeys", "TAS Record")),               QStringLiteral(QT_TRANSLATE_NOOP("Hotkeys", "Main Window")), {QStringLiteral("Ctrl+F7"), QStringLiteral(""), Qt::ApplicationShortcut, false}},
    {QStringLiteral(QT_TRANSLATE_NOOP("Hotkeys", "TAS Reset")),                QStringLiteral(QT_TRANSLATE_NOOP("Hotkeys", "Main Window")), {QStringLiteral("Ctrl+F6"), QStringLiteral(""), Qt::ApplicationShortcut, false}},
    {QStringLiteral(QT_TRANSLATE_NOOP("Hotkeys", "TAS Start/Stop")),           QStringLiteral(QT_TRANSLATE_NOOP("Hotkeys", "Main Window")), {QStringLiteral("Ctrl+F5"), QStringLiteral(""), Qt::ApplicationShortcut, false}},
    {QStringLiteral(QT_TRANSLATE_NOOP("Hotkeys", "Toggle Filter Bar")),        QStringLiteral(QT_TRANSLATE_NOOP("Hotkeys", "Main Window")), {QStringLiteral("Ctrl+F"),  QStringLiteral(""), Qt::WindowShortcut, false}},
    {QStringLiteral(QT_TRANSLATE_NOOP("Hotkeys", "Toggle Framerate Limit")),   QStringLiteral(QT_TRANSLATE_NOOP("Hotkeys", "Main Window")), {QStringLiteral("Ctrl+U"),  QStringLiteral("Home+Y"), Qt::ApplicationShortcut, false}},
    {QStringLiteral(QT_TRANSLATE_NOOP("Hotkeys", "Toggle Mouse Panning")),     QStringLiteral(QT_TRANSLATE_NOOP("Hotkeys", "Main Window")), {QStringLiteral("Ctrl+F9"), QStringLiteral(""), Qt::ApplicationShortcut, false}},
    {QStringLiteral(QT_TRANSLATE_NOOP("Hotkeys", "Toggle Renderdoc Capture")), QStringLiteral(QT_TRANSLATE_NOOP("Hotkeys", "Main Window")), {QStringLiteral(""),        QStringLiteral(""), Qt::ApplicationShortcut, false}},
    {QStringLiteral(QT_TRANSLATE_NOOP("Hotkeys", "Toggle Status Bar")),        QStringLiteral(QT_TRANSLATE_NOOP("Hotkeys", "Main Window")), {QStringLiteral("Ctrl+S"),  QStringLiteral(""), Qt::WindowShortcut, false}},
}};
// clang-format on

void Config::Initialize(const std::string& config_name) {
    const auto fs_config_loc = FS::GetSudachiPath(FS::SudachiPath::ConfigDir);
    const auto config_file = fmt::format("{}.ini", config_name);

    switch (type) {
    case ConfigType::GlobalConfig:
        qt_config_loc = FS::PathToUTF8String(fs_config_loc / config_file);
        void(FS::CreateParentDir(qt_config_loc));
        qt_config = std::make_unique<QSettings>(QString::fromStdString(qt_config_loc),
                                                QSettings::IniFormat);
        Reload();
        break;
    case ConfigType::PerGameConfig:
        qt_config_loc =
            FS::PathToUTF8String(fs_config_loc / "custom" / FS::ToU8String(config_file));
        void(FS::CreateParentDir(qt_config_loc));
        qt_config = std::make_unique<QSettings>(QString::fromStdString(qt_config_loc),
                                                QSettings::IniFormat);
        Reload();
        break;
    case ConfigType::InputProfile:
        qt_config_loc = FS::PathToUTF8String(fs_config_loc / "input" / config_file);
        void(FS::CreateParentDir(qt_config_loc));
        qt_config = std::make_unique<QSettings>(QString::fromStdString(qt_config_loc),
                                                QSettings::IniFormat);
        break;
    }
}

bool Config::IsCustomConfig() {
    return type == ConfigType::PerGameConfig;
}

void Config::ReadPlayerValue(std::size_t player_index) {
    const QString player_prefix = [this, player_index] {
        if (type == ConfigType::InputProfile) {
            return QString{};
        } else {
            return QStringLiteral("player_%1_").arg(player_index);
        }
    }();

    auto& player = Settings::values.players.GetValue()[player_index];
    if (IsCustomConfig()) {
        const auto profile_name =
            qt_config->value(QStringLiteral("%1profile_name").arg(player_prefix), QString{})
                .toString()
                .toStdString();
        if (profile_name.empty()) {
            // Use the global input config
            player = Settings::values.players.GetValue(true)[player_index];
            return;
        }
        player.profile_name = profile_name;
    }

    if (player_prefix.isEmpty() && Settings::IsConfiguringGlobal()) {
        const auto controller = static_cast<Settings::ControllerType>(
            qt_config
                ->value(QStringLiteral("%1type").arg(player_prefix),
                        static_cast<u8>(Settings::ControllerType::ProController))
                .toUInt());

        if (controller == Settings::ControllerType::LeftJoycon ||
            controller == Settings::ControllerType::RightJoycon) {
            player.controller_type = controller;
        }
    } else {
        player.connected =
            ReadSetting(QStringLiteral("%1connected").arg(player_prefix), player_index == 0)
                .toBool();

        player.controller_type = static_cast<Settings::ControllerType>(
            qt_config
                ->value(QStringLiteral("%1type").arg(player_prefix),
                        static_cast<u8>(Settings::ControllerType::ProController))
                .toUInt());

        player.vibration_enabled =
            qt_config->value(QStringLiteral("%1vibration_enabled").arg(player_prefix), true)
                .toBool();

        player.vibration_strength =
            qt_config->value(QStringLiteral("%1vibration_strength").arg(player_prefix), 100)
                .toInt();

        player.body_color_left = qt_config
                                     ->value(QStringLiteral("%1body_color_left").arg(player_prefix),
                                             Settings::JOYCON_BODY_NEON_BLUE)
                                     .toUInt();
        player.body_color_right =
            qt_config
                ->value(QStringLiteral("%1body_color_right").arg(player_prefix),
                        Settings::JOYCON_BODY_NEON_RED)
                .toUInt();
        player.button_color_left =
            qt_config
                ->value(QStringLiteral("%1button_color_left").arg(player_prefix),
                        Settings::JOYCON_BUTTONS_NEON_BLUE)
                .toUInt();
        player.button_color_right =
            qt_config
                ->value(QStringLiteral("%1button_color_right").arg(player_prefix),
                        Settings::JOYCON_BUTTONS_NEON_RED)
                .toUInt();
    }

    for (int i = 0; i < Settings::NativeButton::NumButtons; ++i) {
        const std::string default_param = InputCommon::GenerateKeyboardParam(default_buttons[i]);
        auto& player_buttons = player.buttons[i];

        player_buttons = qt_config
                             ->value(QStringLiteral("%1").arg(player_prefix) +
                                         QString::fromUtf8(Settings::NativeButton::mapping[i]),
                                     QString::fromStdString(default_param))
                             .toString()
                             .toStdString();
        if (player_buttons.empty()) {
            player_buttons = default_param;
        }
    }

    for (int i = 0; i < Settings::NativeAnalog::NumAnalogs; ++i) {
        const std::string default_param = InputCommon::GenerateAnalogParamFromKeys(
            default_analogs[i][0], default_analogs[i][1], default_analogs[i][2],
            default_analogs[i][3], default_stick_mod[i], 0.5f);
        auto& player_analogs = player.analogs[i];

        player_analogs = qt_config
                             ->value(QStringLiteral("%1").arg(player_prefix) +
                                         QString::fromUtf8(Settings::NativeAnalog::mapping[i]),
                                     QString::fromStdString(default_param))
                             .toString()
                             .toStdString();
        if (player_analogs.empty()) {
            player_analogs = default_param;
        }
    }

    for (int i = 0; i < Settings::NativeMotion::NumMotions; ++i) {
        const std::string default_param = InputCommon::GenerateKeyboardParam(default_motions[i]);
        auto& player_motions = player.motions[i];

        player_motions = qt_config
                             ->value(QStringLiteral("%1").arg(player_prefix) +
                                         QString::fromUtf8(Settings::NativeMotion::mapping[i]),
                                     QString::fromStdString(default_param))
                             .toString()
                             .toStdString();
        if (player_motions.empty()) {
            player_motions = default_param;
        }
    }
}

void Config::ReadDebugValues() {
    for (int i = 0; i < Settings::NativeButton::NumButtons; ++i) {
        const std::string default_param = InputCommon::GenerateKeyboardParam(default_buttons[i]);
        auto& debug_pad_buttons = Settings::values.debug_pad_buttons[i];

        debug_pad_buttons = qt_config
                                ->value(QStringLiteral("debug_pad_") +
                                            QString::fromUtf8(Settings::NativeButton::mapping[i]),
                                        QString::fromStdString(default_param))
                                .toString()
                                .toStdString();
        if (debug_pad_buttons.empty()) {
            debug_pad_buttons = default_param;
        }
    }

    for (int i = 0; i < Settings::NativeAnalog::NumAnalogs; ++i) {
        const std::string default_param = InputCommon::GenerateAnalogParamFromKeys(
            default_analogs[i][0], default_analogs[i][1], default_analogs[i][2],
            default_analogs[i][3], default_stick_mod[i], 0.5f);
        auto& debug_pad_analogs = Settings::values.debug_pad_analogs[i];

        debug_pad_analogs = qt_config
                                ->value(QStringLiteral("debug_pad_") +
                                            QString::fromUtf8(Settings::NativeAnalog::mapping[i]),
                                        QString::fromStdString(default_param))
                                .toString()
                                .toStdString();
        if (debug_pad_analogs.empty()) {
            debug_pad_analogs = default_param;
        }
    }
}

void Config::ReadTouchscreenValues() {
    Settings::values.touchscreen.enabled =
        ReadSetting(QStringLiteral("touchscreen_enabled"), true).toBool();

    Settings::values.touchscreen.rotation_angle =
        ReadSetting(QStringLiteral("touchscreen_angle"), 0).toUInt();
    Settings::values.touchscreen.diameter_x =
        ReadSetting(QStringLiteral("touchscreen_diameter_x"), 15).toUInt();
    Settings::values.touchscreen.diameter_y =
        ReadSetting(QStringLiteral("touchscreen_diameter_y"), 15).toUInt();
}

void Config::ReadHidbusValues() {
    const std::string default_param = InputCommon::GenerateAnalogParamFromKeys(
        0, 0, default_ringcon_analogs[0], default_ringcon_analogs[1], 0, 0.05f);
    auto& ringcon_analogs = Settings::values.ringcon_analogs;

    ringcon_analogs =
        qt_config->value(QStringLiteral("ring_controller"), QString::fromStdString(default_param))
            .toString()
            .toStdString();
    if (ringcon_analogs.empty()) {
        ringcon_analogs = default_param;
    }
}

void Config::ReadAudioValues() {
    qt_config->beginGroup(QStringLiteral("Audio"));

    ReadCategory(Settings::Category::Audio);
    ReadCategory(Settings::Category::UiAudio);

    qt_config->endGroup();
}

void Config::ReadControlValues() {
    qt_config->beginGroup(QStringLiteral("Controls"));

    ReadCategory(Settings::Category::Controls);

    Settings::values.players.SetGlobal(!IsCustomConfig());
    for (std::size_t p = 0; p < Settings::values.players.GetValue().size(); ++p) {
        ReadPlayerValue(p);
    }

    // Disable docked mode if handheld is selected
    const auto controller_type = Settings::values.players.GetValue()[0].controller_type;
    if (controller_type == Settings::ControllerType::Handheld) {
        Settings::values.use_docked_mode.SetGlobal(!IsCustomConfig());
        Settings::values.use_docked_mode.SetValue(Settings::ConsoleMode::Handheld);
    }

    if (IsCustomConfig()) {
        qt_config->endGroup();
        return;
    }
    ReadDebugValues();
    ReadTouchscreenValues();
    ReadMotionTouchValues();
    ReadHidbusValues();

    qt_config->endGroup();
}

void Config::ReadMotionTouchValues() {
    int num_touch_from_button_maps =
        qt_config->beginReadArray(QStringLiteral("touch_from_button_maps"));

    if (num_touch_from_button_maps > 0) {
        const auto append_touch_from_button_map = [this] {
            Settings::TouchFromButtonMap map;
            map.name = ReadSetting(QStringLiteral("name"), QStringLiteral("default"))
                           .toString()
                           .toStdString();
            const int num_touch_maps = qt_config->beginReadArray(QStringLiteral("entries"));
            map.buttons.reserve(num_touch_maps);
            for (int i = 0; i < num_touch_maps; i++) {
                qt_config->setArrayIndex(i);
                std::string touch_mapping =
                    ReadSetting(QStringLiteral("bind")).toString().toStdString();
                map.buttons.emplace_back(std::move(touch_mapping));
            }
            qt_config->endArray(); // entries
            Settings::values.touch_from_button_maps.emplace_back(std::move(map));
        };

        for (int i = 0; i < num_touch_from_button_maps; ++i) {
            qt_config->setArrayIndex(i);
            append_touch_from_button_map();
        }
    } else {
        Settings::values.touch_from_button_maps.emplace_back(
            Settings::TouchFromButtonMap{"default", {}});
        num_touch_from_button_maps = 1;
    }
    qt_config->endArray();

    Settings::values.touch_from_button_map_index = std::clamp(
        Settings::values.touch_from_button_map_index.GetValue(), 0, num_touch_from_button_maps - 1);
}

void Config::ReadCoreValues() {
    qt_config->beginGroup(QStringLiteral("Core"));

    ReadCategory(Settings::Category::Core);

    qt_config->endGroup();
}

void Config::ReadDataStorageValues() {
    qt_config->beginGroup(QStringLiteral("Data Storage"));

    FS::SetSudachiPath(
        FS::SudachiPath::NANDDir,
        qt_config
            ->value(QStringLiteral("nand_directory"),
                    QString::fromStdString(FS::GetSudachiPathString(FS::SudachiPath::NANDDir)))
            .toString()
            .toStdString());
    FS::SetSudachiPath(
        FS::SudachiPath::SDMCDir,
        qt_config
            ->value(QStringLiteral("sdmc_directory"),
                    QString::fromStdString(FS::GetSudachiPathString(FS::SudachiPath::SDMCDir)))
            .toString()
            .toStdString());
    FS::SetSudachiPath(
        FS::SudachiPath::LoadDir,
        qt_config
            ->value(QStringLiteral("load_directory"),
                    QString::fromStdString(FS::GetSudachiPathString(FS::SudachiPath::LoadDir)))
            .toString()
            .toStdString());
    FS::SetSudachiPath(
        FS::SudachiPath::DumpDir,
        qt_config
            ->value(QStringLiteral("dump_directory"),
                    QString::fromStdString(FS::GetSudachiPathString(FS::SudachiPath::DumpDir)))
            .toString()
            .toStdString());
    FS::SetSudachiPath(
        FS::SudachiPath::TASDir,
        qt_config
            ->value(QStringLiteral("tas_directory"),
                    QString::fromStdString(FS::GetSudachiPathString(FS::SudachiPath::TASDir)))
            .toString()
            .toStdString());

    ReadCategory(Settings::Category::DataStorage);

    qt_config->endGroup();
}

void Config::ReadDebuggingValues() {
    qt_config->beginGroup(QStringLiteral("Debugging"));

    // Intentionally not using the QT default setting as this is intended to be changed in the ini
    Settings::values.record_frame_times =
        qt_config->value(QStringLiteral("record_frame_times"), false).toBool();

    ReadCategory(Settings::Category::Debugging);
    ReadCategory(Settings::Category::DebuggingGraphics);

    qt_config->endGroup();
}

void Config::ReadServiceValues() {
    qt_config->beginGroup(QStringLiteral("Services"));

    ReadCategory(Settings::Category::Services);

    qt_config->endGroup();
}

void Config::ReadDisabledAddOnValues() {
    const auto size = qt_config->beginReadArray(QStringLiteral("DisabledAddOns"));

    for (int i = 0; i < size; ++i) {
        qt_config->setArrayIndex(i);
        const auto title_id = ReadSetting(QStringLiteral("title_id"), 0).toULongLong();
        std::vector<std::string> out;
        const auto d_size = qt_config->beginReadArray(QStringLiteral("disabled"));
        for (int j = 0; j < d_size; ++j) {
            qt_config->setArrayIndex(j);
            out.push_back(ReadSetting(QStringLiteral("d"), QString{}).toString().toStdString());
        }
        qt_config->endArray();
        Settings::values.disabled_addons.insert_or_assign(title_id, out);
    }

    qt_config->endArray();
}

void Config::ReadMiscellaneousValues() {
    qt_config->beginGroup(QStringLiteral("Miscellaneous"));

    ReadCategory(Settings::Category::Miscellaneous);

    qt_config->endGroup();
}

void Config::ReadPathValues() {
    qt_config->beginGroup(QStringLiteral("Paths"));

    UISettings::values.roms_path = ReadSetting(QStringLiteral("romsPath")).toString();
    UISettings::values.symbols_path = ReadSetting(QStringLiteral("symbolsPath")).toString();
    UISettings::values.game_dir_deprecated =
        ReadSetting(QStringLiteral("gameListRootDir"), QStringLiteral(".")).toString();
    UISettings::values.game_dir_deprecated_deepscan =
        ReadSetting(QStringLiteral("gameListDeepScan"), false).toBool();
    const int gamedirs_size = qt_config->beginReadArray(QStringLiteral("gamedirs"));
    for (int i = 0; i < gamedirs_size; ++i) {
        qt_config->setArrayIndex(i);
        UISettings::GameDir game_dir;
        game_dir.path = ReadSetting(QStringLiteral("path")).toString();
        game_dir.deep_scan = ReadSetting(QStringLiteral("deep_scan"), false).toBool();
        game_dir.expanded = ReadSetting(QStringLiteral("expanded"), true).toBool();
        UISettings::values.game_dirs.append(game_dir);
    }
    qt_config->endArray();
    // create NAND and SD card directories if empty, these are not removable through the UI,
    // also carries over old game list settings if present
    if (UISettings::values.game_dirs.isEmpty()) {
        UISettings::GameDir game_dir;
        game_dir.path = QStringLiteral("SDMC");
        game_dir.expanded = true;
        UISettings::values.game_dirs.append(game_dir);
        game_dir.path = QStringLiteral("UserNAND");
        UISettings::values.game_dirs.append(game_dir);
        game_dir.path = QStringLiteral("SysNAND");
        UISettings::values.game_dirs.append(game_dir);
        if (UISettings::values.game_dir_deprecated != QStringLiteral(".")) {
            game_dir.path = UISettings::values.game_dir_deprecated;
            game_dir.deep_scan = UISettings::values.game_dir_deprecated_deepscan;
            UISettings::values.game_dirs.append(game_dir);
        }
    }
    UISettings::values.recent_files = ReadSetting(QStringLiteral("recentFiles")).toStringList();
    UISettings::values.language = ReadSetting(QStringLiteral("language"), QString{}).toString();

    qt_config->endGroup();
}

void Config::ReadCpuValues() {
    qt_config->beginGroup(QStringLiteral("Cpu"));

    ReadCategory(Settings::Category::Cpu);
    ReadCategory(Settings::Category::CpuDebug);
    ReadCategory(Settings::Category::CpuUnsafe);

    qt_config->endGroup();
}

void Config::ReadRendererValues() {
    qt_config->beginGroup(QStringLiteral("Renderer"));

    ReadCategory(Settings::Category::Renderer);
    ReadCategory(Settings::Category::RendererAdvanced);
    ReadCategory(Settings::Category::RendererDebug);

    qt_config->endGroup();
}

void Config::ReadScreenshotValues() {
    qt_config->beginGroup(QStringLiteral("Screenshots"));

    ReadCategory(Settings::Category::Screenshots);
    FS::SetSudachiPath(FS::SudachiPath::ScreenshotsDir,
                       qt_config
                           ->value(QStringLiteral("screenshot_path"),
                                   QString::fromStdString(
                                       FS::GetSudachiPathString(FS::SudachiPath::ScreenshotsDir)))
                           .toString()
                           .toStdString());

    qt_config->endGroup();
}

void Config::ReadShortcutValues() {
    qt_config->beginGroup(QStringLiteral("Shortcuts"));

    for (const auto& [name, group, shortcut] : default_hotkeys) {
        qt_config->beginGroup(group);
        qt_config->beginGroup(name);
        // No longer using ReadSetting for shortcut.second as it inaccurately returns a value of 1
        // for WidgetWithChildrenShortcut which is a value of 3. Needed to fix shortcuts the open
        // a file dialog in windowed mode
        UISettings::values.shortcuts.push_back(
            {name,
             group,
             {ReadSetting(QStringLiteral("KeySeq"), shortcut.keyseq).toString(),
              ReadSetting(QStringLiteral("Controller_KeySeq"), shortcut.controller_keyseq)
                  .toString(),
              shortcut.context, ReadSetting(QStringLiteral("Repeat"), shortcut.repeat).toBool()}});
        qt_config->endGroup();
        qt_config->endGroup();
    }

    qt_config->endGroup();
}

void Config::ReadSystemValues() {
    qt_config->beginGroup(QStringLiteral("System"));

    ReadCategory(Settings::Category::System);
    ReadCategory(Settings::Category::SystemAudio);

    qt_config->endGroup();
}

void Config::ReadUIValues() {
    qt_config->beginGroup(QStringLiteral("UI"));

    UISettings::values.theme =
        ReadSetting(
            QStringLiteral("theme"),
            QString::fromUtf8(UISettings::themes[static_cast<size_t>(default_theme)].second))
            .toString();

    ReadUIGamelistValues();
    ReadUILayoutValues();
    ReadPathValues();
    ReadScreenshotValues();
    ReadShortcutValues();
    ReadMultiplayerValues();

    ReadCategory(Settings::Category::Ui);
    ReadCategory(Settings::Category::UiGeneral);

    qt_config->endGroup();
}

void Config::ReadUIGamelistValues() {
    qt_config->beginGroup(QStringLiteral("UIGameList"));

    ReadCategory(Settings::Category::UiGameList);

    const int favorites_size = qt_config->beginReadArray(QStringLiteral("favorites"));
    for (int i = 0; i < favorites_size; i++) {
        qt_config->setArrayIndex(i);
        UISettings::values.favorited_ids.append(
            ReadSetting(QStringLiteral("program_id")).toULongLong());
    }
    qt_config->endArray();

    qt_config->endGroup();
}

void Config::ReadUILayoutValues() {
    qt_config->beginGroup(QStringLiteral("UILayout"));

    UISettings::values.geometry = ReadSetting(QStringLiteral("geometry")).toByteArray();
    UISettings::values.state = ReadSetting(QStringLiteral("state")).toByteArray();
    UISettings::values.renderwindow_geometry =
        ReadSetting(QStringLiteral("geometryRenderWindow")).toByteArray();
    UISettings::values.gamelist_header_state =
        ReadSetting(QStringLiteral("gameListHeaderState")).toByteArray();

    ReadCategory(Settings::Category::UiLayout);

    qt_config->endGroup();
}

void Config::ReadWebServiceValues() {
    qt_config->beginGroup(QStringLiteral("WebService"));

    ReadCategory(Settings::Category::WebService);

    qt_config->endGroup();
}

void Config::ReadMultiplayerValues() {
    qt_config->beginGroup(QStringLiteral("Multiplayer"));

    ReadCategory(Settings::Category::Multiplayer);

    // Read ban list back
    int size = qt_config->beginReadArray(QStringLiteral("username_ban_list"));
    UISettings::values.multiplayer_ban_list.first.resize(size);
    for (int i = 0; i < size; ++i) {
        qt_config->setArrayIndex(i);
        UISettings::values.multiplayer_ban_list.first[i] =
            ReadSetting(QStringLiteral("username")).toString().toStdString();
    }
    qt_config->endArray();
    size = qt_config->beginReadArray(QStringLiteral("ip_ban_list"));
    UISettings::values.multiplayer_ban_list.second.resize(size);
    for (int i = 0; i < size; ++i) {
        qt_config->setArrayIndex(i);
        UISettings::values.multiplayer_ban_list.second[i] =
            ReadSetting(QStringLiteral("ip")).toString().toStdString();
    }
    qt_config->endArray();

    qt_config->endGroup();
}

void Config::ReadNetworkValues() {
    qt_config->beginGroup(QString::fromStdString("Services"));

    ReadCategory(Settings::Category::Network);

    qt_config->endGroup();
}

void Config::ReadValues() {
    if (global) {
        ReadDataStorageValues();
        ReadDebuggingValues();
        ReadDisabledAddOnValues();
        ReadNetworkValues();
        ReadServiceValues();
        ReadUIValues();
        ReadWebServiceValues();
        ReadMiscellaneousValues();
    }
    ReadControlValues();
    ReadCoreValues();
    ReadCpuValues();
    ReadRendererValues();
    ReadAudioValues();
    ReadSystemValues();
}

void Config::SavePlayerValue(std::size_t player_index) {
    const QString player_prefix = [this, player_index] {
        if (type == ConfigType::InputProfile) {
            return QString{};
        } else {
            return QStringLiteral("player_%1_").arg(player_index);
        }
    }();

    const auto& player = Settings::values.players.GetValue()[player_index];
    if (IsCustomConfig()) {
        if (player.profile_name.empty()) {
            // No custom profile selected
            return;
        }
        WriteSetting(QStringLiteral("%1profile_name").arg(player_prefix),
                     QString::fromStdString(player.profile_name), QString{});
    }

    WriteSetting(QStringLiteral("%1type").arg(player_prefix),
                 static_cast<u8>(player.controller_type),
                 static_cast<u8>(Settings::ControllerType::ProController));

    if (!player_prefix.isEmpty() || !Settings::IsConfiguringGlobal()) {
        WriteSetting(QStringLiteral("%1connected").arg(player_prefix), player.connected,
                     player_index == 0);
        WriteSetting(QStringLiteral("%1vibration_enabled").arg(player_prefix),
                     player.vibration_enabled, true);
        WriteSetting(QStringLiteral("%1vibration_strength").arg(player_prefix),
                     player.vibration_strength, 100);
        WriteSetting(QStringLiteral("%1body_color_left").arg(player_prefix), player.body_color_left,
                     Settings::JOYCON_BODY_NEON_BLUE);
        WriteSetting(QStringLiteral("%1body_color_right").arg(player_prefix),
                     player.body_color_right, Settings::JOYCON_BODY_NEON_RED);
        WriteSetting(QStringLiteral("%1button_color_left").arg(player_prefix),
                     player.button_color_left, Settings::JOYCON_BUTTONS_NEON_BLUE);
        WriteSetting(QStringLiteral("%1button_color_right").arg(player_prefix),
                     player.button_color_right, Settings::JOYCON_BUTTONS_NEON_RED);
    }

    for (int i = 0; i < Settings::NativeButton::NumButtons; ++i) {
        const std::string default_param = InputCommon::GenerateKeyboardParam(default_buttons[i]);
        WriteSetting(QStringLiteral("%1").arg(player_prefix) +
                         QString::fromStdString(Settings::NativeButton::mapping[i]),
                     QString::fromStdString(player.buttons[i]),
                     QString::fromStdString(default_param));
    }
    for (int i = 0; i < Settings::NativeAnalog::NumAnalogs; ++i) {
        const std::string default_param = InputCommon::GenerateAnalogParamFromKeys(
            default_analogs[i][0], default_analogs[i][1], default_analogs[i][2],
            default_analogs[i][3], default_stick_mod[i], 0.5f);
        WriteSetting(QStringLiteral("%1").arg(player_prefix) +
                         QString::fromStdString(Settings::NativeAnalog::mapping[i]),
                     QString::fromStdString(player.analogs[i]),
                     QString::fromStdString(default_param));
    }
    for (int i = 0; i < Settings::NativeMotion::NumMotions; ++i) {
        const std::string default_param = InputCommon::GenerateKeyboardParam(default_motions[i]);
        WriteSetting(QStringLiteral("%1").arg(player_prefix) +
                         QString::fromStdString(Settings::NativeMotion::mapping[i]),
                     QString::fromStdString(player.motions[i]),
                     QString::fromStdString(default_param));
    }
}

void Config::SaveDebugValues() {
    for (int i = 0; i < Settings::NativeButton::NumButtons; ++i) {
        const std::string default_param = InputCommon::GenerateKeyboardParam(default_buttons[i]);
        WriteSetting(QStringLiteral("debug_pad_") +
                         QString::fromStdString(Settings::NativeButton::mapping[i]),
                     QString::fromStdString(Settings::values.debug_pad_buttons[i]),
                     QString::fromStdString(default_param));
    }
    for (int i = 0; i < Settings::NativeAnalog::NumAnalogs; ++i) {
        const std::string default_param = InputCommon::GenerateAnalogParamFromKeys(
            default_analogs[i][0], default_analogs[i][1], default_analogs[i][2],
            default_analogs[i][3], default_stick_mod[i], 0.5f);
        WriteSetting(QStringLiteral("debug_pad_") +
                         QString::fromStdString(Settings::NativeAnalog::mapping[i]),
                     QString::fromStdString(Settings::values.debug_pad_analogs[i]),
                     QString::fromStdString(default_param));
    }
}

void Config::SaveTouchscreenValues() {
    const auto& touchscreen = Settings::values.touchscreen;

    WriteSetting(QStringLiteral("touchscreen_enabled"), touchscreen.enabled, true);

    WriteSetting(QStringLiteral("touchscreen_angle"), touchscreen.rotation_angle, 0);
    WriteSetting(QStringLiteral("touchscreen_diameter_x"), touchscreen.diameter_x, 15);
    WriteSetting(QStringLiteral("touchscreen_diameter_y"), touchscreen.diameter_y, 15);
}

void Config::SaveMotionTouchValues() {
    qt_config->beginWriteArray(QStringLiteral("touch_from_button_maps"));
    for (std::size_t p = 0; p < Settings::values.touch_from_button_maps.size(); ++p) {
        qt_config->setArrayIndex(static_cast<int>(p));
        WriteSetting(QStringLiteral("name"),
                     QString::fromStdString(Settings::values.touch_from_button_maps[p].name),
                     QStringLiteral("default"));
        qt_config->beginWriteArray(QStringLiteral("entries"));
        for (std::size_t q = 0; q < Settings::values.touch_from_button_maps[p].buttons.size();
             ++q) {
            qt_config->setArrayIndex(static_cast<int>(q));
            WriteSetting(
                QStringLiteral("bind"),
                QString::fromStdString(Settings::values.touch_from_button_maps[p].buttons[q]));
        }
        qt_config->endArray();
    }
    qt_config->endArray();
}

void Config::SaveHidbusValues() {
    const std::string default_param = InputCommon::GenerateAnalogParamFromKeys(
        0, 0, default_ringcon_analogs[0], default_ringcon_analogs[1], 0, 0.05f);
    WriteSetting(QStringLiteral("ring_controller"),
                 QString::fromStdString(Settings::values.ringcon_analogs),
                 QString::fromStdString(default_param));
}

void Config::SaveValues() {
    if (global) {
        SaveDataStorageValues();
        SaveDebuggingValues();
        SaveDisabledAddOnValues();
        SaveNetworkValues();
        SaveUIValues();
        SaveWebServiceValues();
        SaveMiscellaneousValues();
    }
    SaveControlValues();
    SaveCoreValues();
    SaveCpuValues();
    SaveRendererValues();
    SaveAudioValues();
    SaveSystemValues();

    qt_config->sync();
}

void Config::SaveAudioValues() {
    qt_config->beginGroup(QStringLiteral("Audio"));

    WriteCategory(Settings::Category::Audio);
    WriteCategory(Settings::Category::UiAudio);

    qt_config->endGroup();
}

void Config::SaveControlValues() {
    qt_config->beginGroup(QStringLiteral("Controls"));

    WriteCategory(Settings::Category::Controls);

    Settings::values.players.SetGlobal(!IsCustomConfig());
    for (std::size_t p = 0; p < Settings::values.players.GetValue().size(); ++p) {
        SavePlayerValue(p);
    }
    if (IsCustomConfig()) {
        qt_config->endGroup();
        return;
    }
    SaveDebugValues();
    SaveTouchscreenValues();
    SaveMotionTouchValues();
    SaveHidbusValues();

    qt_config->endGroup();
}

void Config::SaveCoreValues() {
    qt_config->beginGroup(QStringLiteral("Core"));

    WriteCategory(Settings::Category::Core);

    qt_config->endGroup();
}

void Config::SaveDataStorageValues() {
    qt_config->beginGroup(QStringLiteral("Data Storage"));

    WriteSetting(QStringLiteral("nand_directory"),
                 QString::fromStdString(FS::GetSudachiPathString(FS::SudachiPath::NANDDir)),
                 QString::fromStdString(FS::GetSudachiPathString(FS::SudachiPath::NANDDir)));
    WriteSetting(QStringLiteral("sdmc_directory"),
                 QString::fromStdString(FS::GetSudachiPathString(FS::SudachiPath::SDMCDir)),
                 QString::fromStdString(FS::GetSudachiPathString(FS::SudachiPath::SDMCDir)));
    WriteSetting(QStringLiteral("load_directory"),
                 QString::fromStdString(FS::GetSudachiPathString(FS::SudachiPath::LoadDir)),
                 QString::fromStdString(FS::GetSudachiPathString(FS::SudachiPath::LoadDir)));
    WriteSetting(QStringLiteral("dump_directory"),
                 QString::fromStdString(FS::GetSudachiPathString(FS::SudachiPath::DumpDir)),
                 QString::fromStdString(FS::GetSudachiPathString(FS::SudachiPath::DumpDir)));
    WriteSetting(QStringLiteral("tas_directory"),
                 QString::fromStdString(FS::GetSudachiPathString(FS::SudachiPath::TASDir)),
                 QString::fromStdString(FS::GetSudachiPathString(FS::SudachiPath::TASDir)));

    WriteCategory(Settings::Category::DataStorage);

    qt_config->endGroup();
}

void Config::SaveDebuggingValues() {
    qt_config->beginGroup(QStringLiteral("Debugging"));

    // Intentionally not using the QT default setting as this is intended to be changed in the ini
    qt_config->setValue(QStringLiteral("record_frame_times"), Settings::values.record_frame_times);

    WriteCategory(Settings::Category::Debugging);
    WriteCategory(Settings::Category::DebuggingGraphics);

    qt_config->endGroup();
}

void Config::SaveNetworkValues() {
    qt_config->beginGroup(QStringLiteral("Services"));

    WriteCategory(Settings::Category::Network);

    qt_config->endGroup();
}

void Config::SaveDisabledAddOnValues() {
    qt_config->beginWriteArray(QStringLiteral("DisabledAddOns"));

    int i = 0;
    for (const auto& elem : Settings::values.disabled_addons) {
        qt_config->setArrayIndex(i);
        WriteSetting(QStringLiteral("title_id"), QVariant::fromValue<u64>(elem.first), 0);
        qt_config->beginWriteArray(QStringLiteral("disabled"));
        for (std::size_t j = 0; j < elem.second.size(); ++j) {
            qt_config->setArrayIndex(static_cast<int>(j));
            WriteSetting(QStringLiteral("d"), QString::fromStdString(elem.second[j]), QString{});
        }
        qt_config->endArray();
        ++i;
    }

    qt_config->endArray();
}

void Config::SaveMiscellaneousValues() {
    qt_config->beginGroup(QStringLiteral("Miscellaneous"));

    WriteCategory(Settings::Category::Miscellaneous);

    qt_config->endGroup();
}

void Config::SavePathValues() {
    qt_config->beginGroup(QStringLiteral("Paths"));

    WriteSetting(QStringLiteral("romsPath"), UISettings::values.roms_path);
    WriteSetting(QStringLiteral("symbolsPath"), UISettings::values.symbols_path);
    qt_config->beginWriteArray(QStringLiteral("gamedirs"));
    for (int i = 0; i < UISettings::values.game_dirs.size(); ++i) {
        qt_config->setArrayIndex(i);
        const auto& game_dir = UISettings::values.game_dirs[i];
        WriteSetting(QStringLiteral("path"), game_dir.path);
        WriteSetting(QStringLiteral("deep_scan"), game_dir.deep_scan, false);
        WriteSetting(QStringLiteral("expanded"), game_dir.expanded, true);
    }
    qt_config->endArray();
    WriteSetting(QStringLiteral("recentFiles"), UISettings::values.recent_files);
    WriteSetting(QStringLiteral("language"), UISettings::values.language, QString{});

    qt_config->endGroup();
}

void Config::SaveCpuValues() {
    qt_config->beginGroup(QStringLiteral("Cpu"));

    WriteCategory(Settings::Category::Cpu);
    WriteCategory(Settings::Category::CpuDebug);
    WriteCategory(Settings::Category::CpuUnsafe);

    qt_config->endGroup();
}

void Config::SaveRendererValues() {
    qt_config->beginGroup(QStringLiteral("Renderer"));

    WriteCategory(Settings::Category::Renderer);
    WriteCategory(Settings::Category::RendererAdvanced);
    WriteCategory(Settings::Category::RendererDebug);

    qt_config->endGroup();
}

void Config::SaveScreenshotValues() {
    qt_config->beginGroup(QStringLiteral("Screenshots"));

    WriteSetting(QStringLiteral("screenshot_path"),
                 QString::fromStdString(FS::GetSudachiPathString(FS::SudachiPath::ScreenshotsDir)));
    WriteCategory(Settings::Category::Screenshots);

    qt_config->endGroup();
}

void Config::SaveShortcutValues() {
    qt_config->beginGroup(QStringLiteral("Shortcuts"));

    // Lengths of UISettings::values.shortcuts & default_hotkeys are same.
    // However, their ordering must also be the same.
    for (std::size_t i = 0; i < default_hotkeys.size(); i++) {
        const auto& [name, group, shortcut] = UISettings::values.shortcuts[i];
        const auto& default_hotkey = default_hotkeys[i].shortcut;

        qt_config->beginGroup(group);
        qt_config->beginGroup(name);
        WriteSetting(QStringLiteral("KeySeq"), shortcut.keyseq, default_hotkey.keyseq);
        WriteSetting(QStringLiteral("Controller_KeySeq"), shortcut.controller_keyseq,
                     default_hotkey.controller_keyseq);
        WriteSetting(QStringLiteral("Context"), shortcut.context, default_hotkey.context);
        WriteSetting(QStringLiteral("Repeat"), shortcut.repeat, default_hotkey.repeat);
        qt_config->endGroup();
        qt_config->endGroup();
    }

    qt_config->endGroup();
}

void Config::SaveSystemValues() {
    qt_config->beginGroup(QStringLiteral("System"));

    WriteCategory(Settings::Category::System);
    WriteCategory(Settings::Category::SystemAudio);

    qt_config->endGroup();
}

void Config::SaveUIValues() {
    qt_config->beginGroup(QStringLiteral("UI"));

    WriteCategory(Settings::Category::Ui);
    WriteCategory(Settings::Category::UiGeneral);

    WriteSetting(QStringLiteral("theme"), UISettings::values.theme,
                 QString::fromUtf8(UISettings::themes[static_cast<size_t>(default_theme)].second));

    SaveUIGamelistValues();
    SaveUILayoutValues();
    SavePathValues();
    SaveScreenshotValues();
    SaveShortcutValues();
    SaveMultiplayerValues();

    qt_config->endGroup();
}

void Config::SaveUIGamelistValues() {
    qt_config->beginGroup(QStringLiteral("UIGameList"));

    WriteCategory(Settings::Category::UiGameList);

    qt_config->beginWriteArray(QStringLiteral("favorites"));
    for (int i = 0; i < UISettings::values.favorited_ids.size(); i++) {
        qt_config->setArrayIndex(i);
        WriteSetting(QStringLiteral("program_id"),
                     QVariant::fromValue(UISettings::values.favorited_ids[i]));
    }
    qt_config->endArray();

    qt_config->endGroup();
}

void Config::SaveUILayoutValues() {
    qt_config->beginGroup(QStringLiteral("UILayout"));

    WriteSetting(QStringLiteral("geometry"), UISettings::values.geometry);
    WriteSetting(QStringLiteral("state"), UISettings::values.state);
    WriteSetting(QStringLiteral("geometryRenderWindow"), UISettings::values.renderwindow_geometry);
    WriteSetting(QStringLiteral("gameListHeaderState"), UISettings::values.gamelist_header_state);

    WriteCategory(Settings::Category::UiLayout);

    qt_config->endGroup();
}

void Config::SaveWebServiceValues() {
    qt_config->beginGroup(QStringLiteral("WebService"));

    WriteCategory(Settings::Category::WebService);

    qt_config->endGroup();
}

void Config::SaveMultiplayerValues() {
    qt_config->beginGroup(QStringLiteral("Multiplayer"));

    WriteCategory(Settings::Category::Multiplayer);

    // Write ban list
    qt_config->beginWriteArray(QStringLiteral("username_ban_list"));
    for (std::size_t i = 0; i < UISettings::values.multiplayer_ban_list.first.size(); ++i) {
        qt_config->setArrayIndex(static_cast<int>(i));
        WriteSetting(QStringLiteral("username"),
                     QString::fromStdString(UISettings::values.multiplayer_ban_list.first[i]));
    }
    qt_config->endArray();
    qt_config->beginWriteArray(QStringLiteral("ip_ban_list"));
    for (std::size_t i = 0; i < UISettings::values.multiplayer_ban_list.second.size(); ++i) {
        qt_config->setArrayIndex(static_cast<int>(i));
        WriteSetting(QStringLiteral("ip"),
                     QString::fromStdString(UISettings::values.multiplayer_ban_list.second[i]));
    }
    qt_config->endArray();

    qt_config->endGroup();
}

QVariant Config::ReadSetting(const QString& name) const {
    return qt_config->value(name);
}

QVariant Config::ReadSetting(const QString& name, const QVariant& default_value) const {
    QVariant result;
    if (qt_config->value(name + QStringLiteral("/default"), false).toBool()) {
        result = default_value;
    } else {
        result = qt_config->value(name, default_value);
    }
    return result;
}

void Config::WriteSetting(const QString& name, const QVariant& value) {
    qt_config->setValue(name, value);
}

void Config::WriteSetting(const QString& name, const QVariant& value,
                          const QVariant& default_value) {
    qt_config->setValue(name + QStringLiteral("/default"), value == default_value);
    qt_config->setValue(name, value);
}

void Config::WriteSetting(const QString& name, const QVariant& value, const QVariant& default_value,
                          bool use_global) {
    if (!global) {
        qt_config->setValue(name + QStringLiteral("/use_global"), use_global);
    }
    if (global || !use_global) {
        qt_config->setValue(name + QStringLiteral("/default"), value == default_value);
        qt_config->setValue(name, value);
    }
}

void Config::Reload() {
    ReadValues();
    // To apply default value changes
    SaveValues();
}

void Config::Save() {
    SaveValues();
}

void Config::ReadControlPlayerValue(std::size_t player_index) {
    qt_config->beginGroup(QStringLiteral("Controls"));
    ReadPlayerValue(player_index);
    qt_config->endGroup();
}

void Config::SaveControlPlayerValue(std::size_t player_index) {
    qt_config->beginGroup(QStringLiteral("Controls"));
    SavePlayerValue(player_index);
    qt_config->endGroup();
}

void Config::ClearControlPlayerValues() {
    qt_config->beginGroup(QStringLiteral("Controls"));
    // If key is an empty string, all keys in the current group() are removed.
    qt_config->remove(QString{});
    qt_config->endGroup();
}

const std::string& Config::GetConfigFilePath() const {
    return qt_config_loc;
}

static auto FindRelevantList(Settings::Category category) {
    auto& map = Settings::values.linkage.by_category;
    if (map.contains(category)) {
        return Settings::values.linkage.by_category[category];
    }
    return UISettings::values.linkage.by_category[category];
}

void Config::ReadCategory(Settings::Category category) {
    const auto& settings = FindRelevantList(category);
    std::for_each(settings.begin(), settings.end(),
                  [&](const auto& setting) { ReadSettingGeneric(setting); });
}

void Config::WriteCategory(Settings::Category category) {
    const auto& settings = FindRelevantList(category);
    std::for_each(settings.begin(), settings.end(),
                  [&](const auto& setting) { WriteSettingGeneric(setting); });
}

void Config::ReadSettingGeneric(Settings::BasicSetting* const setting) {
    if (!setting->Save() || (!setting->Switchable() && !global)) {
        return;
    }
    const QString name = QString::fromStdString(setting->GetLabel());
    const auto default_value =
        QVariant::fromValue<QString>(QString::fromStdString(setting->DefaultToString()));

    bool use_global = true;
    if (setting->Switchable() && !global) {
        use_global = qt_config->value(name + QStringLiteral("/use_global"), true).value<bool>();
        setting->SetGlobal(use_global);
    }

    if (global || !use_global) {
        const bool is_default =
            qt_config->value(name + QStringLiteral("/default"), true).value<bool>();
        if (!is_default) {
            setting->LoadString(
                qt_config->value(name, default_value).value<QString>().toStdString());
        } else {
            // Empty string resets the Setting to default
            setting->LoadString("");
        }
    }
}

void Config::WriteSettingGeneric(Settings::BasicSetting* const setting) const {
    if (!setting->Save()) {
        return;
    }
    const QVariant value = QVariant::fromValue(QString::fromStdString(setting->ToString()));
    const QVariant default_value =
        QVariant::fromValue(QString::fromStdString(setting->DefaultToString()));
    const QString label = QString::fromStdString(setting->GetLabel());
    if (setting->Switchable()) {
        if (!global) {
            qt_config->setValue(label + QStringLiteral("/use_global"), setting->UsingGlobal());
        }
        if (global || !setting->UsingGlobal()) {
            qt_config->setValue(label + QStringLiteral("/default"), value == default_value);
            qt_config->setValue(label, value);
        }
    } else if (global) {
        qt_config->setValue(label + QStringLiteral("/default"), value == default_value);
        qt_config->setValue(label, value);
    }
}
