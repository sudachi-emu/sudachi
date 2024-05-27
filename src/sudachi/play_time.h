// SPDX-FileCopyrightText: 2023 sudachi Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QString>

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <thread>

#include "common/common_types.h"
#include "common/fs/fs.h"
#include "common/polyfill_thread.h"
#include "core/core.h"

namespace PlayTime {
struct PlayTimeElement {
    u64 program_id;
    u64 play_time;
    u64 total_times;

    inline bool operator==(const PlayTimeElement& other) const {
        return program_id == other.program_id;
    }

    inline bool operator==(const u64 _program_id) const {
        return program_id == _program_id;
    }
};

class PlayTimeManager {
public:
    explicit PlayTimeManager() = default;
    ~PlayTimeManager() = default;

public:
    SUDACHI_NON_COPYABLE(PlayTimeManager);
    SUDACHI_NON_MOVEABLE(PlayTimeManager);

public:
    bool ResetProgramPlayTime(u64 program_id);
    void SetProgramId(u64 program_id);
    inline void UpdateTimestamp();
    void Start();
    void Stop();

private:
    u64 running_program_id;
    std::chrono::steady_clock::time_point last_timestamp;
    u64 last_total_times;
    std::jthread play_time_thread;
    void AutoTimestamp(std::stop_token stop_token);
    void Save();
};

std::optional<std::filesystem::path> GetCurrentUserPlayTimePath();

bool UpdatePlayTime(u64 program_id, u64 add_play_time, u64 add_total_times);

[[nodiscard]] bool ReadPlayTimeFile(std::vector<PlayTimeElement>& out_play_time_elements);
[[nodiscard]] bool WritePlayTimeFile(const std::vector<PlayTimeElement>& play_time_elements);

u64 GetPlayTime(u64 program_id);
u64 GetTotalTimes(u64 program_id);

QString ReadablePlayTime(qulonglong time_seconds);
QString ReadableTotalTimes(qulonglong times_count);

} // namespace PlayTime
