// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/core.h"
#include "core/hle/service/psc/time/power_state_request_manager.h"

namespace Service::PSC::Time {

PowerStateRequestManager::PowerStateRequestManager(Core::System& system)
    : m_system{system}, m_ctx{system, "Psc:PowerStateRequestManager"},
      m_event{m_ctx.CreateEvent("Psc:PowerStateRequestManager:Event")} {}

PowerStateRequestManager::~PowerStateRequestManager() {
    m_ctx.CloseEvent(m_event);
}

void PowerStateRequestManager::UpdatePendingPowerStateRequestPriority(u32 priority) {
    std::scoped_lock l{m_mutex};
    if (m_has_pending_request) {
        m_pending_request_priority = std::max(m_pending_request_priority, priority);
    } else {
        m_pending_request_priority = priority;
        m_has_pending_request = true;
    }
}

void PowerStateRequestManager::SignalPowerStateRequestAvailability() {
    std::scoped_lock l{m_mutex};
    if (m_has_pending_request) {
        if (!m_has_available_request) {
            m_has_available_request = true;
        }
        m_has_pending_request = false;
        m_available_request_priority = m_pending_request_priority;
        m_event->Signal();
    }
}

bool PowerStateRequestManager::GetAndClearPowerStateRequest(u32& out_priority) {
    std::scoped_lock l{m_mutex};
    auto had_request{m_has_available_request};
    if (m_has_available_request) {
        out_priority = m_available_request_priority;
        m_has_available_request = false;
        m_event->Clear();
    }
    return had_request;
}

} // namespace Service::PSC::Time
