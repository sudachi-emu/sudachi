// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <chrono>

#include "common/scope_exit.h"
#include "core/core.h"
#include "core/hle/kernel/k_shared_memory.h"
#include "core/hle/kernel/svc.h"
#include "core/hle/service/cmif_serialization.h"
#include "core/hle/service/glue/time/file_timestamp_worker.h"
#include "core/hle/service/glue/time/static.h"
#include "core/hle/service/psc/time/errors.h"
#include "core/hle/service/psc/time/service_manager.h"
#include "core/hle/service/psc/time/static.h"
#include "core/hle/service/psc/time/steady_clock.h"
#include "core/hle/service/psc/time/system_clock.h"
#include "core/hle/service/psc/time/time_zone_service.h"
#include "core/hle/service/set/system_settings_server.h"
#include "core/hle/service/sm/sm.h"

namespace Service::Glue::Time {

StaticService::StaticService(Core::System& system_,
                             Service::PSC::Time::StaticServiceSetupInfo setup_info,
                             std::shared_ptr<TimeManager> time, const char* name)
    : ServiceFramework{system_, name}, m_system{system_}, m_time_m{time->m_time_m},
      m_setup_info{setup_info}, m_time_sm{time->m_time_sm},
      m_file_timestamp_worker{time->m_file_timestamp_worker},
      m_standard_steady_clock_resource{time->m_steady_clock_resource},
      m_time_zone_binary{time->m_time_zone_binary} {
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

    m_set_sys =
        m_system.ServiceManager().GetService<Service::Set::ISystemSettingsServer>("set:sys", true);

    if (m_setup_info.can_write_local_clock && m_setup_info.can_write_user_clock &&
        !m_setup_info.can_write_network_clock && m_setup_info.can_write_timezone_device_location &&
        !m_setup_info.can_write_steady_clock && !m_setup_info.can_write_uninitialized_clock) {
        m_time_m->GetStaticServiceAsAdmin(&m_wrapped_service);
    } else if (!m_setup_info.can_write_local_clock && !m_setup_info.can_write_user_clock &&
               !m_setup_info.can_write_network_clock &&
               !m_setup_info.can_write_timezone_device_location &&
               !m_setup_info.can_write_steady_clock &&
               !m_setup_info.can_write_uninitialized_clock) {
        m_time_m->GetStaticServiceAsUser(&m_wrapped_service);
    } else if (!m_setup_info.can_write_local_clock && !m_setup_info.can_write_user_clock &&
               !m_setup_info.can_write_network_clock &&
               !m_setup_info.can_write_timezone_device_location &&
               m_setup_info.can_write_steady_clock && !m_setup_info.can_write_uninitialized_clock) {
        m_time_m->GetStaticServiceAsRepair(&m_wrapped_service);
    } else {
        UNREACHABLE();
    }

    auto res = m_wrapped_service->GetTimeZoneService(&m_time_zone);
    ASSERT(res == ResultSuccess);
}

Result StaticService::GetStandardUserSystemClock(
    OutInterface<Service::PSC::Time::SystemClock> out_service) {
    LOG_DEBUG(Service_Time, "called.");

    R_RETURN(m_wrapped_service->GetStandardUserSystemClock(out_service));
}

Result StaticService::GetStandardNetworkSystemClock(
    OutInterface<Service::PSC::Time::SystemClock> out_service) {
    LOG_DEBUG(Service_Time, "called.");

    R_RETURN(m_wrapped_service->GetStandardNetworkSystemClock(out_service));
}

Result StaticService::GetStandardSteadyClock(
    OutInterface<Service::PSC::Time::SteadyClock> out_service) {
    LOG_DEBUG(Service_Time, "called.");

    R_RETURN(m_wrapped_service->GetStandardSteadyClock(out_service));
}

Result StaticService::GetTimeZoneService(OutInterface<TimeZoneService> out_service) {
    LOG_DEBUG(Service_Time, "called.");

    *out_service = std::make_shared<TimeZoneService>(
        m_system, m_file_timestamp_worker, m_setup_info.can_write_timezone_device_location,
        m_time_zone_binary, m_time_zone);
    R_SUCCEED();
}

Result StaticService::GetStandardLocalSystemClock(
    OutInterface<Service::PSC::Time::SystemClock> out_service) {
    LOG_DEBUG(Service_Time, "called.");

    R_RETURN(m_wrapped_service->GetStandardLocalSystemClock(out_service));
}

Result StaticService::GetEphemeralNetworkSystemClock(
    OutInterface<Service::PSC::Time::SystemClock> out_service) {
    LOG_DEBUG(Service_Time, "called.");

    R_RETURN(m_wrapped_service->GetEphemeralNetworkSystemClock(out_service));
}

Result StaticService::GetSharedMemoryNativeHandle(
    OutCopyHandle<Kernel::KSharedMemory> out_shared_memory) {
    LOG_DEBUG(Service_Time, "called.");

    R_RETURN(m_wrapped_service->GetSharedMemoryNativeHandle(out_shared_memory));
}

Result StaticService::SetStandardSteadyClockInternalOffset(s64 offset_ns) {
    LOG_DEBUG(Service_Time, "called. offset_ns={}", offset_ns);

    R_UNLESS(m_setup_info.can_write_steady_clock, Service::PSC::Time::ResultPermissionDenied);

    R_RETURN(m_set_sys->SetExternalSteadyClockInternalOffset(
        offset_ns /
        std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::seconds(1)).count()));
}

