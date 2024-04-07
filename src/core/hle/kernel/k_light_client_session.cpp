// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/hle/kernel/k_light_client_session.h"
#include "core/hle/kernel/k_light_session.h"
#include "core/hle/kernel/k_thread.h"

namespace Kernel {

KLightClientSession::KLightClientSession(KernelCore& kernel) : KAutoObject(kernel) {}

KLightClientSession::~KLightClientSession() = default;

void KLightClientSession::Destroy() {
    m_parent->OnClientClosed();
}

void KLightClientSession::OnServerClosed() {}

Result KLightClientSession::SendSyncRequest(u32* data) {
    // Get the request thread.
    KThread* cur_thread = GetCurrentThreadPointer(m_kernel);

    // Set the light data.
    cur_thread->SetLightSessionData(data);

    // Send the request.
    R_RETURN(m_parent->OnRequest(cur_thread));
}

} // namespace Kernel
