// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "core/hle/kernel/k_auto_object.h"
#include "core/hle/result.h"

namespace Kernel {

class KLightSession;

class KLightClientSession final : public KAutoObject {
    KERNEL_AUTOOBJECT_TRAITS(KLightClientSession, KAutoObject);

public:
    explicit KLightClientSession(KernelCore& kernel);
    ~KLightClientSession();

    void Initialize(KLightSession* parent) {
        // Set member variables.
        m_parent = parent;
    }

    virtual void Destroy() override;

    const KLightSession* GetParent() const {
        return m_parent;
    }

    Result SendSyncRequest(u32* data);

    void OnServerClosed();

private:
    KLightSession* m_parent;
};

} // namespace Kernel
