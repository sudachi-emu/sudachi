// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <list>
#include <mutex>
#include <optional>
#include <vector>

#include "common/polyfill_thread.h"
#include "common/thread.h"
#include "core/hle/result.h"
#include "core/hle/service/hle_ipc.h"
#include "core/hle/service/os/multi_wait.h"
#include "core/hle/service/os/mutex.h"

namespace Core {
class System;
}

namespace Kernel {
class KEvent;
class KServerPort;
class KServerSession;
} // namespace Kernel

namespace Service {

class Port;
class Session;

class ServerManager {
public:
    explicit ServerManager(Core::System& system);
    ~ServerManager();

    Result RegisterSession(Kernel::KServerSession* session,
                           std::shared_ptr<SessionRequestManager> manager);
    Result RegisterNamedService(const std::string& service_name,
                                SessionRequestHandlerFactory&& handler_factory,
                                u32 max_sessions = 64);
    Result RegisterNamedService(const std::string& service_name,
                                std::shared_ptr<SessionRequestHandler>&& handler,
                                u32 max_sessions = 64);
    Result ManageNamedPort(const std::string& service_name,
                           SessionRequestHandlerFactory&& handler_factory, u32 max_sessions = 64);
    Result ManageDeferral(Kernel::KEvent** out_event);

    Result LoopProcess();
    void StartAdditionalHostThreads(const char* name, size_t num_threads);

    static void RunServer(std::unique_ptr<ServerManager>&& server);

private:
    void LinkToDeferredList(MultiWaitHolder* holder);
    void LinkDeferred();
    MultiWaitHolder* WaitSignaled();
    Result Process(MultiWaitHolder* holder);
    bool WaitAndProcessImpl();
    Result LoopProcessImpl();

    Result OnPortEvent(Port* port);
    Result OnSessionEvent(Session* session);
    Result OnDeferralEvent();
    Result CompleteSyncRequest(Session* session);

private:
    void DestroySession(Session* session);

private:
    Core::System& m_system;
    Mutex m_selection_mutex;

    // Events
    Kernel::KEvent* m_wakeup_event{};
    Kernel::KEvent* m_deferral_event{};

    // Deferred wait list
    std::mutex m_deferred_list_mutex{};
    MultiWait m_deferred_list{};

    // Guest state tracking
    MultiWait m_multi_wait{};
    Common::IntrusiveListBaseTraits<Port>::ListType m_servers{};
    Common::IntrusiveListBaseTraits<Session>::ListType m_sessions{};
    std::list<Session*> m_deferred_sessions{};
    std::optional<MultiWaitHolder> m_wakeup_holder{};
    std::optional<MultiWaitHolder> m_deferral_holder{};

    // Host state tracking
    Common::Event m_stopped{};
    std::vector<std::jthread> m_threads{};
    std::stop_source m_stop_source{};
};

} // namespace Service
