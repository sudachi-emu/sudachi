// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/scope_exit.h"
#include "core/core.h"
#include "core/core_timing.h"
#include "core/hle/kernel/k_shared_memory.h"
#include "core/hle/service/cmif_serialization.h"
#include "core/hle/service/psc/time/clocks/ephemeral_network_system_clock_core.h"
#include "core/hle/service/psc/time/clocks/standard_local_system_clock_core.h"
#include "core/hle/service/psc/time/clocks/standard_network_system_clock_core.h"
#include "core/hle/service/psc/time/clocks/standard_user_system_clock_core.h"
#include "core/hle/service/psc/time/manager.h"
#include "core/hle/service/psc/time/shared_memory.h"
#include "core/hle/service/psc/time/static.h"
#include "core/hle/service/psc/time/steady_clock.h"
#include "core/hle/service/psc/time/system_clock.h"
#include "core/hle/service/psc/time/time_zone.h"
#include "core/hle/service/psc/time/time_zone_service.h"

namespace Service::PSC::Time {
namespace {
constexpr Result GetTimeFromTimePointAndContext(s64* out_time, SteadyClockTimePoint& time_point,
                                                SystemClockContext& context) {
    R_UNLESS(out_time != nullptr, ResultInvalidArgument);
    R_UNLESS(time_point.IdMatches(context.steady_time_point), ResultClockMismatch);

    *out_time = context.offset + time_point.time_point;
    R_SUCCEED();
}
} // namespace

StaticService::StaticService(Core::System& system_, StaticServiceSetupInfo setup_info,
                             std::shared_ptr<TimeManager> time, const char* name)
    : ServiceFramework{system_, name}, m_system{system}, m_setup_info{setup_info}, m_time{time},
      m_local_system_clock{m_time->m_standard_local_system_clock},
      m_user_system_clock{m_time->m_standard_user_system_clock},
      m_network_system_clock{m_time->m_standard_network_system_clock},
      m_time_zone{m_time->m_time_zone},
      m_ephemeral_network_clock{m_time->m_ephemeral_network_clock},
      m_shared_memory{m_time->m_shared_memory} {
    // clang-format off
        static const FunctionInfo functions[] = {
            {0,   D<&StaticService::GetStandardUserSystemClock>, "GetStandardUserSystemClock"},
            {1,   D<&StaticService::GetStandardNetworkSystemClock>, "GetStandardNetworkSystemClock"},
            {2,   D<&StaticService::GetStandardSteadyClock>, "GetStandardSteadyClock"},
            {3,   D<&StaticService::GetTimeZoneService>, "GetTimeZoneService"},
            {4,   D<&StaticService::GetStandardLocalSystemClock>, "GetStandardLocalSystemClock"},
            {5,   D<&StaticService::GetEphemeralNetworkSystemClock>, "GetEphemeralNetworkSystemClock"},
            {20,  D<&StaticService::GetSharedMemoryNativeHandle>, "GetSharedMemoryNativeHandle"},
            {50,  D<&StaticService::SetStandardSteadyClockInternalOffset>, "SetStandardSteadyClockInternalOffset"},
            {51,  D<&StaticService::GetStandardSteadyClockRtcValue>, "GetStandardSteadyClockRtcValue"},
            {100, D<&StaticService::IsStandardUserSystemClockAutomaticCorrectionEnabled>, "IsStandardUserSystemClockAutomaticCorrectionEnabled"},
            {101, D<&StaticService::SetStandardUserSystemClockAutomaticCorrectionEnabled>, "SetStandardUserSystemClockAutomaticCorrectionEnabled"},
            {102, D<&StaticService::GetStandardUserSystemClockInitialYear>, "GetStandardUserSystemClockInitialYear"},
            {200, D<&StaticService::IsStandardNetworkSystemClockAccuracySufficient>, "IsStandardNetworkSystemClockAccuracySufficient"},
            {201, D<&StaticService::GetStandardUserSystemClockAutomaticCorrectionUpdatedTime>, "GetStandardUserSystemClockAutomaticCorrectionUpdatedTime"},
            {300, D<&StaticService::CalculateMonotonicSystemClockBaseTimePoint>, "CalculateMonotonicSystemClockBaseTimePoint"},
            {400, D<&StaticService::GetClockSnapshot>, "GetClockSnapshot"},
            {401, D<&StaticService::GetClockSnapshotFromSystemClockContext>, "GetClockSnapshotFromSystemClockContext"},
            {500, D<&StaticService::CalculateStandardUserSystemClockDifferenceByUser>, "CalculateStandardUserSystemClockDifferenceByUser"},
            {501, D<&StaticService::CalculateSpanBetween>, "CalculateSpanBetween"},
        };
    // clang-format on

    RegisterHandlers(functions);
}

Result StaticService::GetStandardUserSystemClock(OutInterface<SystemClock> out_service) {
    LOG_DEBUG(Service_Time, "called.");

    *out_service = std::make_shared<SystemClock>(m_system, m_user_system_clock,
                                                 m_setup_info.can_write_user_clock,
                                                 m_setup_info.can_write_uninitialized_clock);
    R_SUCCEED();
}

Result StaticService::GetStandardNetworkSystemClock(OutInterface<SystemClock> out_service) {
    LOG_DEBUG(Service_Time, "called.");

    *out_service = std::make_shared<SystemClock>(m_system, m_network_system_clock,
                                                 m_setup_info.can_write_network_clock,
                                                 m_setup_info.can_write_uninitialized_clock);
    R_SUCCEED();
}

Result StaticService::GetStandardSteadyClock(OutInterface<SteadyClock> out_service) {
    LOG_DEBUG(Service_Time, "called.");

    *out_service =
        std::make_shared<SteadyClock>(m_system, m_time, m_setup_info.can_write_steady_clock,
                                      m_setup_info.can_write_uninitialized_clock);
    R_SUCCEED();
}

Result StaticService::GetTimeZoneService(OutInterface<TimeZoneService> out_service) {
    LOG_DEBUG(Service_Time, "called.");

    *out_service =
        std::make_shared<TimeZoneService>(m_system, m_time->m_standard_steady_clock, m_time_zone,
                                          m_setup_info.can_write_timezone_device_location);
    R_SUCCEED();
}

Result StaticService::GetStandardLocalSystemClock(OutInterface<SystemClock> out_service) {
    LOG_DEBUG(Service_Time, "called.");

    *out_service = std::make_shared<SystemClock>(m_system, m_local_system_clock,
                                                 m_setup_info.can_write_local_clock,
                                                 m_setup_info.can_write_uninitialized_clock);
    R_SUCCEED();
}

Result StaticService::GetEphemeralNetworkSystemClock(OutInterface<SystemClock> out_service) {
    LOG_DEBUG(Service_Time, "called.");

    *out_service = std::make_shared<SystemClock>(m_system, m_ephemeral_network_clock,
                                                 m_setup_info.can_write_network_clock,
                                                 m_setup_info.can_write_uninitialized_clock);
    R_SUCCEED();
}

Result StaticService::GetSharedMemoryNativeHandle(
    OutCopyHandle<Kernel::KSharedMemory> out_shared_memory) {
    LOG_DEBUG(Service_Time, "called.");

    *out_shared_memory = &m_shared_memory.GetKSharedMemory();
    R_SUCCEED();
}

Result StaticService::SetStandardSteadyClockInternalOffset(s64 offset_ns) {
    LOG_DEBUG(Service_Time, "called. This function is not implemented!");

    R_UNLESS(m_setup_info.can_write_steady_clock, ResultPermissionDenied);

    R_RETURN(ResultNotImplemented);
}

Result StaticService::GetStandardSteadyClockRtcValue(Out<s64> out_rtc_value) {
    LOG_DEBUG(Service_Time, "called. This function is not implemented!");

    R_RETURN(ResultNotImplemented);
}

Result StaticService::IsStandardUserSystemClockAutomaticCorrectionEnabled(
    Out<bool> out_is_enabled) {
    SCOPE_EXIT {
        LOG_DEBUG(Service_Time, "called. out_is_enabled={}", *out_is_enabled);
    };

    R_UNLESS(m_user_system_clock.IsInitialized(), ResultClockUninitialized);

    *out_is_enabled = m_user_system_clock.GetAutomaticCorrection();

    R_SUCCEED();
}

Result StaticService::SetStandardUserSystemClockAutomaticCorrectionEnabled(
    bool automatic_correction) {
    LOG_DEBUG(Service_Time, "called. automatic_correction={}", automatic_correction);

    R_UNLESS(m_user_system_clock.IsInitialized() && m_time->m_standard_steady_clock.IsInitialized(),
             ResultClockUninitialized);
    R_UNLESS(m_setup_info.can_write_user_clock, ResultPermissionDenied);

    R_TRY(m_user_system_clock.SetAutomaticCorrection(automatic_correction));

    m_shared_memory.SetAutomaticCorrection(automatic_correction);

    SteadyClockTimePoint time_point{};
    R_TRY(m_time->m_standard_steady_clock.GetCurrentTimePoint(time_point));

    m_user_system_clock.SetTimePointAndSignal(time_point);
    m_user_system_clock.GetEvent().Signal();
    R_SUCCEED();
}

Result StaticService::GetStandardUserSystemClockInitialYear(Out<s32> out_year) {
    LOG_DEBUG(Service_Time, "called. This function is not implemented!");

    R_RETURN(ResultNotImplemented);
}

Result StaticService::IsStandardNetworkSystemClockAccuracySufficient(Out<bool> out_is_sufficient) {
    SCOPE_EXIT {
        LOG_DEBUG(Service_Time, "called. out_is_sufficient={}", *out_is_sufficient);
    };

    *out_is_sufficient = m_network_system_clock.IsAccuracySufficient();

    R_SUCCEED();
}

Result StaticService::GetStandardUserSystemClockAutomaticCorrectionUpdatedTime(
    Out<SteadyClockTimePoint> out_time_point) {
    SCOPE_EXIT {
        LOG_DEBUG(Service_Time, "called. out_time_point={}", *out_time_point);
    };

    R_UNLESS(m_user_system_clock.IsInitialized(), ResultClockUninitialized);

    m_user_system_clock.GetTimePoint(*out_time_point);

    R_SUCCEED();
}

Result StaticService::CalculateMonotonicSystemClockBaseTimePoint(
    Out<s64> out_time, const SystemClockContext& context) {
    SCOPE_EXIT {
        LOG_DEBUG(Service_Time, "called. context={} out_time={}", context, *out_time);
    };

    R_UNLESS(m_time->m_standard_steady_clock.IsInitialized(), ResultClockUninitialized);

    SteadyClockTimePoint time_point{};
    R_TRY(m_time->m_standard_steady_clock.GetCurrentTimePoint(time_point));

    R_UNLESS(time_point.IdMatches(context.steady_time_point), ResultClockMismatch);

    auto one_second_ns{
        std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::seconds(1)).count()};
    auto ticks{m_system.CoreTiming().GetClockTicks()};
    auto current_time_ns{ConvertToTimeSpan(ticks).count()};
    *out_time = ((context.offset + time_point.time_point) - (current_time_ns / one_second_ns));

