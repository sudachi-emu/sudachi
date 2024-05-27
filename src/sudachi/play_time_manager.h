// SPDX-FileCopyrightText: 2023 sudachi Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QString>

#include <map>

#include "common/common_funcs.h"
#include "common/common_types.h"
#include "common/polyfill_thread.h"

namespace Service::Account {
class ProfileManager;
}

namespace PlayTime {

using ProgramId = u64;
using PlayTime = u64;
using TotalTimes = u64;
using PlayTimeDatabase = std::map<ProgramId, std::pair<PlayTime, TotalTimes>>;

class PlayTimeManager {
public:
    explicit PlayTimeManager(Service::Account::ProfileManager& profile_manager);
    ~PlayTimeManager();

    SUDACHI_NON_COPYABLE(PlayTimeManager);
    SUDACHI_NON_MOVEABLE(PlayTimeManager);

    u64 GetPlayTime(u64 program_id) const;
    u64 GetTotalTimes(u64 program_id) const;
    void ResetProgramPlayTime(u64 program_id);
    void SetProgramId(u64 program_id);
    void Start();
    void Stop();

private:
    void AutoTimestamp(std::stop_token stop_token);
    void Save();

    PlayTimeDatabase database;
    u64 running_program_id;
    std::jthread play_time_thread;
    Service::Account::ProfileManager& manager;
};

QString ReadablePlayTime(qulonglong time_seconds);
QString ReadableTotalTimes(qulonglong times_count);

} // namespace PlayTime
