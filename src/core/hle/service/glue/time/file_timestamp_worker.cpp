// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/hle/service/glue/time/file_timestamp_worker.h"
#include "core/hle/service/psc/time/common.h"
#include "core/hle/service/psc/time/system_clock.h"
#include "core/hle/service/psc/time/time_zone_service.h"

namespace Service::Glue::Time {

void FileTimestampWorker::SetFilesystemPosixTime() {
    s64 time{};
    Service::PSC::Time::CalendarTime calendar_time{};
    Service::PSC::Time::CalendarAdditionalInfo additional_info{};

    if (m_initialized && m_system_clock->GetCurrentTime(&time) == ResultSuccess &&
        m_time_zone->ToCalendarTimeWithMyRule(&calendar_time, &additional_info, time) ==
            ResultSuccess) {
        // TODO IFileSystemProxy::SetCurrentPosixTime
    }
}

} // namespace Service::Glue::Time
