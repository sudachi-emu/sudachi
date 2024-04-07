// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/core.h"
#include "core/file_sys/content_archive.h"
#include "core/file_sys/nca_metadata.h"
#include "core/file_sys/registered_cache.h"
#include "core/file_sys/romfs.h"
#include "core/file_sys/system_archive/system_archive.h"
#include "core/file_sys/vfs/vfs.h"
#include "core/hle/service/filesystem/filesystem.h"
#include "core/hle/service/glue/time/time_zone_binary.h"

namespace Service::Glue::Time {
constexpr u64 TimeZoneBinaryId = 0x10000000000080E;

void TimeZoneBinary::Reset() {
    time_zone_binary_romfs = {};
    time_zone_binary_mount_result = ResultUnknown;
    time_zone_scratch_space.clear();
    time_zone_scratch_space.resize(0x2800, 0);
}

Result TimeZoneBinary::Mount() {
    Reset();

    auto& fsc{system.GetFileSystemController()};
    std::unique_ptr<FileSys::NCA> nca{};

    auto* bis_system = fsc.GetSystemNANDContents();

    R_UNLESS(bis_system, ResultUnknown);

    nca = bis_system->GetEntry(TimeZoneBinaryId, FileSys::ContentRecordType::Data);

    if (nca) {
        time_zone_binary_romfs = FileSys::ExtractRomFS(nca->GetRomFS());
    }

    if (time_zone_binary_romfs) {
        // Validate that the romfs is readable, using invalid firmware keys can cause this to get
        // set but the files to be garbage. In that case, we want to hit the next path and
        // synthesise them instead.
        time_zone_binary_mount_result = ResultSuccess;
        Service::PSC::Time::LocationName name{"Etc/GMT"};
        if (!IsValid(name)) {
            Reset();
        }
    }

    if (!time_zone_binary_romfs) {
        time_zone_binary_romfs = FileSys::ExtractRomFS(
            FileSys::SystemArchive::SynthesizeSystemArchive(TimeZoneBinaryId));
    }

    R_UNLESS(time_zone_binary_romfs, ResultUnknown);

    time_zone_binary_mount_result = ResultSuccess;
    R_SUCCEED();
}

Result TimeZoneBinary::Read(size_t& out_read_size, std::span<u8> out_buffer, size_t out_buffer_size,
                            std::string_view path) {
    R_UNLESS(time_zone_binary_mount_result == ResultSuccess, time_zone_binary_mount_result);

    auto vfs_file{time_zone_binary_romfs->GetFileRelative(path)};
    R_UNLESS(vfs_file, ResultUnknown);

    auto file_size{vfs_file->GetSize()};
    R_UNLESS(file_size > 0, ResultUnknown);

    R_UNLESS(file_size <= out_buffer_size, Service::PSC::Time::ResultFailed);

    out_read_size = vfs_file->Read(out_buffer.data(), file_size);
    R_UNLESS(out_read_size > 0, ResultUnknown);

    R_SUCCEED();
}

void TimeZoneBinary::GetListPath(std::string& out_path) {
    if (time_zone_binary_mount_result != ResultSuccess) {
        return;
    }
    // out_path = fmt::format("{}:/binaryList.txt", "TimeZoneBinary");
    out_path = "/binaryList.txt";
}

void TimeZoneBinary::GetVersionPath(std::string& out_path) {
    if (time_zone_binary_mount_result != ResultSuccess) {
        return;
    }
    // out_path = fmt::format("{}:/version.txt", "TimeZoneBinary");
    out_path = "/version.txt";
}

void TimeZoneBinary::GetTimeZonePath(std::string& out_path,
                                     const Service::PSC::Time::LocationName& name) {
    if (time_zone_binary_mount_result != ResultSuccess) {
        return;
    }
    // out_path = fmt::format("{}:/zoneinfo/{}", "TimeZoneBinary", name);
    out_path = fmt::format("/zoneinfo/{}", name.data());
}

bool TimeZoneBinary::IsValid(const Service::PSC::Time::LocationName& name) {
    std::string path{};
    GetTimeZonePath(path, name);

    auto vfs_file{time_zone_binary_romfs->GetFileRelative(path)};
    if (!vfs_file) {
        LOG_INFO(Service_Time, "Could not find timezone file {}", path);
        return false;
    }
    return vfs_file->GetSize() != 0;
}

u32 TimeZoneBinary::GetTimeZoneCount() {
    std::string path{};
    GetListPath(path);

    size_t bytes_read{};
    if (Read(bytes_read, time_zone_scratch_space, 0x2800, path) != ResultSuccess) {
        return 0;
    }
    if (bytes_read == 0) {
        return 0;
    }

    auto chars = std::span(reinterpret_cast<char*>(time_zone_scratch_space.data()), bytes_read);
    u32 count{};
    for (auto chr : chars) {
        if (chr == '\n') {
            count++;
        }
    }
    return count;
}

Result TimeZoneBinary::GetTimeZoneVersion(Service::PSC::Time::RuleVersion& out_rule_version) {
    std::string path{};
    GetVersionPath(path);

    auto rule_version_buffer{std::span(reinterpret_cast<u8*>(&out_rule_version),
                                       sizeof(Service::PSC::Time::RuleVersion))};
    size_t bytes_read{};
    R_TRY(Read(bytes_read, rule_version_buffer, rule_version_buffer.size_bytes(), path));

    rule_version_buffer[bytes_read] = 0;
    R_SUCCEED();
}

Result TimeZoneBinary::GetTimeZoneRule(std::span<const u8>& out_rule, size_t& out_rule_size,
                                       const Service::PSC::Time::LocationName& name) {
    std::string path{};
    GetTimeZonePath(path, name);

    size_t bytes_read{};
    R_TRY(Read(bytes_read, time_zone_scratch_space, time_zone_scratch_space.size(), path));

    out_rule = std::span(time_zone_scratch_space.data(), bytes_read);
    out_rule_size = bytes_read;
    R_SUCCEED();
}

Result TimeZoneBinary::GetTimeZoneLocationList(
    u32& out_count, std::span<Service::PSC::Time::LocationName> out_names, size_t max_names,
    u32 index) {
    std::string path{};
    GetListPath(path);

    size_t bytes_read{};
    R_TRY(Read(bytes_read, time_zone_scratch_space, time_zone_scratch_space.size(), path));

    out_count = 0;
    R_SUCCEED_IF(bytes_read == 0);

    Service::PSC::Time::LocationName current_name{};
    size_t current_name_len{};
    std::span<const u8> chars{time_zone_scratch_space};
    u32 name_count{};

    for (auto chr : chars) {
        if (chr == '\r') {
            continue;
        }

        if (chr == '\n') {
            if (name_count >= index) {
                out_names[out_count] = current_name;
                out_count++;
                if (out_count >= max_names) {
                    break;
                }
            }
            name_count++;
            current_name_len = 0;
            current_name = {};
            continue;
        }

        if (chr == '\0') {
            break;
        }

        R_UNLESS(current_name_len <= current_name.size() - 2, Service::PSC::Time::ResultFailed);

        current_name[current_name_len++] = chr;
    }

    R_SUCCEED();
}

} // namespace Service::Glue::Time
