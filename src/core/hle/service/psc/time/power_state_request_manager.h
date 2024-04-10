// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <mutex>

#include "core/hle/kernel/k_event.h"
#include "core/hle/service/kernel_helpers.h"

namespace Core {
class System;
}

namespace Service::PSC::Time {

class PowerStateRequestManager {
public:
    explicit PowerStateRequestManager(Core::System& system);
    ~PowerStateRequestManager();

    Kernel::KReadableEvent& GetReadableEvent() {
        return m_event->GetReadableEvent();
    }

    void UpdatePendingPowerStateRequestPriority(u32 priority);
    void SignalPowerStateRequestAvailability();
    bool GetAndClearPowerStateRequest(u32& out_priority);

private:
    Core::System& m_system;
    KernelHelpers::ServiceContext m_ctx;

    Kernel::KEvent* m_event{};
    bool m_has_pending_request{};
    u32 m_pending_request_priority{};
    bool m_has_available_request{};
    u32 m_available_request_priority{};
    std::mutex m_mutex;
};

} // namespace Service::PSC::Time
