// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "core/hle/kernel/k_auto_object.h"
#include "core/hle/kernel/k_thread.h"
#include "core/hle/result.h"

namespace Kernel {

class KLightSession;

class KLightServerSession final : public KAutoObject,
                                  public Common::IntrusiveListBaseNode<KLightServerSession> {
    KERNEL_AUTOOBJECT_TRAITS(KLightServerSession, KAutoObject);

private:
    KLightSession* m_parent{};
    KThread::WaiterList m_request_list{};
    KThread* m_current_request{};
    u64 m_server_thread_id{std::numeric_limits<u64>::max()};
    KThread* m_server_thread{};

public:
    explicit KLightServerSession(KernelCore& kernel);
    ~KLightServerSession();

    void Initialize(KLightSession* parent) {
        // Set member variables. */
        m_parent = parent;
    }

    virtual void Destroy() override;

    constexpr const KLightSession* GetParent() const {
        return m_parent;
    }

    Result OnRequest(KThread* request_thread);
    Result ReplyAndReceive(u32* data);

    void OnClientClosed();

private:
    void CleanupRequests();
};

} // namespace Kernel
