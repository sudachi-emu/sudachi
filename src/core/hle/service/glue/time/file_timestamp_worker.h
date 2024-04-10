// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>

#include "common/common_types.h"

namespace Service::PSC::Time {
class SystemClock;
class TimeZoneService;
} // namespace Service::PSC::Time

namespace Service::Glue::Time {

class FileTimestampWorker {
public:
    FileTimestampWorker() = default;

    void SetFilesystemPosixTime();

    std::shared_ptr<Service::PSC::Time::SystemClock> m_system_clock{};
    std::shared_ptr<Service::PSC::Time::TimeZoneService> m_time_zone{};
    bool m_initialized{};
};

} // namespace Service::Glue::Time