    R_SUCCEED();
}

Result StaticService::GetClockSnapshot(OutClockSnapshot out_snapshot, TimeType type) {
    SCOPE_EXIT {
        LOG_DEBUG(Service_Time, "called. type={} out_snapshot={}", type, *out_snapshot);
    };

    SystemClockContext user_context{};
    R_TRY(m_user_system_clock.GetContext(user_context));

    SystemClockContext network_context{};
    R_TRY(m_network_system_clock.GetContext(network_context));

    R_RETURN(GetClockSnapshotImpl(out_snapshot, user_context, network_context, type));
}

Result StaticService::GetClockSnapshotFromSystemClockContext(
    TimeType type, OutClockSnapshot out_snapshot, const SystemClockContext& user_context,
    const SystemClockContext& network_context) {
    SCOPE_EXIT {
        LOG_DEBUG(Service_Time,
                  "called. type={} user_context={} network_context={} out_snapshot={}", type,
                  user_context, network_context, *out_snapshot);
    };

    R_RETURN(GetClockSnapshotImpl(out_snapshot, user_context, network_context, type));
}

Result StaticService::CalculateStandardUserSystemClockDifferenceByUser(Out<s64> out_difference,
                                                                       InClockSnapshot a,
                                                                       InClockSnapshot b) {
    SCOPE_EXIT {
        LOG_DEBUG(Service_Time, "called. a={} b={} out_difference={}", *a, *b, *out_difference);
    };

    auto diff_s =
        std::chrono::seconds(b->user_context.offset) - std::chrono::seconds(a->user_context.offset);

    if (a->user_context == b->user_context ||
        !a->user_context.steady_time_point.IdMatches(b->user_context.steady_time_point)) {
        *out_difference = 0;
        R_SUCCEED();
    }

    if (!a->is_automatic_correction_enabled || !b->is_automatic_correction_enabled) {
        *out_difference = std::chrono::duration_cast<std::chrono::nanoseconds>(diff_s).count();
        R_SUCCEED();
    }

    if (a->network_context.steady_time_point.IdMatches(a->steady_clock_time_point) ||
        b->network_context.steady_time_point.IdMatches(b->steady_clock_time_point)) {
        *out_difference = 0;
        R_SUCCEED();
    }

    *out_difference = std::chrono::duration_cast<std::chrono::nanoseconds>(diff_s).count();
    R_SUCCEED();
}