Result StaticService::GetStandardSteadyClockRtcValue(Out<s64> out_rtc_value) {
    SCOPE_EXIT {
        LOG_DEBUG(Service_Time, "called. out_rtc_value={}", *out_rtc_value);
    };

    R_RETURN(m_standard_steady_clock_resource.GetRtcTimeInSeconds(*out_rtc_value));
}

Result StaticService::IsStandardUserSystemClockAutomaticCorrectionEnabled(
    Out<bool> out_automatic_correction) {
    SCOPE_EXIT {
        LOG_DEBUG(Service_Time, "called. out_automatic_correction={}", *out_automatic_correction);
    };

    R_RETURN(m_wrapped_service->IsStandardUserSystemClockAutomaticCorrectionEnabled(
        out_automatic_correction));
}

Result StaticService::SetStandardUserSystemClockAutomaticCorrectionEnabled(
    bool automatic_correction) {
    LOG_DEBUG(Service_Time, "called. automatic_correction={}", automatic_correction);

    R_RETURN(m_wrapped_service->SetStandardUserSystemClockAutomaticCorrectionEnabled(
        automatic_correction));
}

Result StaticService::GetStandardUserSystemClockInitialYear(Out<s32> out_year) {
    SCOPE_EXIT {
        LOG_DEBUG(Service_Time, "called. out_year={}", *out_year);
    };

    R_RETURN(m_set_sys->GetSettingsItemValueImpl<s32>(*out_year, "time",
                                                      "standard_user_clock_initial_year"));
}

Result StaticService::IsStandardNetworkSystemClockAccuracySufficient(Out<bool> out_is_sufficient) {
    SCOPE_EXIT {
        LOG_DEBUG(Service_Time, "called. out_is_sufficient={}", *out_is_sufficient);
    };

    R_RETURN(m_wrapped_service->IsStandardNetworkSystemClockAccuracySufficient(out_is_sufficient));
}

Result StaticService::GetStandardUserSystemClockAutomaticCorrectionUpdatedTime(
    Out<Service::PSC::Time::SteadyClockTimePoint> out_time_point) {
    SCOPE_EXIT {
        LOG_DEBUG(Service_Time, "called. out_time_point={}", *out_time_point);
    };

    R_RETURN(m_wrapped_service->GetStandardUserSystemClockAutomaticCorrectionUpdatedTime(
        out_time_point));
}

Result StaticService::CalculateMonotonicSystemClockBaseTimePoint(
    Out<s64> out_time, const Service::PSC::Time::SystemClockContext& context) {
    SCOPE_EXIT {
        LOG_DEBUG(Service_Time, "called. context={} out_time={}", context, *out_time);
    };

    R_RETURN(m_wrapped_service->CalculateMonotonicSystemClockBaseTimePoint(out_time, context));
}

Result StaticService::GetClockSnapshot(OutClockSnapshot out_snapshot,
                                       Service::PSC::Time::TimeType type) {
    SCOPE_EXIT {
        LOG_DEBUG(Service_Time, "called. type={} out_snapshot={}", type, *out_snapshot);
    };

    R_RETURN(m_wrapped_service->GetClockSnapshot(out_snapshot, type));
}

Result StaticService::GetClockSnapshotFromSystemClockContext(
    Service::PSC::Time::TimeType type, OutClockSnapshot out_snapshot,
    const Service::PSC::Time::SystemClockContext& user_context,
    const Service::PSC::Time::SystemClockContext& network_context) {
    SCOPE_EXIT {
        LOG_DEBUG(Service_Time,
                  "called. type={} out_snapshot={} user_context={} network_context={}", type,
                  *out_snapshot, user_context, network_context);
    };

    R_RETURN(m_wrapped_service->GetClockSnapshotFromSystemClockContext(
        type, out_snapshot, user_context, network_context));
}

Result StaticService::CalculateStandardUserSystemClockDifferenceByUser(Out<s64> out_time,
                                                                       InClockSnapshot a,
                                                                       InClockSnapshot b) {
    SCOPE_EXIT {
        LOG_DEBUG(Service_Time, "called. a={} b={} out_time={}", *a, *b, *out_time);
    };

    R_RETURN(m_wrapped_service->CalculateStandardUserSystemClockDifferenceByUser(out_time, a, b));
}

Result StaticService::CalculateSpanBetween(Out<s64> out_time, InClockSnapshot a,
                                           InClockSnapshot b) {
    SCOPE_EXIT {
        LOG_DEBUG(Service_Time, "called. a={} b={} out_time={}", *a, *b, *out_time);
    };

    R_RETURN(m_wrapped_service->CalculateSpanBetween(out_time, a, b));
}

} // namespace Service::Glue::Time
