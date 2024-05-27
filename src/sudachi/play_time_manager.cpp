// SPDX-FileCopyrightText: 2023 sudachi Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/alignment.h"
#include "common/fs/file.h"
#include "common/fs/fs.h"
#include "common/fs/path_util.h"
#include "common/logging/log.h"
#include "common/settings.h"
#include "common/thread.h"
#include "core/hle/service/acc/profile_manager.h"
#include "sudachi/play_time_manager.h"

namespace PlayTime {

namespace {

struct PlayTimeElement {
    ProgramId program_id;
    PlayTime play_time;
    TotalTimes total_times;
};

std::optional<std::filesystem::path> GetCurrentUserPlayTimePath(
    const Service::Account::ProfileManager& manager) {
    const auto uuid = manager.GetUser(static_cast<s32>(Settings::values.current_user));
    if (!uuid.has_value()) {
        return std::nullopt;
    }
    return Common::FS::GetSudachiPath(Common::FS::SudachiPath::PlayTimeDir) /
           uuid->RawString().append(".bin");
}

[[nodiscard]] bool ReadPlayTimeFile(PlayTimeDatabase& out_play_time_db,
                                    const Service::Account::ProfileManager& manager) {
    const auto filename = GetCurrentUserPlayTimePath(manager);

    if (!filename.has_value()) {
        LOG_ERROR(Frontend, "Failed to get current user path");
        return false;
    }

    out_play_time_db.clear();

    if (Common::FS::Exists(filename.value())) {
        Common::FS::IOFile file{filename.value(), Common::FS::FileAccessMode::Read,
                                Common::FS::FileType::BinaryFile};
        if (!file.IsOpen()) {
            LOG_ERROR(Frontend, "Failed to open play time file: {}",
                      Common::FS::PathToUTF8String(filename.value()));
            return false;
        }

        const size_t num_elements = file.GetSize() / sizeof(PlayTimeElement);
        std::vector<PlayTimeElement> elements(num_elements);

        if (file.ReadSpan<PlayTimeElement>(elements) != num_elements) {
            return false;
        }

        for (const auto& [program_id, play_time, total_times] : elements) {
            if (program_id != 0) {
                out_play_time_db[program_id] = {play_time, total_times};
            }
        }
    }

    return true;
}

[[nodiscard]] bool WritePlayTimeFile(const PlayTimeDatabase& play_time_db,
                                     const Service::Account::ProfileManager& manager) {
    const auto filename = GetCurrentUserPlayTimePath(manager);

    if (!filename.has_value()) {
        LOG_ERROR(Frontend, "Failed to get current user path");
        return false;
    }

    Common::FS::IOFile file{filename.value(), Common::FS::FileAccessMode::Write,
                            Common::FS::FileType::BinaryFile};
    if (!file.IsOpen()) {
        LOG_ERROR(Frontend, "Failed to open play time file: {}",
                  Common::FS::PathToUTF8String(filename.value()));
        return false;
    }

    std::vector<PlayTimeElement> elements;
    elements.reserve(play_time_db.size());

    for (auto& [program_id, play_time] : play_time_db) {
        if (program_id != 0) {
            elements.push_back(PlayTimeElement{program_id, play_time.first, play_time.second});
        }
    }

    return file.WriteSpan<PlayTimeElement>(elements) == elements.size();
}

} // namespace

PlayTimeManager::PlayTimeManager(Service::Account::ProfileManager& profile_manager)
    : manager{profile_manager} {
    if (!ReadPlayTimeFile(database, manager)) {
        LOG_ERROR(Frontend, "Failed to read play time database! Resetting to default.");
    }
}

PlayTimeManager::~PlayTimeManager() {
    Save();
}

void PlayTimeManager::SetProgramId(u64 program_id) {
    running_program_id = program_id;
}

void PlayTimeManager::Start() {
    play_time_thread = std::jthread([&](std::stop_token stop_token) { AutoTimestamp(stop_token); });
}

void PlayTimeManager::Stop() {
    play_time_thread = {};
}

void PlayTimeManager::AutoTimestamp(std::stop_token stop_token) {
    Common::SetCurrentThreadName("PlayTimeReport");

    using namespace std::literals::chrono_literals;
    using std::chrono::seconds;
    using std::chrono::steady_clock;

    auto timestamp = steady_clock::now();

    const auto GetDuration = [&]() -> u64 {
        const auto last_timestamp = std::exchange(timestamp, steady_clock::now());
        const auto duration = std::chrono::duration_cast<seconds>(timestamp - last_timestamp);
        return static_cast<u64>(duration.count());
    };

    while (!stop_token.stop_requested()) {
        Common::StoppableTimedWait(stop_token, 30s);

        database[running_program_id].first += GetDuration();
        database[running_program_id].second += 1;
        Save();
    }
}

void PlayTimeManager::Save() {
    if (!WritePlayTimeFile(database, manager)) {
        LOG_ERROR(Frontend, "Failed to update play time database!");
    }
}

u64 PlayTimeManager::GetPlayTime(u64 program_id) const {
    auto it = database.find(program_id);
    if (it != database.end()) {
        return it->second.first;
    } else {
        return 0;
    }
}

u64 PlayTimeManager::GetTotalTimes(u64 program_id) const {
    auto it = database.find(program_id);
    if (it != database.end()) {
        return it->second.second;
    } else {
        return 0;
    }
}

void PlayTimeManager::ResetProgramPlayTime(u64 program_id) {
    database.erase(program_id);
    Save();
}

QString ReadablePlayTime(qulonglong time_seconds) {
    if (time_seconds == 0) {
        return {};
    }
    const auto time_minutes = std::max(static_cast<double>(time_seconds) / 60, 1.0);
    const auto time_hours = static_cast<double>(time_seconds) / 3600;
    const bool is_minutes = time_minutes < 60;
    const char* unit = is_minutes ? "m" : "h";
    const auto value = is_minutes ? time_minutes : time_hours;

    return QStringLiteral("%L1 %2")
        .arg(value, 0, 'f', !is_minutes && time_seconds % 60 != 0)
        .arg(QString::fromUtf8(unit));
}

QString ReadableTotalTimes(qulonglong times_count) {
    if (times_count == 0) {
        return {};
    }

    return QStringLiteral("%L1").arg((double)times_count, 0, 'f', 0);
}

} // namespace PlayTime
