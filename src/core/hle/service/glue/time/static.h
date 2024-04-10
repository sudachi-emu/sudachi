// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "common/common_types.h"
#include "core/hle/service/cmif_types.h"
#include "core/hle/service/glue/time/manager.h"
#include "core/hle/service/glue/time/time_zone.h"
#include "core/hle/service/psc/time/common.h"

namespace Core {
class System;
}

namespace Service::Set {
class ISystemSettingsServer;
}

namespace Service::PSC::Time {
class StaticService;
class SystemClock;
class SteadyClock;
class TimeZoneService;
class ServiceManager;
} // namespace Service::PSC::Time

namespace Service::Glue::Time {
class FileTimestampWorker;
class StandardSteadyClockResource;

class StaticService final : public ServiceFramework<StaticService> {
    using InClockSnapshot = InLargeData<Service::PSC::Time::ClockSnapshot, BufferAttr_HipcPointer>;
    using OutClockSnapshot =
        OutLargeData<Service::PSC::Time::ClockSnapshot, BufferAttr_HipcPointer>;

public:
    explicit StaticService(Core::System& system,
                           Service::PSC::Time::StaticServiceSetupInfo setup_info,
                           std::shared_ptr<TimeManager> time, const char* name);

    ~StaticService() override = default;

    Result GetStandardUserSystemClock(OutInterface<Service::PSC::Time::SystemClock> out_service);
    Result GetStandardNetworkSystemClock(OutInterface<Service::PSC::Time::SystemClock> out_service);
    Result GetStandardSteadyClock(OutInterface<Service::PSC::Time::SteadyClock> out_service);
    Result GetTimeZoneService(OutInterface<TimeZoneService> out_service);
    Result GetStandardLocalSystemClock(OutInterface<Service::PSC::Time::SystemClock> out_service);
    Result GetEphemeralNetworkSystemClock(
        OutInterface<Service::PSC::Time::SystemClock> out_service);
    Result GetSharedMemoryNativeHandle(OutCopyHandle<Kernel::KSharedMemory> out_shared_memory);
    Result SetStandardSteadyClockInternalOffset(s64 offset_ns);
    Result GetStandardSteadyClockRtcValue(Out<s64> out_rtc_value);
    Result IsStandardUserSystemClockAutomaticCorrectionEnabled(Out<bool> out_is_enabled);
    Result SetStandardUserSystemClockAutomaticCorrectionEnabled(bool automatic_correction);
    Result GetStandardUserSystemClockInitialYear(Out<s32> out_year);
    Result IsStandardNetworkSystemClockAccuracySufficient(Out<bool> out_is_sufficient);
    Result GetStandardUserSystemClockAutomaticCorrectionUpdatedTime(
        Out<Service::PSC::Time::SteadyClockTimePoint> out_time_point);
    Result CalculateMonotonicSystemClockBaseTimePoint(
        Out<s64> out_time, const Service::PSC::Time::SystemClockContext& context);
    Result GetClockSnapshot(OutClockSnapshot out_snapshot, Service::PSC::Time::TimeType type);
    Result GetClockSnapshotFromSystemClockContext(
        Service::PSC::Time::TimeType type, OutClockSnapshot out_snapshot,
        const Service::PSC::Time::SystemClockContext& user_context,
        const Service::PSC::Time::SystemClockContext& network_context);
    Result CalculateStandardUserSystemClockDifferenceByUser(Out<s64> out_difference,
                                                            InClockSnapshot a, InClockSnapshot b);
    Result CalculateSpanBetween(Out<s64> out_time, InClockSnapshot a, InClockSnapshot b);

private:
    Core::System& m_system;

    std::shared_ptr<Service::Set::ISystemSettingsServer> m_set_sys;
    std::shared_ptr<Service::PSC::Time::ServiceManager> m_time_m;
    std::shared_ptr<Service::PSC::Time::StaticService> m_wrapped_service;

    Service::PSC::Time::StaticServiceSetupInfo m_setup_info;
    std::shared_ptr<Service::PSC::Time::StaticService> m_time_sm;
    std::shared_ptr<Service::PSC::Time::TimeZoneService> m_time_zone;
    FileTimestampWorker& m_file_timestamp_worker;
    StandardSteadyClockResource& m_standard_steady_clock_resource;
    TimeZoneBinary& m_time_zone_binary;
};
} // namespace Service::Glue::Time
