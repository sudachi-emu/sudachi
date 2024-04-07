// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/scope_exit.h"
#include "core/core.h"
#include "core/hle/service/cmif_serialization.h"
#include "core/hle/service/psc/time/steady_clock.h"

namespace Service::PSC::Time {

SteadyClock::SteadyClock(Core::System& system_, std::shared_ptr<TimeManager> manager,
                         bool can_write_steady_clock, bool can_write_uninitialized_clock)
    : ServiceFramework{system_, "ISteadyClock"}, m_system{system},
      m_clock_core{manager->m_standard_steady_clock},
      m_can_write_steady_clock{can_write_steady_clock},
      m_can_write_uninitialized_clock{can_write_uninitialized_clock} {
    // clang-format off
         static const FunctionInfo functions[] = {
        {0, D<&SteadyClock::GetCurrentTimePoint>, "GetCurrentTimePoint"},
        {2, D<&SteadyClock::GetTestOffset>, "GetTestOffset"},
        {3, D<&SteadyClock::SetTestOffset>, "SetTestOffset"},
        {100, D<&SteadyClock::GetRtcValue>, "GetRtcValue"},
        {101, D<&SteadyClock::IsRtcResetDetected>, "IsRtcResetDetected"},
        {102, D<&SteadyClock::GetSetupResultValue>, "GetSetupResultValue"},
        {200, D<&SteadyClock::GetInternalOffset>, "GetInternalOffset"},
    };
    // clang-format on
    RegisterHandlers(functions);
}

Result SteadyClock::GetCurrentTimePoint(Out<SteadyClockTimePoint> out_time_point) {
    SCOPE_EXIT {
        LOG_DEBUG(Service_Time, "called. out_time_point={}", *out_time_point);
    };

    R_UNLESS(m_can_write_uninitialized_clock || m_clock_core.IsInitialized(),
             ResultClockUninitialized);

    R_RETURN(m_clock_core.GetCurrentTimePoint(*out_time_point));
}

Result SteadyClock::GetTestOffset(Out<s64> out_test_offset) {
    SCOPE_EXIT {
        LOG_DEBUG(Service_Time, "called. out_test_offset={}", *out_test_offset);
    };

    R_UNLESS(m_can_write_uninitialized_clock || m_clock_core.IsInitialized(),
             ResultClockUninitialized);

    *out_test_offset = m_clock_core.GetTestOffset();
    R_SUCCEED();
}

Result SteadyClock::SetTestOffset(s64 test_offset) {
    LOG_DEBUG(Service_Time, "called. test_offset={}", test_offset);

    R_UNLESS(m_can_write_steady_clock, ResultPermissionDenied);
    R_UNLESS(m_can_write_uninitialized_clock || m_clock_core.IsInitialized(),
             ResultClockUninitialized);

    m_clock_core.SetTestOffset(test_offset);
    R_SUCCEED();
}

Result SteadyClock::GetRtcValue(Out<s64> out_rtc_value) {
    SCOPE_EXIT {
        LOG_DEBUG(Service_Time, "called. out_rtc_value={}", *out_rtc_value);
    };

    R_UNLESS(m_can_write_uninitialized_clock || m_clock_core.IsInitialized(),
             ResultClockUninitialized);

    R_RETURN(m_clock_core.GetRtcValue(*out_rtc_value));
}

Result SteadyClock::IsRtcResetDetected(Out<bool> out_is_detected) {
    SCOPE_EXIT {
        LOG_DEBUG(Service_Time, "called. out_is_detected={}", *out_is_detected);
    };

    R_UNLESS(m_can_write_uninitialized_clock || m_clock_core.IsInitialized(),
             ResultClockUninitialized);

    *out_is_detected = m_clock_core.IsResetDetected();
    R_SUCCEED();
}

Result SteadyClock::GetSetupResultValue(Out<Result> out_result) {
    SCOPE_EXIT {
        LOG_DEBUG(Service_Time, "called. out_result=0x{:X}", out_result->raw);
    };

    R_UNLESS(m_can_write_uninitialized_clock || m_clock_core.IsInitialized(),
             ResultClockUninitialized);

    *out_result = m_clock_core.GetSetupResultValue();
    R_SUCCEED();
}

Result SteadyClock::GetInternalOffset(Out<s64> out_internal_offset) {
    SCOPE_EXIT {
        LOG_DEBUG(Service_Time, "called. out_internal_offset={}", *out_internal_offset);
    };

    R_UNLESS(m_can_write_uninitialized_clock || m_clock_core.IsInitialized(),
             ResultClockUninitialized);

    *out_internal_offset = m_clock_core.GetInternalOffset();
    R_SUCCEED();
}

} // namespace Service::PSC::Time
