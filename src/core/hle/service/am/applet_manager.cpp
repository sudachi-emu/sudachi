// SPDX-FileCopyrightText: Copyright 2024 sudachi Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/settings.h"
#include "common/uuid.h"
#include "core/core.h"
#include "core/core_timing.h"
#include "core/hle/service/acc/profile_manager.h"
#include "core/hle/service/am/applet_data_broker.h"
#include "core/hle/service/am/applet_manager.h"
#include "core/hle/service/am/frontend/applet_cabinet.h"
#include "core/hle/service/am/frontend/applet_controller.h"
#include "core/hle/service/am/frontend/applet_mii_edit_types.h"
#include "core/hle/service/am/frontend/applet_software_keyboard_types.h"
#include "core/hle/service/am/service/storage.h"
#include "core/hle/service/am/window_system.h"
#include "hid_core/hid_types.h"

namespace Service::AM {

namespace {

constexpr u32 LaunchParameterAccountPreselectedUserMagic = 0xC79497CA;

struct LaunchParameterAccountPreselectedUser {
    u32 magic;
    u32 is_account_selected;
    Common::UUID current_user;
    INSERT_PADDING_BYTES(0x70);
};
static_assert(sizeof(LaunchParameterAccountPreselectedUser) == 0x88);

AppletStorageChannel& InitializeFakeCallerApplet(Core::System& system,
                                                 std::shared_ptr<Applet>& applet) {
    applet->caller_applet_broker = std::make_shared<AppletDataBroker>(system);
    return applet->caller_applet_broker->GetInData();
}

void PushInShowQlaunch(Core::System& system, AppletStorageChannel& channel) {
    const CommonArguments arguments{
        .arguments_version = CommonArgumentVersion::Version3,
        .size = CommonArgumentSize::Version3,
        .library_version = 0,
        .theme_color = ThemeColor::BasicBlack,
        .play_startup_sound = true,
        .system_tick = system.CoreTiming().GetClockTicks(),
    };

    std::vector<u8> argument_data(sizeof(arguments));
    std::memcpy(argument_data.data(), &arguments, sizeof(arguments));
    channel.Push(std::make_shared<IStorage>(system, std::move(argument_data)));
}

void PushInShowAlbum(Core::System& system, AppletStorageChannel& channel) {
    const CommonArguments arguments{
        .arguments_version = CommonArgumentVersion::Version3,
        .size = CommonArgumentSize::Version3,
        .library_version = 1,
        .theme_color = ThemeColor::BasicBlack,
        .play_startup_sound = true,
        .system_tick = system.CoreTiming().GetClockTicks(),
    };

    std::vector<u8> argument_data(sizeof(arguments));
    std::vector<u8> settings_data{2};
    std::memcpy(argument_data.data(), &arguments, sizeof(arguments));
    channel.Push(std::make_shared<IStorage>(system, std::move(argument_data)));
    channel.Push(std::make_shared<IStorage>(system, std::move(settings_data)));
}

void PushInShowController(Core::System& system, AppletStorageChannel& channel) {
    const CommonArguments common_args = {
        .arguments_version = CommonArgumentVersion::Version3,
        .size = CommonArgumentSize::Version3,
        .library_version = static_cast<u32>(Frontend::ControllerAppletVersion::Version8),
        .theme_color = ThemeColor::BasicBlack,
        .play_startup_sound = true,
        .system_tick = system.CoreTiming().GetClockTicks(),
    };

    Frontend::ControllerSupportArgNew user_args = {
        .header = {.player_count_min = 1,
                   .player_count_max = 4,
                   .enable_take_over_connection = true,
                   .enable_left_justify = false,
                   .enable_permit_joy_dual = true,
                   .enable_single_mode = false,
                   .enable_identification_color = false},
        .identification_colors = {},
        .enable_explain_text = false,
        .explain_text = {},
    };

    Frontend::ControllerSupportArgPrivate private_args = {
        .arg_private_size = sizeof(Frontend::ControllerSupportArgPrivate),
        .arg_size = sizeof(Frontend::ControllerSupportArgNew),
        .is_home_menu = true,
        .flag_1 = true,
        .mode = Frontend::ControllerSupportMode::ShowControllerSupport,
        .caller = Frontend::ControllerSupportCaller::
            Application, // switchbrew: Always zero except with
                         // ShowControllerFirmwareUpdateForSystem/ShowControllerKeyRemappingForSystem,
                         // which sets this to the input param
        .style_set = Core::HID::NpadStyleSet::None,
        .joy_hold_type = Frontend::NpadJoyDeviceHoldType::Vertical,
    };
    std::vector<u8> common_args_data(sizeof(common_args));
    std::vector<u8> private_args_data(sizeof(private_args));
    std::vector<u8> user_args_data(sizeof(user_args));

    std::memcpy(common_args_data.data(), &common_args, sizeof(common_args));
    std::memcpy(private_args_data.data(), &private_args, sizeof(private_args));
    std::memcpy(user_args_data.data(), &user_args, sizeof(user_args));

    channel.Push(std::make_shared<IStorage>(system, std::move(common_args_data)));
    channel.Push(std::make_shared<IStorage>(system, std::move(private_args_data)));
    channel.Push(std::make_shared<IStorage>(system, std::move(user_args_data)));
}

void PushInShowCabinetData(Core::System& system, AppletStorageChannel& channel) {
    const CommonArguments arguments{
        .arguments_version = CommonArgumentVersion::Version3,
        .size = CommonArgumentSize::Version3,
        .library_version = static_cast<u32>(Frontend::CabinetAppletVersion::Version1),
        .theme_color = ThemeColor::BasicBlack,
        .play_startup_sound = true,
        .system_tick = system.CoreTiming().GetClockTicks(),
    };

    const Frontend::StartParamForAmiiboSettings amiibo_settings{
        .param_1 = 0,
        .applet_mode = system.GetFrontendAppletHolder().GetCabinetMode(),
        .flags = Frontend::CabinetFlags::None,
        .amiibo_settings_1 = 0,
        .device_handle = 0,
        .tag_info{},
        .register_info{},
        .amiibo_settings_3{},
    };

    std::vector<u8> argument_data(sizeof(arguments));
    std::vector<u8> settings_data(sizeof(amiibo_settings));
    std::memcpy(argument_data.data(), &arguments, sizeof(arguments));
    std::memcpy(settings_data.data(), &amiibo_settings, sizeof(amiibo_settings));
    channel.Push(std::make_shared<IStorage>(system, std::move(argument_data)));
    channel.Push(std::make_shared<IStorage>(system, std::move(settings_data)));
}

void PushInShowMiiEditData(Core::System& system, AppletStorageChannel& channel) {
    struct MiiEditV3 {
        Frontend::MiiEditAppletInputCommon common;
        Frontend::MiiEditAppletInputV3 input;
    };
    static_assert(sizeof(MiiEditV3) == 0x100, "MiiEditV3 has incorrect size.");

    MiiEditV3 mii_arguments{
        .common =
            {
                .version = Frontend::MiiEditAppletVersion::Version3,
                .applet_mode = Frontend::MiiEditAppletMode::ShowMiiEdit,
            },
        .input{},
    };

    std::vector<u8> argument_data(sizeof(mii_arguments));
    std::memcpy(argument_data.data(), &mii_arguments, sizeof(mii_arguments));

    channel.Push(std::make_shared<IStorage>(system, std::move(argument_data)));
}

void PushInShowSoftwareKeyboard(Core::System& system, AppletStorageChannel& channel) {
    const CommonArguments arguments{
        .arguments_version = CommonArgumentVersion::Version3,
        .size = CommonArgumentSize::Version3,
        .library_version = static_cast<u32>(Frontend::SwkbdAppletVersion::Version524301),
        .theme_color = ThemeColor::BasicBlack,
        .play_startup_sound = true,
        .system_tick = system.CoreTiming().GetClockTicks(),
    };

    std::vector<char16_t> initial_string(0);

    const Frontend::SwkbdConfigCommon swkbd_config{
        .type = Frontend::SwkbdType::Qwerty,
        .ok_text{},
        .left_optional_symbol_key{},
        .right_optional_symbol_key{},
        .use_prediction = false,
        .key_disable_flags{},
        .initial_cursor_position = Frontend::SwkbdInitialCursorPosition::Start,
        .header_text{},
        .sub_text{},
        .guide_text{},
        .max_text_length = 500,
        .min_text_length = 0,
        .password_mode = Frontend::SwkbdPasswordMode::Disabled,
        .text_draw_type = Frontend::SwkbdTextDrawType::Box,
        .enable_return_button = true,
        .use_utf8 = false,
        .use_blur_background = true,
        .initial_string_offset{},
        .initial_string_length = static_cast<u32>(initial_string.size()),
        .user_dictionary_offset{},
        .user_dictionary_entries{},
        .use_text_check = false,
    };

    Frontend::SwkbdConfigNew swkbd_config_new{};

    std::vector<u8> argument_data(sizeof(arguments));
    std::vector<u8> swkbd_data(sizeof(swkbd_config) + sizeof(swkbd_config_new));
    std::vector<u8> work_buffer(swkbd_config.initial_string_length * sizeof(char16_t));

    std::memcpy(argument_data.data(), &arguments, sizeof(arguments));
    std::memcpy(swkbd_data.data(), &swkbd_config, sizeof(swkbd_config));
    std::memcpy(swkbd_data.data() + sizeof(swkbd_config), &swkbd_config_new,
                sizeof(Frontend::SwkbdConfigNew));
    std::memcpy(work_buffer.data(), initial_string.data(),
                swkbd_config.initial_string_length * sizeof(char16_t));

    channel.Push(std::make_shared<IStorage>(system, std::move(argument_data)));
    channel.Push(std::make_shared<IStorage>(system, std::move(swkbd_data)));
    channel.Push(std::make_shared<IStorage>(system, std::move(work_buffer)));
}

} // namespace

AppletManager::AppletManager(Core::System& system) : m_system(system) {}
AppletManager::~AppletManager() = default;

void AppletManager::CreateAndInsertByFrontendAppletParameters(
    std::unique_ptr<Process> process, const FrontendAppletParameters& params) {
    {
        std::scoped_lock lk{m_lock};
        m_pending_process = std::move(process);
        m_pending_parameters = params;
    }
    m_cv.notify_all();
}

void AppletManager::RequestExit() {
    std::scoped_lock lk{m_lock};
    if (m_window_system) {
        m_window_system->OnExitRequested();
    }
}

void AppletManager::OperationModeChanged() {
    std::scoped_lock lk{m_lock};
    if (m_window_system) {
        m_window_system->OnOperationModeChanged();
    }
}

void AppletManager::SetWindowSystem(WindowSystem* window_system) {
    std::unique_lock lk{m_lock};

    m_window_system = window_system;
    if (!m_window_system) {
        return;
    }

    m_cv.wait(lk, [&] { return m_pending_process != nullptr; });

    const auto& params = m_pending_parameters;
    auto applet = std::make_shared<Applet>(m_system, std::move(m_pending_process),
                                           params.applet_id == AppletId::Application);

    applet->program_id = params.program_id;
    applet->applet_id = params.applet_id;
    applet->type = params.applet_type;
    applet->previous_program_index = params.previous_program_index;

    // Push UserChannel data from previous application
    if (params.launch_type == LaunchType::ApplicationInitiated) {
        applet->user_channel_launch_parameter.swap(m_system.GetUserChannel());
    }

    // TODO: Read whether we need a preselected user from NACP?
    // TODO: This can be done quite easily from loader
    {
        LaunchParameterAccountPreselectedUser lp{};

        lp.magic = LaunchParameterAccountPreselectedUserMagic;
        lp.is_account_selected = 1;

        Account::ProfileManager profile_manager{};
        const auto uuid = profile_manager.GetUser(static_cast<s32>(Settings::values.current_user));
        ASSERT(uuid.has_value() && uuid->IsValid());
        lp.current_user = *uuid;

        std::vector<u8> buffer(sizeof(LaunchParameterAccountPreselectedUser));
        std::memcpy(buffer.data(), &lp, buffer.size());

        applet->preselected_user_launch_parameter.push_back(std::move(buffer));
    }

    // Starting from frontend, some applets require input data.
    switch (applet->applet_id) {
    case AppletId::QLaunch:
        PushInShowQlaunch(m_system, InitializeFakeCallerApplet(m_system, applet));
        break;
    case AppletId::Cabinet:
        PushInShowCabinetData(m_system, InitializeFakeCallerApplet(m_system, applet));
        break;
    case AppletId::MiiEdit:
        PushInShowMiiEditData(m_system, InitializeFakeCallerApplet(m_system, applet));
        break;
    case AppletId::PhotoViewer:
        PushInShowAlbum(m_system, InitializeFakeCallerApplet(m_system, applet));
        break;
    case AppletId::SoftwareKeyboard:
        PushInShowSoftwareKeyboard(m_system, InitializeFakeCallerApplet(m_system, applet));
        break;
    case AppletId::Controller:
        PushInShowController(m_system, InitializeFakeCallerApplet(m_system, applet));
        break;
    default:
        break;
    }

    // Applet was started by frontend, so it is foreground.
    applet->lifecycle_manager.SetFocusState(FocusState::InFocus);

    if (applet->applet_id == AppletId::QLaunch) {
        applet->lifecycle_manager.SetFocusHandlingMode(false);
        applet->lifecycle_manager.SetOutOfFocusSuspendingEnabled(false);
        m_window_system->TrackApplet(applet, false);
        m_window_system->RequestHomeMenuToGetForeground();
    } else {
        m_window_system->TrackApplet(applet, true);
        m_window_system->RequestApplicationToGetForeground();
    }

    applet->process->Run();
}

} // namespace Service::AM
