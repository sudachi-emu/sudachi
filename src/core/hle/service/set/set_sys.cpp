// SPDX-FileCopyrightText: Copyright 2018 sudachi Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <fstream>

#include "common/assert.h"
#include "common/fs/file.h"
#include "common/fs/fs.h"
#include "common/fs/path_util.h"
#include "common/logging/log.h"
#include "common/settings.h"
#include "common/string_util.h"
#include "core/core.h"
#include "core/file_sys/content_archive.h"
#include "core/file_sys/errors.h"
#include "core/file_sys/nca_metadata.h"
#include "core/file_sys/registered_cache.h"
#include "core/file_sys/romfs.h"
#include "core/file_sys/system_archive/system_archive.h"
#include "core/hle/service/filesystem/filesystem.h"
#include "core/hle/service/ipc_helpers.h"
#include "core/hle/service/set/set.h"
#include "core/hle/service/set/settings_types.h"

namespace Service::Set {

namespace {
constexpr u32 SETTINGS_VERSION{1u};
constexpr auto SETTINGS_MAGIC = Common::MakeMagic('s', 'u', 'd', 'a', '_', 's', 'e', 't');
struct SettingsHeader {
    u64 magic;
    u32 version;
    u32 reserved;
};
} // Anonymous namespace

Result GetFirmwareVersionImpl(FirmwareVersionFormat& out_firmware, Core::System& system,
                              GetFirmwareVersionType type) {
    constexpr u64 FirmwareVersionSystemDataId = 0x0100000000000809;
    auto& fsc = system.GetFileSystemController();

    // Attempt to load version data from disk
    const FileSys::RegisteredCache* bis_system{};
    std::unique_ptr<FileSys::NCA> nca{};
    FileSys::VirtualDir romfs{};

    bis_system = fsc.GetSystemNANDContents();
    if (bis_system) {
        nca = bis_system->GetEntry(FirmwareVersionSystemDataId, FileSys::ContentRecordType::Data);
    }
    if (nca) {
        if (auto nca_romfs = nca->GetRomFS(); nca_romfs) {
            romfs = FileSys::ExtractRomFS(nca_romfs);
        }
    }
    if (!romfs) {
        romfs = FileSys::ExtractRomFS(
            FileSys::SystemArchive::SynthesizeSystemArchive(FirmwareVersionSystemDataId));
    }

    const auto early_exit_failure = [](std::string_view desc, Result code) {
        LOG_ERROR(Service_SET, "General failure while attempting to resolve firmware version ({}).",
                  desc);
        return code;
    };

    const auto ver_file = romfs->GetFile("file");
    if (ver_file == nullptr) {
        return early_exit_failure("The system version archive didn't contain the file 'file'.",
                                  FileSys::ERROR_INVALID_ARGUMENT);
    }

    auto data = ver_file->ReadAllBytes();
    if (data.size() != sizeof(FirmwareVersionFormat)) {
        return early_exit_failure("The system version file 'file' was not the correct size.",
                                  FileSys::ERROR_OUT_OF_BOUNDS);
    }

    std::memcpy(&out_firmware, data.data(), sizeof(FirmwareVersionFormat));

    // If the command is GetFirmwareVersion (as opposed to GetFirmwareVersion2), hardware will
    // zero out the REVISION_MINOR field.
    if (type == GetFirmwareVersionType::Version1) {
        out_firmware.revision_minor = 0;
    }

    return ResultSuccess;
}

bool SET_SYS::LoadSettingsFile(std::filesystem::path& path, auto&& default_func) {
    using settings_type = decltype(default_func());

    if (!Common::FS::CreateDirs(path)) {
        return false;
    }

    auto settings_file = path / "settings.dat";
    auto exists = std::filesystem::exists(settings_file);
    auto file_size_ok = exists && std::filesystem::file_size(settings_file) ==
                                      sizeof(SettingsHeader) + sizeof(settings_type);

    auto ResetToDefault = [&]() {
        auto default_settings{default_func()};

        SettingsHeader hdr{
            .magic = SETTINGS_MAGIC,
            .version = SETTINGS_VERSION,
            .reserved = 0u,
        };

        std::ofstream out_settings_file(settings_file, std::ios::out | std::ios::binary);
        out_settings_file.write(reinterpret_cast<const char*>(&hdr), sizeof(hdr));
        out_settings_file.write(reinterpret_cast<const char*>(&default_settings),
                                sizeof(settings_type));
        out_settings_file.flush();
        out_settings_file.close();
    };

    constexpr auto IsHeaderValid = [](std::ifstream& file) -> bool {
        if (!file.is_open()) {
            return false;
        }
        SettingsHeader hdr{};
        file.read(reinterpret_cast<char*>(&hdr), sizeof(hdr));
        return hdr.magic == SETTINGS_MAGIC && hdr.version == SETTINGS_VERSION;
    };

    if (!exists || !file_size_ok) {
        ResetToDefault();
    }

    std::ifstream file(settings_file, std::ios::binary | std::ios::in);
    if (!IsHeaderValid(file)) {
        file.close();
        ResetToDefault();
        file = std::ifstream(settings_file, std::ios::binary | std::ios::in);
        if (!IsHeaderValid(file)) {
            return false;
        }
    }

    if constexpr (std::is_same_v<settings_type, PrivateSettings>) {
        file.read(reinterpret_cast<char*>(&m_private_settings), sizeof(settings_type));
    } else if constexpr (std::is_same_v<settings_type, DeviceSettings>) {
        file.read(reinterpret_cast<char*>(&m_device_settings), sizeof(settings_type));
    } else if constexpr (std::is_same_v<settings_type, ApplnSettings>) {
        file.read(reinterpret_cast<char*>(&m_appln_settings), sizeof(settings_type));
    } else if constexpr (std::is_same_v<settings_type, SystemSettings>) {
        file.read(reinterpret_cast<char*>(&m_system_settings), sizeof(settings_type));
    } else {
        UNREACHABLE();
    }
    file.close();

    return true;
}

bool SET_SYS::StoreSettingsFile(std::filesystem::path& path, auto& settings) {
    using settings_type = std::decay_t<decltype(settings)>;

    if (!Common::FS::IsDir(path)) {
        return false;
    }

    auto settings_base = path / "settings";
    auto settings_tmp_file = settings_base;
    settings_tmp_file = settings_tmp_file.replace_extension("tmp");
    std::ofstream file(settings_tmp_file, std::ios::binary | std::ios::out);
    if (!file.is_open()) {
        return false;
    }

    SettingsHeader hdr{
        .magic = SETTINGS_MAGIC,
        .version = SETTINGS_VERSION,
        .reserved = 0u,
    };
    file.write(reinterpret_cast<const char*>(&hdr), sizeof(hdr));

    if constexpr (std::is_same_v<settings_type, PrivateSettings>) {
        file.write(reinterpret_cast<const char*>(&m_private_settings), sizeof(settings_type));
    } else if constexpr (std::is_same_v<settings_type, DeviceSettings>) {
        file.write(reinterpret_cast<const char*>(&m_device_settings), sizeof(settings_type));
    } else if constexpr (std::is_same_v<settings_type, ApplnSettings>) {
        file.write(reinterpret_cast<const char*>(&m_appln_settings), sizeof(settings_type));
    } else if constexpr (std::is_same_v<settings_type, SystemSettings>) {
        file.write(reinterpret_cast<const char*>(&m_system_settings), sizeof(settings_type));
    } else {
        UNREACHABLE();
    }
    file.close();

    std::filesystem::rename(settings_tmp_file, settings_base.replace_extension("dat"));

    return true;
}

void SET_SYS::SetLanguageCode(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    m_system_settings.language_code = rp.PopEnum<LanguageCode>();
    SetSaveNeeded();

    LOG_INFO(Service_SET, "called, language_code={}", m_system_settings.language_code);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void SET_SYS::GetFirmwareVersion(HLERequestContext& ctx) {
    LOG_DEBUG(Service_SET, "called");

    FirmwareVersionFormat firmware_data{};
    const auto result =
        GetFirmwareVersionImpl(firmware_data, system, GetFirmwareVersionType::Version1);

    if (result.IsSuccess()) {
        ctx.WriteBuffer(firmware_data);
    }

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(result);
}

void SET_SYS::GetFirmwareVersion2(HLERequestContext& ctx) {
    LOG_DEBUG(Service_SET, "called");

    FirmwareVersionFormat firmware_data{};
    const auto result =
        GetFirmwareVersionImpl(firmware_data, system, GetFirmwareVersionType::Version2);

    if (result.IsSuccess()) {
        ctx.WriteBuffer(firmware_data);
    }

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(result);
}

void SET_SYS::GetExternalSteadyClockSourceId(HLERequestContext& ctx) {
    LOG_INFO(Service_SET, "called");

    Common::UUID id{};
    auto res = GetExternalSteadyClockSourceId(id);

    IPC::ResponseBuilder rb{ctx, 2 + sizeof(Common::UUID) / sizeof(u32)};
    rb.Push(res);
    rb.PushRaw(id);
}

void SET_SYS::SetExternalSteadyClockSourceId(HLERequestContext& ctx) {
    LOG_INFO(Service_SET, "called");

    IPC::RequestParser rp{ctx};
    auto id{rp.PopRaw<Common::UUID>()};

    auto res = SetExternalSteadyClockSourceId(id);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(res);
}

void SET_SYS::GetUserSystemClockContext(HLERequestContext& ctx) {
    LOG_INFO(Service_SET, "called");

    Service::Time::Clock::SystemClockContext context{};
    auto res = GetUserSystemClockContext(context);

    IPC::ResponseBuilder rb{ctx,
                            2 + sizeof(Service::Time::Clock::SystemClockContext) / sizeof(u32)};
    rb.Push(res);
    rb.PushRaw(context);
}

void SET_SYS::SetUserSystemClockContext(HLERequestContext& ctx) {
    LOG_INFO(Service_SET, "called");

    IPC::RequestParser rp{ctx};
    auto context{rp.PopRaw<Service::Time::Clock::SystemClockContext>()};

    auto res = SetUserSystemClockContext(context);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(res);
}

void SET_SYS::GetAccountSettings(HLERequestContext& ctx) {
    LOG_INFO(Service_SET, "called");

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.PushRaw(m_system_settings.account_settings);
}

void SET_SYS::SetAccountSettings(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    m_system_settings.account_settings = rp.PopRaw<AccountSettings>();
    SetSaveNeeded();

    LOG_INFO(Service_SET, "called, account_settings_flags={}",
             m_system_settings.account_settings.flags);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void SET_SYS::GetEulaVersions(HLERequestContext& ctx) {
    LOG_INFO(Service_SET, "called");

    ctx.WriteBuffer(m_system_settings.eula_versions);

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.Push(m_system_settings.eula_version_count);
}

void SET_SYS::SetEulaVersions(HLERequestContext& ctx) {
    const auto elements = ctx.GetReadBufferNumElements<EulaVersion>();
    const auto buffer_data = ctx.ReadBuffer();

    LOG_INFO(Service_SET, "called, elements={}", elements);
    ASSERT(elements <= m_system_settings.eula_versions.size());

    m_system_settings.eula_version_count = static_cast<u32>(elements);
    std::memcpy(&m_system_settings.eula_versions, buffer_data.data(),
                sizeof(EulaVersion) * elements);
    SetSaveNeeded();

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void SET_SYS::GetColorSetId(HLERequestContext& ctx) {
    LOG_DEBUG(Service_SET, "called");

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.PushEnum(m_system_settings.color_set_id);
}

void SET_SYS::SetColorSetId(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    m_system_settings.color_set_id = rp.PopEnum<ColorSet>();
    SetSaveNeeded();

    LOG_DEBUG(Service_SET, "called, color_set={}", m_system_settings.color_set_id);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void SET_SYS::GetNotificationSettings(HLERequestContext& ctx) {
    LOG_INFO(Service_SET, "called");

    IPC::ResponseBuilder rb{ctx, 8};
    rb.Push(ResultSuccess);
    rb.PushRaw(m_system_settings.notification_settings);
}

void SET_SYS::SetNotificationSettings(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    m_system_settings.notification_settings = rp.PopRaw<NotificationSettings>();
    SetSaveNeeded();

    LOG_INFO(Service_SET, "called, flags={}, volume={}, head_time={}:{}, tailt_time={}:{}",
             m_system_settings.notification_settings.flags.raw,
             m_system_settings.notification_settings.volume,
             m_system_settings.notification_settings.start_time.hour,
             m_system_settings.notification_settings.start_time.minute,
             m_system_settings.notification_settings.stop_time.hour,
             m_system_settings.notification_settings.stop_time.minute);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void SET_SYS::GetAccountNotificationSettings(HLERequestContext& ctx) {
    LOG_INFO(Service_SET, "called");

    ctx.WriteBuffer(m_system_settings.account_notification_settings);

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.Push(m_system_settings.account_notification_settings_count);
}

void SET_SYS::SetAccountNotificationSettings(HLERequestContext& ctx) {
    const auto elements = ctx.GetReadBufferNumElements<AccountNotificationSettings>();
    const auto buffer_data = ctx.ReadBuffer();

    LOG_INFO(Service_SET, "called, elements={}", elements);

    ASSERT(elements <= m_system_settings.account_notification_settings.size());

    m_system_settings.account_notification_settings_count = static_cast<u32>(elements);
    std::memcpy(&m_system_settings.account_notification_settings, buffer_data.data(),
                elements * sizeof(AccountNotificationSettings));
    SetSaveNeeded();

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

// FIXME: implement support for the real system_settings.ini

template <typename T>
static std::vector<u8> ToBytes(const T& value) {
    static_assert(std::is_trivially_copyable_v<T>);

    const auto* begin = reinterpret_cast<const u8*>(&value);
    const auto* end = begin + sizeof(T);

    return std::vector<u8>(begin, end);
}

using Settings =
    std::map<std::string, std::map<std::string, std::vector<u8>, std::less<>>, std::less<>>;

static Settings GetSettings() {
    Settings ret;

    ret["hbloader"]["applet_heap_size"] = ToBytes(u64{0x0});
    ret["hbloader"]["applet_heap_reservation_size"] = ToBytes(u64{0x8600000});

    // Time
    ret["time"]["notify_time_to_fs_interval_seconds"] = ToBytes(s32{600});
    ret["time"]["standard_network_clock_sufficient_accuracy_minutes"] =
        ToBytes(s32{43200}); // 30 days
    ret["time"]["standard_steady_clock_rtc_update_interval_minutes"] = ToBytes(s32{5});
    ret["time"]["standard_steady_clock_test_offset_minutes"] = ToBytes(s32{0});
    ret["time"]["standard_user_clock_initial_year"] = ToBytes(s32{2023});

    return ret;
}

void SET_SYS::GetSettingsItemValueSize(HLERequestContext& ctx) {
    LOG_DEBUG(Service_SET, "called");

    // The category of the setting. This corresponds to the top-level keys of
    // system_settings.ini.
    const auto setting_category_buf{ctx.ReadBuffer(0)};
    const std::string setting_category{setting_category_buf.begin(), setting_category_buf.end()};

    // The name of the setting. This corresponds to the second-level keys of
    // system_settings.ini.
    const auto setting_name_buf{ctx.ReadBuffer(1)};
    const std::string setting_name{setting_name_buf.begin(), setting_name_buf.end()};

    auto settings{GetSettings()};
    u64 response_size{0};

    if (settings.contains(setting_category) && settings[setting_category].contains(setting_name)) {
        response_size = settings[setting_category][setting_name].size();
    }

    IPC::ResponseBuilder rb{ctx, 4};
    rb.Push(response_size == 0 ? ResultUnknown : ResultSuccess);
    rb.Push(response_size);
}

void SET_SYS::GetSettingsItemValue(HLERequestContext& ctx) {
    // The category of the setting. This corresponds to the top-level keys of
    // system_settings.ini.
    const auto setting_category_buf{ctx.ReadBuffer(0)};
    const std::string setting_category{setting_category_buf.begin(), setting_category_buf.end()};

    // The name of the setting. This corresponds to the second-level keys of
    // system_settings.ini.
    const auto setting_name_buf{ctx.ReadBuffer(1)};
    const std::string setting_name{setting_name_buf.begin(), setting_name_buf.end()};

    std::vector<u8> value;
    auto response = GetSettingsItemValue(value, setting_category, setting_name);

    LOG_INFO(Service_SET, "called. category={}, name={} -- res=0x{:X}", setting_category,
             setting_name, response.raw);

    ctx.WriteBuffer(value.data(), value.size());

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(response);
}

void SET_SYS::GetTvSettings(HLERequestContext& ctx) {
    LOG_INFO(Service_SET, "called");

    IPC::ResponseBuilder rb{ctx, 10};
    rb.Push(ResultSuccess);
    rb.PushRaw(m_system_settings.tv_settings);
}

void SET_SYS::SetTvSettings(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    m_system_settings.tv_settings = rp.PopRaw<TvSettings>();
    SetSaveNeeded();

    LOG_INFO(Service_SET,
             "called, flags={}, cmu_mode={}, constrast_ratio={}, hdmi_content_type={}, "
             "rgb_range={}, tv_gama={}, tv_resolution={}, tv_underscan={}",
             m_system_settings.tv_settings.flags.raw, m_system_settings.tv_settings.cmu_mode,
             m_system_settings.tv_settings.constrast_ratio,
             m_system_settings.tv_settings.hdmi_content_type,
             m_system_settings.tv_settings.rgb_range, m_system_settings.tv_settings.tv_gama,
             m_system_settings.tv_settings.tv_resolution,
             m_system_settings.tv_settings.tv_underscan);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void SET_SYS::GetDebugModeFlag(HLERequestContext& ctx) {
    LOG_DEBUG(Service_SET, "called");

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.Push<u32>(0);
}

void SET_SYS::GetQuestFlag(HLERequestContext& ctx) {
    LOG_WARNING(Service_SET, "(STUBBED) called");

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.PushEnum(QuestFlag::Retail);
}

void SET_SYS::GetDeviceTimeZoneLocationName(HLERequestContext& ctx) {
    LOG_WARNING(Service_SET, "called");

    Service::Time::TimeZone::LocationName name{};
    auto res = GetDeviceTimeZoneLocationName(name);

    IPC::ResponseBuilder rb{ctx, 2 + sizeof(Service::Time::TimeZone::LocationName) / sizeof(u32)};
    rb.Push(res);
    rb.PushRaw<Service::Time::TimeZone::LocationName>(name);
}

void SET_SYS::SetDeviceTimeZoneLocationName(HLERequestContext& ctx) {
    LOG_WARNING(Service_SET, "called");

    IPC::RequestParser rp{ctx};
    auto name{rp.PopRaw<Service::Time::TimeZone::LocationName>()};

    auto res = SetDeviceTimeZoneLocationName(name);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(res);
}

void SET_SYS::SetRegionCode(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    m_system_settings.region_code = rp.PopEnum<RegionCode>();
    SetSaveNeeded();

    LOG_INFO(Service_SET, "called, region_code={}", m_system_settings.region_code);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void SET_SYS::GetNetworkSystemClockContext(HLERequestContext& ctx) {
    LOG_INFO(Service_SET, "called");

    Service::Time::Clock::SystemClockContext context{};
    auto res = GetNetworkSystemClockContext(context);

    IPC::ResponseBuilder rb{ctx,
                            2 + sizeof(Service::Time::Clock::SystemClockContext) / sizeof(u32)};
    rb.Push(res);
    rb.PushRaw(context);
}

void SET_SYS::SetNetworkSystemClockContext(HLERequestContext& ctx) {
    LOG_INFO(Service_SET, "called");

    IPC::RequestParser rp{ctx};
    auto context{rp.PopRaw<Service::Time::Clock::SystemClockContext>()};

    auto res = SetNetworkSystemClockContext(context);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(res);
}

void SET_SYS::IsUserSystemClockAutomaticCorrectionEnabled(HLERequestContext& ctx) {
    LOG_INFO(Service_SET, "called");

    bool enabled{};
    auto res = IsUserSystemClockAutomaticCorrectionEnabled(enabled);

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(res);
    rb.PushRaw(enabled);
}

void SET_SYS::SetUserSystemClockAutomaticCorrectionEnabled(HLERequestContext& ctx) {
    LOG_INFO(Service_SET, "called");

    IPC::RequestParser rp{ctx};
    auto enabled{rp.Pop<bool>()};

    auto res = SetUserSystemClockAutomaticCorrectionEnabled(enabled);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(res);
}

void SET_SYS::GetPrimaryAlbumStorage(HLERequestContext& ctx) {
    LOG_WARNING(Service_SET, "(STUBBED) called");

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.PushEnum(PrimaryAlbumStorage::SdCard);
}

void SET_SYS::GetSleepSettings(HLERequestContext& ctx) {
    LOG_INFO(Service_SET, "called");

    IPC::ResponseBuilder rb{ctx, 5};
    rb.Push(ResultSuccess);
    rb.PushRaw(m_system_settings.sleep_settings);
}

void SET_SYS::SetSleepSettings(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    m_system_settings.sleep_settings = rp.PopRaw<SleepSettings>();
    SetSaveNeeded();

    LOG_INFO(Service_SET, "called, flags={}, handheld_sleep_plan={}, console_sleep_plan={}",
             m_system_settings.sleep_settings.flags.raw,
             m_system_settings.sleep_settings.handheld_sleep_plan,
             m_system_settings.sleep_settings.console_sleep_plan);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void SET_SYS::GetInitialLaunchSettings(HLERequestContext& ctx) {
    LOG_INFO(Service_SET, "called");
    IPC::ResponseBuilder rb{ctx, 10};
    rb.Push(ResultSuccess);
    rb.PushRaw(m_system_settings.initial_launch_settings_packed);
}

void SET_SYS::SetInitialLaunchSettings(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    auto inital_launch_settings = rp.PopRaw<InitialLaunchSettings>();

    m_system_settings.initial_launch_settings_packed.flags = inital_launch_settings.flags;
    m_system_settings.initial_launch_settings_packed.timestamp = inital_launch_settings.timestamp;
    SetSaveNeeded();

    LOG_INFO(Service_SET, "called, flags={}, timestamp={}",
             m_system_settings.initial_launch_settings_packed.flags.raw,
             m_system_settings.initial_launch_settings_packed.timestamp.time_point);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void SET_SYS::GetDeviceNickName(HLERequestContext& ctx) {
    LOG_DEBUG(Service_SET, "called");

    ctx.WriteBuffer(::Settings::values.device_name.GetValue());

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void SET_SYS::SetDeviceNickName(HLERequestContext& ctx) {
    const std::string device_name = Common::StringFromBuffer(ctx.ReadBuffer());

    LOG_INFO(Service_SET, "called, device_name={}", device_name);

    ::Settings::values.device_name = device_name;

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void SET_SYS::GetProductModel(HLERequestContext& ctx) {
    const u32 product_model = 1;

    LOG_WARNING(Service_SET, "(STUBBED) called, product_model={}", product_model);
    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.Push(product_model);
}

void SET_SYS::GetMiiAuthorId(HLERequestContext& ctx) {
    const auto author_id = Common::UUID::MakeDefault();

    LOG_WARNING(Service_SET, "(STUBBED) called, author_id={}", author_id.FormattedString());

    IPC::ResponseBuilder rb{ctx, 6};
    rb.Push(ResultSuccess);
    rb.PushRaw(author_id);
}

void SET_SYS::GetAutoUpdateEnableFlag(HLERequestContext& ctx) {
    u8 auto_update_flag{};

    LOG_WARNING(Service_SET, "(STUBBED) called, auto_update_flag={}", auto_update_flag);

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.Push(auto_update_flag);
}

void SET_SYS::GetBatteryPercentageFlag(HLERequestContext& ctx) {
    u8 battery_percentage_flag{1};

    LOG_WARNING(Service_SET, "(STUBBED) called, battery_percentage_flag={}",
                battery_percentage_flag);

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.Push(battery_percentage_flag);
}

void SET_SYS::SetExternalSteadyClockInternalOffset(HLERequestContext& ctx) {
    LOG_DEBUG(Service_SET, "called.");

    IPC::RequestParser rp{ctx};
    auto offset{rp.Pop<s64>()};

    auto res = SetExternalSteadyClockInternalOffset(offset);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(res);
}

void SET_SYS::GetExternalSteadyClockInternalOffset(HLERequestContext& ctx) {
    LOG_DEBUG(Service_SET, "called.");

    s64 offset{};
    auto res = GetExternalSteadyClockInternalOffset(offset);

    IPC::ResponseBuilder rb{ctx, 4};
    rb.Push(res);
    rb.Push(offset);
}

void SET_SYS::GetErrorReportSharePermission(HLERequestContext& ctx) {
    LOG_WARNING(Service_SET, "(STUBBED) called");

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.PushEnum(ErrorReportSharePermission::Denied);
}

void SET_SYS::GetAppletLaunchFlags(HLERequestContext& ctx) {
    LOG_INFO(Service_SET, "called, applet_launch_flag={}", m_system_settings.applet_launch_flag);

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.Push(m_system_settings.applet_launch_flag);
}

void SET_SYS::SetAppletLaunchFlags(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    m_system_settings.applet_launch_flag = rp.Pop<u32>();
    SetSaveNeeded();

    LOG_INFO(Service_SET, "called, applet_launch_flag={}", m_system_settings.applet_launch_flag);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void SET_SYS::GetKeyboardLayout(HLERequestContext& ctx) {
    const auto language_code =
        available_language_codes[static_cast<s32>(::Settings::values.language_index.GetValue())];
    const auto key_code =
        std::find_if(language_to_layout.cbegin(), language_to_layout.cend(),
                     [=](const auto& element) { return element.first == language_code; });

    KeyboardLayout selected_keyboard_layout = KeyboardLayout::EnglishUs;
    if (key_code != language_to_layout.end()) {
        selected_keyboard_layout = key_code->second;
    }

    LOG_INFO(Service_SET, "called, selected_keyboard_layout={}", selected_keyboard_layout);

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.Push(static_cast<u32>(selected_keyboard_layout));
}

void SET_SYS::GetRebootlessSystemUpdateVersion(HLERequestContext& ctx) {
    LOG_WARNING(Service_SET, "(STUBBED) called.");

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.PushRaw<RebootlessSystemUpdateVersion>(m_system_settings.rebootless_system_version);
}

void SET_SYS::GetDeviceTimeZoneLocationUpdatedTime(HLERequestContext& ctx) {
    LOG_WARNING(Service_SET, "called.");

    Service::Time::Clock::SteadyClockTimePoint time_point{};
    auto res = GetDeviceTimeZoneLocationUpdatedTime(time_point);

    IPC::ResponseBuilder rb{ctx, 4};
    rb.Push(res);
    rb.PushRaw<Service::Time::Clock::SteadyClockTimePoint>(time_point);
}

void SET_SYS::SetDeviceTimeZoneLocationUpdatedTime(HLERequestContext& ctx) {
    LOG_WARNING(Service_SET, "called.");

    IPC::RequestParser rp{ctx};
    auto time_point{rp.PopRaw<Service::Time::Clock::SteadyClockTimePoint>()};

    auto res = SetDeviceTimeZoneLocationUpdatedTime(time_point);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(res);
}

void SET_SYS::GetUserSystemClockAutomaticCorrectionUpdatedTime(HLERequestContext& ctx) {
    LOG_WARNING(Service_SET, "called.");

    Service::Time::Clock::SteadyClockTimePoint time_point{};
    auto res = GetUserSystemClockAutomaticCorrectionUpdatedTime(time_point);

    IPC::ResponseBuilder rb{ctx, 4};
    rb.Push(res);
    rb.PushRaw<Service::Time::Clock::SteadyClockTimePoint>(time_point);
}

void SET_SYS::SetUserSystemClockAutomaticCorrectionUpdatedTime(HLERequestContext& ctx) {
    LOG_WARNING(Service_SET, "called.");

    IPC::RequestParser rp{ctx};
    auto time_point{rp.PopRaw<Service::Time::Clock::SteadyClockTimePoint>()};

    auto res = SetUserSystemClockAutomaticCorrectionUpdatedTime(time_point);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(res);
}

void SET_SYS::GetChineseTraditionalInputMethod(HLERequestContext& ctx) {
    LOG_WARNING(Service_SET, "(STUBBED) called");

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.PushEnum(ChineseTraditionalInputMethod::Unknown0);
}

void SET_SYS::GetHomeMenuScheme(HLERequestContext& ctx) {
    LOG_DEBUG(Service_SET, "(STUBBED) called");

    const HomeMenuScheme default_color = {
        .main = 0xFF323232,
        .back = 0xFF323232,
        .sub = 0xFFFFFFFF,
        .bezel = 0xFFFFFFFF,
        .extra = 0xFF000000,
    };

    IPC::ResponseBuilder rb{ctx, 2 + sizeof(HomeMenuScheme) / sizeof(u32)};
    rb.Push(ResultSuccess);
    rb.PushRaw(default_color);
}

void SET_SYS::GetHomeMenuSchemeModel(HLERequestContext& ctx) {
    LOG_WARNING(Service_SET, "(STUBBED) called");

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.Push(0);
}

void SET_SYS::GetFieldTestingFlag(HLERequestContext& ctx) {
    LOG_WARNING(Service_SET, "(STUBBED) called");

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.Push<u8>(false);
}

SET_SYS::SET_SYS(Core::System& system_) : ServiceFramework{system_, "set:sys"}, m_system{system} {
    // clang-format off
    static const FunctionInfo functions[] = {
        {0, &SET_SYS::SetLanguageCode, "SetLanguageCode"},
        {1, nullptr, "SetNetworkSettings"},
        {2, nullptr, "GetNetworkSettings"},
        {3, &SET_SYS::GetFirmwareVersion, "GetFirmwareVersion"},
        {4, &SET_SYS::GetFirmwareVersion2, "GetFirmwareVersion2"},
        {5, nullptr, "GetFirmwareVersionDigest"},
        {7, nullptr, "GetLockScreenFlag"},
        {8, nullptr, "SetLockScreenFlag"},
        {9, nullptr, "GetBacklightSettings"},
        {10, nullptr, "SetBacklightSettings"},
        {11, nullptr, "SetBluetoothDevicesSettings"},
        {12, nullptr, "GetBluetoothDevicesSettings"},
        {13, &SET_SYS::GetExternalSteadyClockSourceId, "GetExternalSteadyClockSourceId"},
        {14, &SET_SYS::SetExternalSteadyClockSourceId, "SetExternalSteadyClockSourceId"},
        {15, &SET_SYS::GetUserSystemClockContext, "GetUserSystemClockContext"},
        {16, &SET_SYS::SetUserSystemClockContext, "SetUserSystemClockContext"},
        {17, &SET_SYS::GetAccountSettings, "GetAccountSettings"},
        {18, &SET_SYS::SetAccountSettings, "SetAccountSettings"},
        {19, nullptr, "GetAudioVolume"},
        {20, nullptr, "SetAudioVolume"},
        {21, &SET_SYS::GetEulaVersions, "GetEulaVersions"},
        {22, &SET_SYS::SetEulaVersions, "SetEulaVersions"},
        {23, &SET_SYS::GetColorSetId, "GetColorSetId"},
        {24, &SET_SYS::SetColorSetId, "SetColorSetId"},
        {25, nullptr, "GetConsoleInformationUploadFlag"},
        {26, nullptr, "SetConsoleInformationUploadFlag"},
        {27, nullptr, "GetAutomaticApplicationDownloadFlag"},
        {28, nullptr, "SetAutomaticApplicationDownloadFlag"},
        {29, &SET_SYS::GetNotificationSettings, "GetNotificationSettings"},
        {30, &SET_SYS::SetNotificationSettings, "SetNotificationSettings"},
        {31, &SET_SYS::GetAccountNotificationSettings, "GetAccountNotificationSettings"},
        {32, &SET_SYS::SetAccountNotificationSettings, "SetAccountNotificationSettings"},
        {35, nullptr, "GetVibrationMasterVolume"},
        {36, nullptr, "SetVibrationMasterVolume"},
        {37, &SET_SYS::GetSettingsItemValueSize, "GetSettingsItemValueSize"},
        {38, &SET_SYS::GetSettingsItemValue, "GetSettingsItemValue"},
        {39, &SET_SYS::GetTvSettings, "GetTvSettings"},
        {40, &SET_SYS::SetTvSettings, "SetTvSettings"},
        {41, nullptr, "GetEdid"},
        {42, nullptr, "SetEdid"},
        {43, nullptr, "GetAudioOutputMode"},
        {44, nullptr, "SetAudioOutputMode"},
        {45, nullptr, "IsForceMuteOnHeadphoneRemoved"},
        {46, nullptr, "SetForceMuteOnHeadphoneRemoved"},
        {47, &SET_SYS::GetQuestFlag, "GetQuestFlag"},
        {48, nullptr, "SetQuestFlag"},
        {49, nullptr, "GetDataDeletionSettings"},
        {50, nullptr, "SetDataDeletionSettings"},
        {51, nullptr, "GetInitialSystemAppletProgramId"},
        {52, nullptr, "GetOverlayDispProgramId"},
        {53, &SET_SYS::GetDeviceTimeZoneLocationName, "GetDeviceTimeZoneLocationName"},
        {54, &SET_SYS::SetDeviceTimeZoneLocationName, "SetDeviceTimeZoneLocationName"},
        {55, nullptr, "GetWirelessCertificationFileSize"},
        {56, nullptr, "GetWirelessCertificationFile"},
        {57, &SET_SYS::SetRegionCode, "SetRegionCode"},
        {58, &SET_SYS::GetNetworkSystemClockContext, "GetNetworkSystemClockContext"},
        {59, &SET_SYS::SetNetworkSystemClockContext, "SetNetworkSystemClockContext"},
        {60, &SET_SYS::IsUserSystemClockAutomaticCorrectionEnabled, "IsUserSystemClockAutomaticCorrectionEnabled"},
        {61, &SET_SYS::SetUserSystemClockAutomaticCorrectionEnabled, "SetUserSystemClockAutomaticCorrectionEnabled"},
        {62, &SET_SYS::GetDebugModeFlag, "GetDebugModeFlag"},
        {63, &SET_SYS::GetPrimaryAlbumStorage, "GetPrimaryAlbumStorage"},
        {64, nullptr, "SetPrimaryAlbumStorage"},
        {65, nullptr, "GetUsb30EnableFlag"},
        {66, nullptr, "SetUsb30EnableFlag"},
        {67, nullptr, "GetBatteryLot"},
        {68, nullptr, "GetSerialNumber"},
        {69, nullptr, "GetNfcEnableFlag"},
        {70, nullptr, "SetNfcEnableFlag"},
        {71, &SET_SYS::GetSleepSettings, "GetSleepSettings"},
        {72, &SET_SYS::SetSleepSettings, "SetSleepSettings"},
        {73, nullptr, "GetWirelessLanEnableFlag"},
        {74, nullptr, "SetWirelessLanEnableFlag"},
        {75, &SET_SYS::GetInitialLaunchSettings, "GetInitialLaunchSettings"},
        {76, &SET_SYS::SetInitialLaunchSettings, "SetInitialLaunchSettings"},
        {77, &SET_SYS::GetDeviceNickName, "GetDeviceNickName"},
        {78, &SET_SYS::SetDeviceNickName, "SetDeviceNickName"},
        {79, &SET_SYS::GetProductModel, "GetProductModel"},
        {80, nullptr, "GetLdnChannel"},
        {81, nullptr, "SetLdnChannel"},
        {82, nullptr, "AcquireTelemetryDirtyFlagEventHandle"},
        {83, nullptr, "GetTelemetryDirtyFlags"},
        {84, nullptr, "GetPtmBatteryLot"},
        {85, nullptr, "SetPtmBatteryLot"},
        {86, nullptr, "GetPtmFuelGaugeParameter"},
        {87, nullptr, "SetPtmFuelGaugeParameter"},
        {88, nullptr, "GetBluetoothEnableFlag"},
        {89, nullptr, "SetBluetoothEnableFlag"},
        {90, &SET_SYS::GetMiiAuthorId, "GetMiiAuthorId"},
        {91, nullptr, "SetShutdownRtcValue"},
        {92, nullptr, "GetShutdownRtcValue"},
        {93, nullptr, "AcquireFatalDirtyFlagEventHandle"},
        {94, nullptr, "GetFatalDirtyFlags"},
        {95, &SET_SYS::GetAutoUpdateEnableFlag, "GetAutoUpdateEnableFlag"},
        {96, nullptr, "SetAutoUpdateEnableFlag"},
        {97, nullptr, "GetNxControllerSettings"},
        {98, nullptr, "SetNxControllerSettings"},
        {99, &SET_SYS::GetBatteryPercentageFlag, "GetBatteryPercentageFlag"},
        {100, nullptr, "SetBatteryPercentageFlag"},
        {101, nullptr, "GetExternalRtcResetFlag"},
        {102, nullptr, "SetExternalRtcResetFlag"},
        {103, nullptr, "GetUsbFullKeyEnableFlag"},
        {104, nullptr, "SetUsbFullKeyEnableFlag"},
        {105, &SET_SYS::SetExternalSteadyClockInternalOffset, "SetExternalSteadyClockInternalOffset"},
        {106, &SET_SYS::GetExternalSteadyClockInternalOffset, "GetExternalSteadyClockInternalOffset"},
        {107, nullptr, "GetBacklightSettingsEx"},
        {108, nullptr, "SetBacklightSettingsEx"},
        {109, nullptr, "GetHeadphoneVolumeWarningCount"},
        {110, nullptr, "SetHeadphoneVolumeWarningCount"},
        {111, nullptr, "GetBluetoothAfhEnableFlag"},
        {112, nullptr, "SetBluetoothAfhEnableFlag"},
        {113, nullptr, "GetBluetoothBoostEnableFlag"},
        {114, nullptr, "SetBluetoothBoostEnableFlag"},
        {115, nullptr, "GetInRepairProcessEnableFlag"},
        {116, nullptr, "SetInRepairProcessEnableFlag"},
        {117, nullptr, "GetHeadphoneVolumeUpdateFlag"},
        {118, nullptr, "SetHeadphoneVolumeUpdateFlag"},
        {119, nullptr, "NeedsToUpdateHeadphoneVolume"},
        {120, nullptr, "GetPushNotificationActivityModeOnSleep"},
        {121, nullptr, "SetPushNotificationActivityModeOnSleep"},
        {122, nullptr, "GetServiceDiscoveryControlSettings"},
        {123, nullptr, "SetServiceDiscoveryControlSettings"},
        {124, &SET_SYS::GetErrorReportSharePermission, "GetErrorReportSharePermission"},
        {125, nullptr, "SetErrorReportSharePermission"},
        {126, &SET_SYS::GetAppletLaunchFlags, "GetAppletLaunchFlags"},
        {127, &SET_SYS::SetAppletLaunchFlags, "SetAppletLaunchFlags"},
        {128, nullptr, "GetConsoleSixAxisSensorAccelerationBias"},
        {129, nullptr, "SetConsoleSixAxisSensorAccelerationBias"},
        {130, nullptr, "GetConsoleSixAxisSensorAngularVelocityBias"},
        {131, nullptr, "SetConsoleSixAxisSensorAngularVelocityBias"},
        {132, nullptr, "GetConsoleSixAxisSensorAccelerationGain"},
        {133, nullptr, "SetConsoleSixAxisSensorAccelerationGain"},
        {134, nullptr, "GetConsoleSixAxisSensorAngularVelocityGain"},
        {135, nullptr, "SetConsoleSixAxisSensorAngularVelocityGain"},
        {136, &SET_SYS::GetKeyboardLayout, "GetKeyboardLayout"},
        {137, nullptr, "SetKeyboardLayout"},
        {138, nullptr, "GetWebInspectorFlag"},
        {139, nullptr, "GetAllowedSslHosts"},
        {140, nullptr, "GetHostFsMountPoint"},
        {141, nullptr, "GetRequiresRunRepairTimeReviser"},
        {142, nullptr, "SetRequiresRunRepairTimeReviser"},
        {143, nullptr, "SetBlePairingSettings"},
        {144, nullptr, "GetBlePairingSettings"},
        {145, nullptr, "GetConsoleSixAxisSensorAngularVelocityTimeBias"},
        {146, nullptr, "SetConsoleSixAxisSensorAngularVelocityTimeBias"},
        {147, nullptr, "GetConsoleSixAxisSensorAngularAcceleration"},
        {148, nullptr, "SetConsoleSixAxisSensorAngularAcceleration"},
        {149, &SET_SYS::GetRebootlessSystemUpdateVersion, "GetRebootlessSystemUpdateVersion"},
        {150, &SET_SYS::GetDeviceTimeZoneLocationUpdatedTime, "GetDeviceTimeZoneLocationUpdatedTime"},
        {151, &SET_SYS::SetDeviceTimeZoneLocationUpdatedTime, "SetDeviceTimeZoneLocationUpdatedTime"},
        {152, &SET_SYS::GetUserSystemClockAutomaticCorrectionUpdatedTime, "GetUserSystemClockAutomaticCorrectionUpdatedTime"},
        {153, &SET_SYS::SetUserSystemClockAutomaticCorrectionUpdatedTime, "SetUserSystemClockAutomaticCorrectionUpdatedTime"},
        {154, nullptr, "GetAccountOnlineStorageSettings"},
        {155, nullptr, "SetAccountOnlineStorageSettings"},
        {156, nullptr, "GetPctlReadyFlag"},
        {157, nullptr, "SetPctlReadyFlag"},
        {158, nullptr, "GetAnalogStickUserCalibrationL"},
        {159, nullptr, "SetAnalogStickUserCalibrationL"},
        {160, nullptr, "GetAnalogStickUserCalibrationR"},
        {161, nullptr, "SetAnalogStickUserCalibrationR"},
        {162, nullptr, "GetPtmBatteryVersion"},
        {163, nullptr, "SetPtmBatteryVersion"},
        {164, nullptr, "GetUsb30HostEnableFlag"},
        {165, nullptr, "SetUsb30HostEnableFlag"},
        {166, nullptr, "GetUsb30DeviceEnableFlag"},
        {167, nullptr, "SetUsb30DeviceEnableFlag"},
        {168, nullptr, "GetThemeId"},
        {169, nullptr, "SetThemeId"},
        {170, &SET_SYS::GetChineseTraditionalInputMethod, "GetChineseTraditionalInputMethod"},
        {171, nullptr, "SetChineseTraditionalInputMethod"},
        {172, nullptr, "GetPtmCycleCountReliability"},
        {173, nullptr, "SetPtmCycleCountReliability"},
        {174, &SET_SYS::GetHomeMenuScheme, "GetHomeMenuScheme"},
        {175, nullptr, "GetThemeSettings"},
        {176, nullptr, "SetThemeSettings"},
        {177, nullptr, "GetThemeKey"},
        {178, nullptr, "SetThemeKey"},
        {179, nullptr, "GetZoomFlag"},
        {180, nullptr, "SetZoomFlag"},
        {181, nullptr, "GetT"},
        {182, nullptr, "SetT"},
        {183, nullptr, "GetPlatformRegion"},
        {184, nullptr, "SetPlatformRegion"},
        {185, &SET_SYS::GetHomeMenuSchemeModel, "GetHomeMenuSchemeModel"},
        {186, nullptr, "GetMemoryUsageRateFlag"},
        {187, nullptr, "GetTouchScreenMode"},
        {188, nullptr, "SetTouchScreenMode"},
        {189, nullptr, "GetButtonConfigSettingsFull"},
        {190, nullptr, "SetButtonConfigSettingsFull"},
        {191, nullptr, "GetButtonConfigSettingsEmbedded"},
        {192, nullptr, "SetButtonConfigSettingsEmbedded"},
        {193, nullptr, "GetButtonConfigSettingsLeft"},
        {194, nullptr, "SetButtonConfigSettingsLeft"},
        {195, nullptr, "GetButtonConfigSettingsRight"},
        {196, nullptr, "SetButtonConfigSettingsRight"},
        {197, nullptr, "GetButtonConfigRegisteredSettingsEmbedded"},
        {198, nullptr, "SetButtonConfigRegisteredSettingsEmbedded"},
        {199, nullptr, "GetButtonConfigRegisteredSettings"},
        {200, nullptr, "SetButtonConfigRegisteredSettings"},
        {201, &SET_SYS::GetFieldTestingFlag, "GetFieldTestingFlag"},
        {202, nullptr, "SetFieldTestingFlag"},
        {203, nullptr, "GetPanelCrcMode"},
        {204, nullptr, "SetPanelCrcMode"},
        {205, nullptr, "GetNxControllerSettingsEx"},
        {206, nullptr, "SetNxControllerSettingsEx"},
        {207, nullptr, "GetHearingProtectionSafeguardFlag"},
        {208, nullptr, "SetHearingProtectionSafeguardFlag"},
        {209, nullptr, "GetHearingProtectionSafeguardRemainingTime"},
        {210, nullptr, "SetHearingProtectionSafeguardRemainingTime"},
    };
    // clang-format on

    RegisterHandlers(functions);

    SetupSettings();
    m_save_thread =
        std::jthread([this](std::stop_token stop_token) { StoreSettingsThreadFunc(stop_token); });
}

SET_SYS::~SET_SYS() {
    SetSaveNeeded();
    m_save_thread.request_stop();
}

void SET_SYS::SetupSettings() {
    auto system_dir = Common::FS::GetSudachiPath(Common::FS::SudachiPath::NANDDir) /
                      "system/save/8000000000000050";
    if (!LoadSettingsFile(system_dir, []() { return DefaultSystemSettings(); })) {
        ASSERT(false);
    }

    auto private_dir = Common::FS::GetSudachiPath(Common::FS::SudachiPath::NANDDir) /
                       "system/save/8000000000000052";
    if (!LoadSettingsFile(private_dir, []() { return DefaultPrivateSettings(); })) {
        ASSERT(false);
    }

    auto device_dir = Common::FS::GetSudachiPath(Common::FS::SudachiPath::NANDDir) /
                      "system/save/8000000000000053";
    if (!LoadSettingsFile(device_dir, []() { return DefaultDeviceSettings(); })) {
        ASSERT(false);
    }

    auto appln_dir = Common::FS::GetSudachiPath(Common::FS::SudachiPath::NANDDir) /
                     "system/save/8000000000000054";
    if (!LoadSettingsFile(appln_dir, []() { return DefaultApplnSettings(); })) {
        ASSERT(false);
    }
}

void SET_SYS::StoreSettings() {
    auto system_dir = Common::FS::GetSudachiPath(Common::FS::SudachiPath::NANDDir) /
                      "system/save/8000000000000050";
    if (!StoreSettingsFile(system_dir, m_system_settings)) {
        LOG_ERROR(HW_GPU, "Failed to store System settings");
    }

    auto private_dir = Common::FS::GetSudachiPath(Common::FS::SudachiPath::NANDDir) /
                       "system/save/8000000000000052";
    if (!StoreSettingsFile(private_dir, m_private_settings)) {
        LOG_ERROR(HW_GPU, "Failed to store Private settings");
    }

    auto device_dir = Common::FS::GetSudachiPath(Common::FS::SudachiPath::NANDDir) /
                      "system/save/8000000000000053";
    if (!StoreSettingsFile(device_dir, m_device_settings)) {
        LOG_ERROR(HW_GPU, "Failed to store Device settings");
    }

    auto appln_dir = Common::FS::GetSudachiPath(Common::FS::SudachiPath::NANDDir) /
                     "system/save/8000000000000054";
    if (!StoreSettingsFile(appln_dir, m_appln_settings)) {
        LOG_ERROR(HW_GPU, "Failed to store ApplLn settings");
    }
}

void SET_SYS::StoreSettingsThreadFunc(std::stop_token stop_token) {
    Common::SetCurrentThreadName("SettingsStore");

    while (Common::StoppableTimedWait(stop_token, std::chrono::minutes(1))) {
        std::scoped_lock l{m_save_needed_mutex};
        if (!std::exchange(m_save_needed, false)) {
            continue;
        }
        StoreSettings();
    }
}

void SET_SYS::SetSaveNeeded() {
    std::scoped_lock l{m_save_needed_mutex};
    m_save_needed = true;
}

Result SET_SYS::GetSettingsItemValue(std::vector<u8>& out_value, const std::string& category,
                                     const std::string& name) {
    auto settings{GetSettings()};
    R_UNLESS(settings.contains(category) && settings[category].contains(name), ResultUnknown);

    out_value = settings[category][name];
    R_SUCCEED();
}

Result SET_SYS::GetExternalSteadyClockSourceId(Common::UUID& out_id) {
    out_id = m_private_settings.external_clock_source_id;
    R_SUCCEED();
}

Result SET_SYS::SetExternalSteadyClockSourceId(Common::UUID id) {
    m_private_settings.external_clock_source_id = id;
    SetSaveNeeded();
    R_SUCCEED();
}

Result SET_SYS::GetUserSystemClockContext(Service::Time::Clock::SystemClockContext& out_context) {
    out_context = m_system_settings.user_system_clock_context;
    R_SUCCEED();
}

Result SET_SYS::SetUserSystemClockContext(Service::Time::Clock::SystemClockContext& context) {
    m_system_settings.user_system_clock_context = context;
    SetSaveNeeded();
    R_SUCCEED();
}

Result SET_SYS::GetDeviceTimeZoneLocationName(Service::Time::TimeZone::LocationName& out_name) {
    out_name = m_system_settings.device_time_zone_location_name;
    R_SUCCEED();
}

Result SET_SYS::SetDeviceTimeZoneLocationName(Service::Time::TimeZone::LocationName& name) {
    m_system_settings.device_time_zone_location_name = name;
    SetSaveNeeded();
    R_SUCCEED();
}

Result SET_SYS::GetNetworkSystemClockContext(
    Service::Time::Clock::SystemClockContext& out_context) {
    out_context = m_system_settings.network_system_clock_context;
    R_SUCCEED();
}

Result SET_SYS::SetNetworkSystemClockContext(Service::Time::Clock::SystemClockContext& context) {
    m_system_settings.network_system_clock_context = context;
    SetSaveNeeded();
    R_SUCCEED();
}

Result SET_SYS::IsUserSystemClockAutomaticCorrectionEnabled(bool& out_enabled) {
    out_enabled = m_system_settings.user_system_clock_automatic_correction_enabled;
    R_SUCCEED();
}

Result SET_SYS::SetUserSystemClockAutomaticCorrectionEnabled(bool enabled) {
    m_system_settings.user_system_clock_automatic_correction_enabled = enabled;
    SetSaveNeeded();
    R_SUCCEED();
}

Result SET_SYS::SetExternalSteadyClockInternalOffset(s64 offset) {
    m_private_settings.external_steady_clock_internal_offset = offset;
    SetSaveNeeded();
    R_SUCCEED();
}

Result SET_SYS::GetExternalSteadyClockInternalOffset(s64& out_offset) {
    out_offset = m_private_settings.external_steady_clock_internal_offset;
    R_SUCCEED();
}

Result SET_SYS::GetDeviceTimeZoneLocationUpdatedTime(
    Service::Time::Clock::SteadyClockTimePoint& out_time_point) {
    out_time_point = m_system_settings.device_time_zone_location_updated_time;
    R_SUCCEED();
}

Result SET_SYS::SetDeviceTimeZoneLocationUpdatedTime(
    Service::Time::Clock::SteadyClockTimePoint& time_point) {
    m_system_settings.device_time_zone_location_updated_time = time_point;
    SetSaveNeeded();
    R_SUCCEED();
}

Result SET_SYS::GetUserSystemClockAutomaticCorrectionUpdatedTime(
    Service::Time::Clock::SteadyClockTimePoint& out_time_point) {
    out_time_point = m_system_settings.user_system_clock_automatic_correction_updated_time_point;
    R_SUCCEED();
}

Result SET_SYS::SetUserSystemClockAutomaticCorrectionUpdatedTime(
    Service::Time::Clock::SteadyClockTimePoint out_time_point) {
    m_system_settings.user_system_clock_automatic_correction_updated_time_point = out_time_point;
    SetSaveNeeded();
    R_SUCCEED();
}

} // namespace Service::Set
