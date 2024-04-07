// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "core/hle/service/cmif_types.h"
#include "core/hle/service/ipc_helpers.h"
#include "core/hle/service/psc/time/common.h"
#include "core/hle/service/server_manager.h"
#include "core/hle/service/service.h"

namespace Core {
class System;
}

namespace Kernel {
class KSharedMemory;
}

namespace Service::PSC::Time {
class TimeManager;
class StandardLocalSystemClockCore;
class StandardUserSystemClockCore;
class StandardNetworkSystemClockCore;
class TimeZone;
class SystemClock;
class SteadyClock;
class TimeZoneService;
class EphemeralNetworkSystemClockCore;
class SharedMemory;

class StaticService final : public ServiceFramework<StaticService> {
    using InClockSnapshot = InLargeData<ClockSnapshot, BufferAttr_HipcPointer>;
    using OutClockSnapshot = OutLargeData<ClockSnapshot, BufferAttr_HipcPointer>;

public:
    explicit StaticService(Core::System& system, StaticServiceSetupInfo setup_info,
                           std::shared_ptr<TimeManager> time, const char* name);

    ~StaticService() override = default;

    Result GetStandardUserSystemClock(OutInterface<SystemClock> out_service);
    Result GetStandardNetworkSystemClock(OutInterface<SystemClock> out_service);
    Result GetStandardSteadyClock(OutInterface<SteadyClock> out_service);
    Result GetTimeZoneService(OutInterface<TimeZoneService> out_service);
    Result GetStandardLocalSystemClock(OutInterface<SystemClock> out_service);
    Result GetEphemeralNetworkSystemClock(OutInterface<SystemClock> out_service);
    Result GetSharedMemoryNativeHandle(OutCopyHandle<Kernel::KSharedMemory> out_shared_memory);
    Result SetStandardSteadyClockInternalOffset(s64 offset_ns);
    Result GetStandardSteadyClockRtcValue(Out<s64> out_rtc_value);
    Result IsStandardUserSystemClockAutomaticCorrectionEnabled(Out<bool> out_is_enabled);
    Result SetStandardUserSystemClockAutomaticCorrectionEnabled(bool automatic_correction);
    Result GetStandardUserSystemClockInitialYear(Out<s32> out_year);
    Result IsStandardNetworkSystemClockAccuracySufficient(Out<bool> out_is_sufficient);
    Result GetStandardUserSystemClockAutomaticCorrectionUpdatedTime(
        Out<SteadyClockTimePoint> out_time_point);
    Result CalculateMonotonicSystemClockBaseTimePoint(Out<s64> out_time,
                                                      const SystemClockContext& context);
    Result GetClockSnapshot(OutClockSnapshot out_snapshot, TimeType type);
    Result GetClockSnapshotFromSystemClockContext(TimeType type, OutClockSnapshot out_snapshot,
                                                  const SystemClockContext& user_context,
                                                  const SystemClockContext& network_context);
    Result CalculateStandardUserSystemClockDifferenceByUser(Out<s64> out_difference,
                                                            InClockSnapshot a, InClockSnapshot b);
    Result CalculateSpanBetween(Out<s64> out_time, InClockSnapshot a, InClockSnapshot b);

private:
    Result GetClockSnapshotImpl(OutClockSnapshot out_snapshot,
                                const SystemClockContext& user_context,
                                const SystemClockContext& network_context, TimeType type);

    Core::System& m_system;
    StaticServiceSetupInfo m_setup_info;
    std::shared_ptr<TimeManager> m_time;
    StandardLocalSystemClockCore& m_local_system_clock;
    StandardUserSystemClockCore& m_user_system_clock;
    StandardNetworkSystemClockCore& m_network_system_clock;
    TimeZone& m_time_zone;
    EphemeralNetworkSystemClockCore& m_ephemeral_network_clock;
    SharedMemory& m_shared_memory;
};

} // namespace Service::PSC::Time
