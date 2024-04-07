// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/scope_exit.h"
#include "core/core.h"
#include "core/hle/service/cmif_serialization.h"
#include "core/hle/service/psc/time/system_clock.h"

namespace Service::PSC::Time {

SystemClock::SystemClock(Core::System& system_, SystemClockCore& clock_core, bool can_write_clock,
                         bool can_write_uninitialized_clock)
    : ServiceFramework{system_, "ISystemClock"}, m_system{system}, m_clock_core{clock_core},
      m_can_write_clock{can_write_clock},
      m_can_write_uninitialized_clock{can_write_uninitialized_clock} {
    // clang-format off
    static const FunctionInfo functions[] = {
        {0, D<&SystemClock::GetCurrentTime>, "GetCurrentTime"},
        {1, D<&SystemClock::SetCurrentTime>, "SetCurrentTime"},
        {2, D<&SystemClock::GetSystemClockContext>, "GetSystemClockContext"},
        {3, D<&SystemClock::SetSystemClockContext>, "SetSystemClockContext"},
        {4, D<&SystemClock::GetOperationEventReadableHandle>, "GetOperationEventReadableHandle"},
    };
    // clang-format on
    RegisterHandlers(functions);
}

Result SystemClock::GetCurrentTime(Out<s64> out_time) {
    SCOPE_EXIT {
        LOG_DEBUG(Service_Time, "called. out_time={}", *out_time);
    };

    R_UNLESS(m_can_write_uninitialized_clock || m_clock_core.IsInitialized(),
             ResultClockUninitialized);

    R_RETURN(m_clock_core.GetCurrentTime(out_time.Get()));
}

Result SystemClock::SetCurrentTime(s64 time) {
    LOG_DEBUG(Service_Time, "called. time={}", time);

    R_UNLESS(m_can_write_clock, ResultPermissionDenied);
    R_UNLESS(m_can_write_uninitialized_clock || m_clock_core.IsInitialized(),
             ResultClockUninitialized);

    R_RETURN(m_clock_core.SetCurrentTime(time));
}

Result SystemClock::GetSystemClockContext(Out<SystemClockContext> out_context) {
    SCOPE_EXIT {
        LOG_DEBUG(Service_Time, "called. out_context={}", *out_context);
    };

    R_UNLESS(m_can_write_uninitialized_clock || m_clock_core.IsInitialized(),
             ResultClockUninitialized);

    R_RETURN(m_clock_core.GetContext(*out_context));
}

Result SystemClock::SetSystemClockContext(const SystemClockContext& context) {
    LOG_DEBUG(Service_Time, "called. context={}", context);

    R_UNLESS(m_can_write_clock, ResultPermissionDenied);
    R_UNLESS(m_can_write_uninitialized_clock || m_clock_core.IsInitialized(),
             ResultClockUninitialized);

    R_RETURN(m_clock_core.SetContextAndWrite(context));
}

Result SystemClock::GetOperationEventReadableHandle(
    OutCopyHandle<Kernel::KReadableEvent> out_event) {
    LOG_DEBUG(Service_Time, "called.");

    if (!m_operation_event) {
        m_operation_event = std::make_unique<OperationEvent>(m_system);
        R_UNLESS(m_operation_event != nullptr, ResultFailed);

        m_clock_core.LinkOperationEvent(*m_operation_event);
    }

    *out_event = &m_operation_event->m_event->GetReadableEvent();
    R_SUCCEED();
}

} // namespace Service::PSC::Time
