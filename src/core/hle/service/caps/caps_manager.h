// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <unordered_map>

#include "common/fs/fs.h"
#include "core/hle/result.h"
#include "core/hle/service/caps/caps_types.h"

namespace Core {
class System;
}

namespace std {
// Hash used to create lists from AlbumFileId data
template <>
struct hash<Service::Capture::AlbumFileId> {
    size_t operator()(const Service::Capture::AlbumFileId& pad_id) const noexcept {
        u64 hash_value = (static_cast<u64>(pad_id.date.year) << 8);
        hash_value ^= (static_cast<u64>(pad_id.date.month) << 7);
        hash_value ^= (static_cast<u64>(pad_id.date.day) << 6);
        hash_value ^= (static_cast<u64>(pad_id.date.hour) << 5);
        hash_value ^= (static_cast<u64>(pad_id.date.minute) << 4);
        hash_value ^= (static_cast<u64>(pad_id.date.second) << 3);
        hash_value ^= (static_cast<u64>(pad_id.date.unique_id) << 2);
        hash_value ^= (static_cast<u64>(pad_id.storage) << 1);
        hash_value ^= static_cast<u64>(pad_id.type);
        return static_cast<size_t>(hash_value);
    }
};

} // namespace std

namespace Service::Capture {

class AlbumManager {
public:
    explicit AlbumManager(Core::System& system_);
    ~AlbumManager();

    Result DeleteAlbumFile(const AlbumFileId& file_id);
    Result IsAlbumMounted(AlbumStorage storage);
    Result GetAlbumFileList(std::span<AlbumEntry> out_entries, u64& out_entries_count,
                            AlbumStorage storage, u8 flags) const;
    Result GetAlbumFileList(std::span<ApplicationAlbumFileEntry> out_entries,
                            u64& out_entries_count, ContentType content_type, s64 start_posix_time,
                            s64 end_posix_time, u64 aruid) const;
    Result GetAlbumFileList(std::span<ApplicationAlbumEntry> out_entries, u64& out_entries_count,
                            ContentType content_type, AlbumFileDateTime start_date,
                            AlbumFileDateTime end_date, u64 aruid) const;
    Result GetAutoSavingStorage(bool& out_is_autosaving) const;
    Result LoadAlbumScreenShotImage(LoadAlbumScreenShotImageOutput& out_image_output,
                                    std::span<u8> out_image, const AlbumFileId& file_id,
                                    const ScreenShotDecodeOption& decoder_options) const;
    Result LoadAlbumScreenShotThumbnail(LoadAlbumScreenShotImageOutput& out_image_output,
                                        std::span<u8> out_image, const AlbumFileId& file_id,
                                        const ScreenShotDecodeOption& decoder_options) const;

    Result SaveScreenShot(ApplicationAlbumEntry& out_entry, const ScreenShotAttribute& attribute,
                          AlbumReportOption report_option, std::span<const u8> image_data,
                          u64 aruid);
    Result SaveScreenShot(ApplicationAlbumEntry& out_entry, const ScreenShotAttribute& attribute,
                          AlbumReportOption report_option, const ApplicationData& app_data,
                          std::span<const u8> image_data, u64 aruid);
    Result SaveEditedScreenShot(ApplicationAlbumEntry& out_entry,
                                const ScreenShotAttribute& attribute, const AlbumFileId& file_id,
                                std::span<const u8> image_data);

    void FlipVerticallyOnWrite(bool flip);

private:
    static constexpr std::size_t NandAlbumFileLimit = 1000;
    static constexpr std::size_t SdAlbumFileLimit = 10000;

    void FindScreenshots();
    Result GetFile(std::filesystem::path& out_path, const AlbumFileId& file_id) const;
    Result GetAlbumEntry(AlbumEntry& out_entry, const std::filesystem::path& path) const;
    Result LoadImage(std::span<u8> out_image, const std::filesystem::path& path, int width,
                     int height, ScreenShotDecoderFlag flag) const;
    Result SaveImage(ApplicationAlbumEntry& out_entry, std::span<const u8> image, u64 title_id,
                     const AlbumFileDateTime& date) const;

    AlbumFileDateTime ConvertToAlbumDateTime(u64 posix_time) const;

    bool is_mounted{};
    std::unordered_map<AlbumFileId, std::filesystem::path> album_files;

    Core::System& system;
};

} // namespace Service::Capture
