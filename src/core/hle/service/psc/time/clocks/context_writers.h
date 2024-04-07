// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <list>

#include "common/common_types.h"
#include "core/hle/kernel/k_event.h"
#include "core/hle/service/psc/time/clocks/system_clock_core.h"
#include "core/hle/service/psc/time/common.h"
#include "core/hle/service/psc/time/shared_memory.h"

namespace Core {
class System;
}

namespace Service::PSC::Time {

class ContextWriter {
private:
    using OperationEventList = Common::IntrusiveListBaseTraits<OperationEvent>::ListType;

public:
    virtual ~ContextWriter() = default;

    virtual Result Write(const SystemClockContext& context) = 0;
    void SignalAllNodes();
    void Link(OperationEvent& operation_event);

private:
    OperationEventList m_operation_events;
    std::mutex m_mutex;
};

class LocalSystemClockContextWriter : public ContextWriter {
public:
    explicit LocalSystemClockContextWriter(Core::System& system, SharedMemory& shared_memory);

    Result Write(const SystemClockContext& context) override;

private:
    Core::System& m_system;

    SharedMemory& m_shared_memory;
    bool m_in_use{};
    SystemClockContext m_context{};
};

class NetworkSystemClockContextWriter : public ContextWriter {
public:
    explicit NetworkSystemClockContextWriter(Core::System& system, SharedMemory& shared_memory,
                                             SystemClockCore& system_clock);

    Result Write(const SystemClockContext& context) override;

private:
    Core::System& m_system;

    SharedMemory& m_shared_memory;
    bool m_in_use{};
    SystemClockContext m_context{};
    SystemClockCore& m_system_clock;
};

class EphemeralNetworkSystemClockContextWriter : public ContextWriter {
public:
    EphemeralNetworkSystemClockContextWriter(Core::System& system);

    Result Write(const SystemClockContext& context) override;

private:
    Core::System& m_system;

    bool m_in_use{};
    SystemClockContext m_context{};
};

} // namespace Service::PSC::Time