Result StaticService::CalculateSpanBetween(Out<s64> out_time, InClockSnapshot a,
                                           InClockSnapshot b) {
    SCOPE_EXIT {
        LOG_DEBUG(Service_Time, "called. a={} b={} out_time={}", *a, *b, *out_time);
    };

    s64 time_s{};
    auto res =
        GetSpanBetweenTimePoints(&time_s, a->steady_clock_time_point, b->steady_clock_time_point);

    if (res != ResultSuccess) {
        R_UNLESS(a->network_time != 0 && b->network_time != 0, ResultTimeNotFound);
        time_s = b->network_time - a->network_time;
    }

    *out_time =
        std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::seconds(time_s)).count();
    R_SUCCEED();
}

Result StaticService::GetClockSnapshotImpl(OutClockSnapshot out_snapshot,
                                           const SystemClockContext& user_context,
                                           const SystemClockContext& network_context,
                                           TimeType type) {
    out_snapshot->user_context = user_context;
    out_snapshot->network_context = network_context;

    R_TRY(
        m_time->m_standard_steady_clock.GetCurrentTimePoint(out_snapshot->steady_clock_time_point));

    out_snapshot->is_automatic_correction_enabled = m_user_system_clock.GetAutomaticCorrection();

    R_TRY(m_time_zone.GetLocationName(out_snapshot->location_name));

    R_TRY(GetTimeFromTimePointAndContext(&out_snapshot->user_time,
                                         out_snapshot->steady_clock_time_point,
                                         out_snapshot->user_context));

    R_TRY(m_time_zone.ToCalendarTimeWithMyRule(out_snapshot->user_calendar_time,
                                               out_snapshot->user_calendar_additional_time,
                                               out_snapshot->user_time));

    if (GetTimeFromTimePointAndContext(&out_snapshot->network_time,
                                       out_snapshot->steady_clock_time_point,
                                       out_snapshot->network_context) != ResultSuccess) {
        out_snapshot->network_time = 0;
    }

    R_TRY(m_time_zone.ToCalendarTimeWithMyRule(out_snapshot->network_calendar_time,
                                               out_snapshot->network_calendar_additional_time,
                                               out_snapshot->network_time));
    out_snapshot->type = type;
    out_snapshot->unk_CE = 0;
    R_SUCCEED();
}

} // namespace Service::PSC::Time
