// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <functional>
#include <string>

#include "common/common_types.h"
#include "core/file_sys/vfs/vfs_types.h"
#include "core/hle/service/glue/time/file_timestamp_worker.h"
#include "core/hle/service/glue/time/standard_steady_clock_resource.h"
#include "core/hle/service/glue/time/time_zone_binary.h"
#include "core/hle/service/glue/time/worker.h"
#include "core/hle/service/service.h"

namespace Core {
class System;
}

namespace Service::PSC::Time {
class ServiceManager;
class StaticService;
} // namespace Service::PSC::Time

namespace Service::Glue::Time {
class TimeManager {
public:
    explicit TimeManager(Core::System& system);
    ~TimeManager() = default;

    std::shared_ptr<Service::Set::ISystemSettingsServer> m_set_sys;

    std::shared_ptr<Service::PSC::Time::ServiceManager> m_time_m{};
    std::shared_ptr<Service::PSC::Time::StaticService> m_time_sm{};
    StandardSteadyClockResource m_steady_clock_resource;
    FileTimestampWorker m_file_timestamp_worker;
    TimeZoneBinary m_time_zone_binary;
    TimeWorker m_worker;

private:
    Result SetupStandardSteadyClockCore();
    Result SetupTimeZoneServiceCore();
};
} // namespace Service::Glue::Time
