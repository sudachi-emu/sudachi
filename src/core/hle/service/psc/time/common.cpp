// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/core.h"
#include "core/hle/service/psc/time/common.h"

namespace Service::PSC::Time {
OperationEvent::OperationEvent(Core::System& system)
    : m_ctx{system, "Time:OperationEvent"},
      m_event{m_ctx.CreateEvent("Time:OperationEvent:Event")} {}

OperationEvent::~OperationEvent() {
    m_ctx.CloseEvent(m_event);
}

} // namespace Service::PSC::Time
