// SPDX-FileCopyrightText: 2023 sudachi Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/fs/file.h"
#include "common/fs/path_util.h"
#include "common/logging/log.h"
#include "common/settings.h"
#include "common/thread.h"
#include "core/hle/service/acc/profile_manager.h"

#include "sudachi/play_time.h"

namespace PlayTime {

void PlayTimeManager::SetProgramId(u64 program_id) {
    this->running_program_id = program_id;
}

inline void PlayTimeManager::UpdateTimestamp() {
    this->last_timestamp = std::chrono::steady_clock::now();
    this->last_total_times = GetTotalTimes(this->running_program_id);
}

void PlayTimeManager::Start() {
    UpdateTimestamp();
    play_time_thread =
        std::jthread([&](std::stop_token stop_token) { this->AutoTimestamp(stop_token); });
}

void PlayTimeManager::Stop() {
    play_time_thread.request_stop();
}

void PlayTimeManager::AutoTimestamp(std::stop_token stop_token) {
    Common::SetCurrentThreadName("PlayTimeReport");

    using namespace std::literals::chrono_literals;

    const auto duration = 30s;
    while (Common::StoppableTimedWait(stop_token, duration)) {
        Save();
    }

    Save();
}

void PlayTimeManager::Save() {
    const auto now = std::chrono::steady_clock::now();
    const auto duration =
        static_cast<u64>(std::chrono::duration_cast<std::chrono::seconds>(
                             std::chrono::steady_clock::duration(now - this->last_timestamp))
                             .count());
    UpdateTimestamp();
    if (!UpdatePlayTime(running_program_id, duration, 1)) {
        LOG_ERROR(Common, "Failed to update play time");
    }
}

bool UpdatePlayTime(u64 program_id, u64 add_play_time, u64 add_total_times) {
    std::vector<PlayTimeElement> play_time_elements;
    if (!ReadPlayTimeFile(play_time_elements)) {
        return false;
    }
    const auto it = std::find(play_time_elements.begin(), play_time_elements.end(), program_id);

    if (it == play_time_elements.end()) {
        play_time_elements.push_back(
            {.program_id = program_id, .play_time = add_play_time, .total_times = add_total_times});
    } else {
        play_time_elements.at(it - play_time_elements.begin()).play_time += add_play_time;
        play_time_elements.at(it - play_time_elements.begin()).total_times += add_total_times;
    }
    if (!WritePlayTimeFile(play_time_elements)) {
        return false;
    }
    return true;
}

u64 GetPlayTime(u64 program_id) {
    std::vector<PlayTimeElement> play_time_elements;

    if (!ReadPlayTimeFile(play_time_elements)) {
        return 0;
    }
    const auto it = std::find(play_time_elements.begin(), play_time_elements.end(), program_id);
    if (it == play_time_elements.end()) {
        return 0;
    }
    return play_time_elements.at(it - play_time_elements.begin()).play_time;
}

u64 GetTotalTimes(u64 program_id) {
    std::vector<PlayTimeElement> play_time_elements;

    if (!ReadPlayTimeFile(play_time_elements)) {
        return 0;
    }
    const auto it = std::find(play_time_elements.begin(), play_time_elements.end(), program_id);
    if (it == play_time_elements.end()) {
        return 0;
    }
    return play_time_elements.at(it - play_time_elements.begin()).total_times;
}

bool PlayTimeManager::ResetProgramPlayTime(u64 program_id) {
    std::vector<PlayTimeElement> play_time_elements;

    if (!ReadPlayTimeFile(play_time_elements)) {
        return false;
    }
    const auto it = std::find(play_time_elements.begin(), play_time_elements.end(), program_id);
    if (it == play_time_elements.end()) {
        return false;
    }
    play_time_elements.erase(it);
    if (!WritePlayTimeFile(play_time_elements)) {
        return false;
    }
    return true;
}

std::optional<std::filesystem::path> GetCurrentUserPlayTimePath() {
    const Service::Account::ProfileManager manager;
    const auto uuid = manager.GetUser(static_cast<s32>(Settings::values.current_user));
    if (!uuid.has_value()) {
        return std::nullopt;
    }
    return Common::FS::GetSudachiPath(Common::FS::SudachiPath::PlayTimeDir) /
           uuid->RawString().append(".bin");
}

[[nodiscard]] bool ReadPlayTimeFile(std::vector<PlayTimeElement>& out_play_time_elements) {
    const auto filename = GetCurrentUserPlayTimePath();
    if (!filename.has_value()) {
        LOG_ERROR(Common, "Failed to get current user path");
        return false;
    }

    if (Common::FS::Exists(filename.value())) {
        Common::FS::IOFile file{filename.value(), Common::FS::FileAccessMode::Read,
                                Common::FS::FileType::BinaryFile};
        if (!file.IsOpen()) {
            LOG_ERROR(Common, "Failed to open play time file: {}",
                      Common::FS::PathToUTF8String(filename.value()));
            return false;
        }
        const size_t elem_num = file.GetSize() / sizeof(PlayTimeElement);
        out_play_time_elements.resize(elem_num);
        const bool success = file.ReadSpan<PlayTimeElement>(out_play_time_elements) == elem_num;
        file.Close();
        return success;
    } else {
        out_play_time_elements.clear();
        return true;
    }
}

[[nodiscard]] bool WritePlayTimeFile(const std::vector<PlayTimeElement>& play_time_elements) {
    const auto filename = GetCurrentUserPlayTimePath();
    if (!filename.has_value()) {
        LOG_ERROR(Common, "Failed to get current user path");
        return false;
    }
    Common::FS::IOFile file{filename.value(), Common::FS::FileAccessMode::Write,
                            Common::FS::FileType::BinaryFile};

    if (!file.IsOpen()) {
        LOG_ERROR(Common, "Failed to open play time file: {}",
                  Common::FS::PathToUTF8String(filename.value()));
        return false;
    }
    const bool success =
        file.WriteSpan<PlayTimeElement>(play_time_elements) == play_time_elements.size();
    file.Close();
    return success;
}

QString ReadablePlayTime(qulonglong time_seconds) {
    static constexpr std::array units{"m", "h"};
    if (time_seconds == 0) {
        return QLatin1String("");
    }
    const auto time_minutes = std::max(static_cast<double>(time_seconds) / 60, 1.0);
    const auto time_hours = static_cast<double>(time_seconds) / 3600;
    const int unit = time_minutes < 60 ? 0 : 1;
    const auto value = unit == 0 ? time_minutes : time_hours;

    return QStringLiteral("%L1 %2")
        .arg(value, 0, 'f', unit && time_seconds % 60 != 0)
        .arg(QString::fromUtf8(units[unit]));
}

QString ReadableTotalTimes(qulonglong times_count) {
    return QStringLiteral("%L1").arg((double)times_count, 0, 'f', 0);
}

} // namespace PlayTime
