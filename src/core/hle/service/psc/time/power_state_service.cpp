// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/hle/service/cmif_serialization.h"
#include "core/hle/service/psc/time/power_state_service.h"

namespace Service::PSC::Time {

IPowerStateRequestHandler::IPowerStateRequestHandler(
    Core::System& system_, PowerStateRequestManager& power_state_request_manager)
    : ServiceFramework{system_, "time:p"}, m_system{system},
      m_power_state_request_manager{power_state_request_manager} {
    // clang-format off
        static const FunctionInfo functions[] = {
            {0, D<&IPowerStateRequestHandler::GetPowerStateRequestEventReadableHandle>, "GetPowerStateRequestEventReadableHandle"},
            {1, D<&IPowerStateRequestHandler::GetAndClearPowerStateRequest>, "GetAndClearPowerStateRequest"},
        };
    // clang-format on

    RegisterHandlers(functions);
}

Result IPowerStateRequestHandler::GetPowerStateRequestEventReadableHandle(
    OutCopyHandle<Kernel::KReadableEvent> out_event) {
    LOG_DEBUG(Service_Time, "called.");

    *out_event = &m_power_state_request_manager.GetReadableEvent();
    R_SUCCEED();
}

Result IPowerStateRequestHandler::GetAndClearPowerStateRequest(Out<bool> out_cleared,
                                                               Out<u32> out_priority) {
    LOG_DEBUG(Service_Time, "called.");

    u32 priority{};
    auto cleared = m_power_state_request_manager.GetAndClearPowerStateRequest(priority);
    *out_cleared = cleared;

    if (cleared) {
        *out_priority = priority;
    }
    R_SUCCEED();
}

} // namespace Service::PSC::Time
