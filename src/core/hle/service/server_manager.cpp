// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/scope_exit.h"

#include "core/core.h"
#include "core/hle/kernel/k_client_port.h"
#include "core/hle/kernel/k_client_session.h"
#include "core/hle/kernel/k_event.h"
#include "core/hle/kernel/k_object_name.h"
#include "core/hle/kernel/k_port.h"
#include "core/hle/kernel/k_server_port.h"
#include "core/hle/kernel/k_server_session.h"
#include "core/hle/kernel/k_synchronization_object.h"
#include "core/hle/kernel/svc_results.h"
#include "core/hle/service/hle_ipc.h"
#include "core/hle/service/ipc_helpers.h"
#include "core/hle/service/server_manager.h"
#include "core/hle/service/sm/sm.h"

namespace Service {

enum class UserDataTag {
    Port,
    Session,
    DeferEvent,
};

class Port : public MultiWaitHolder, public Common::IntrusiveListBaseNode<Port> {
public:
    explicit Port(Kernel::KServerPort* server_port, SessionRequestHandlerFactory&& handler_factory)
        : MultiWaitHolder(server_port), m_handler_factory(std::move(handler_factory)) {
        this->SetUserData(static_cast<uintptr_t>(UserDataTag::Port));
    }

    ~Port() {
        this->GetNativeHandle()->Close();
    }

    SessionRequestHandlerPtr CreateHandler() {
        return m_handler_factory();
    }

private:
    const SessionRequestHandlerFactory m_handler_factory;
};

class Session : public MultiWaitHolder, public Common::IntrusiveListBaseNode<Session> {
public:
    explicit Session(Kernel::KServerSession* server_session,
                     std::shared_ptr<SessionRequestManager>&& manager)
        : MultiWaitHolder(server_session), m_manager(std::move(manager)) {
        this->SetUserData(static_cast<uintptr_t>(UserDataTag::Session));
    }

    ~Session() {
        this->GetNativeHandle()->Close();
    }

    std::shared_ptr<SessionRequestManager>& GetManager() {
        return m_manager;
    }

    std::shared_ptr<HLERequestContext>& GetContext() {
        return m_context;
    }

private:
    std::shared_ptr<SessionRequestManager> m_manager;
    std::shared_ptr<HLERequestContext> m_context;
};

ServerManager::ServerManager(Core::System& system) : m_system{system}, m_selection_mutex{system} {
    // Initialize event.
    m_wakeup_event = Kernel::KEvent::Create(system.Kernel());
    m_wakeup_event->Initialize(nullptr);

    // Register event.
    Kernel::KEvent::Register(system.Kernel(), m_wakeup_event);

    // Link to holder.
    m_wakeup_holder.emplace(std::addressof(m_wakeup_event->GetReadableEvent()));
    m_wakeup_holder->LinkToMultiWait(std::addressof(m_deferred_list));
}

ServerManager::~ServerManager() {
    // Signal stop.
    m_stop_source.request_stop();
    m_wakeup_event->Signal();

    // Wait for processing to stop.
    m_stopped.Wait();
    m_threads.clear();

    // Clean up ports.
    auto port_it = m_servers.begin();
    while (port_it != m_servers.end()) {
        auto* const port = std::addressof(*port_it);
        port_it = m_servers.erase(port_it);
        delete port;
    }

    // Clean up sessions.
    auto session_it = m_sessions.begin();
    while (session_it != m_sessions.end()) {
        auto* const session = std::addressof(*session_it);
        session_it = m_sessions.erase(session_it);
        delete session;
    }

    // Close wakeup event.
    m_wakeup_event->GetReadableEvent().Close();
    m_wakeup_event->Close();

    if (m_deferral_event) {
        m_deferral_event->GetReadableEvent().Close();
        // Write event is owned by ServiceManager
    }
}

void ServerManager::RunServer(std::unique_ptr<ServerManager>&& server_manager) {
    server_manager->m_system.RunServer(std::move(server_manager));
}

Result ServerManager::RegisterSession(Kernel::KServerSession* server_session,
                                      std::shared_ptr<SessionRequestManager> manager) {
    // We are taking ownership of the server session, so don't open it.
    auto* session = new Session(server_session, std::move(manager));

    // Begin tracking the server session.
    {
        std::scoped_lock ll{m_deferred_list_mutex};
        m_sessions.push_back(*session);
    }

    // Register to wait on the session.
    this->LinkToDeferredList(session);

    R_SUCCEED();
}

Result ServerManager::RegisterNamedService(const std::string& service_name,
                                           SessionRequestHandlerFactory&& handler_factory,
                                           u32 max_sessions) {
    // Add the new server to sm: and get the moved server port.
    Kernel::KServerPort* server_port{};
    R_ASSERT(m_system.ServiceManager().RegisterService(std::addressof(server_port), service_name,
                                                       max_sessions, handler_factory));

    // We are taking ownership of the server port, so don't open it.
    auto* server = new Port(server_port, std::move(handler_factory));

    // Begin tracking the server port.
    {
        std::scoped_lock ll{m_deferred_list_mutex};
        m_servers.push_back(*server);
    }

    // Register to wait on the server port.
    this->LinkToDeferredList(server);

    R_SUCCEED();
}

Result ServerManager::RegisterNamedService(const std::string& service_name,
                                           std::shared_ptr<SessionRequestHandler>&& handler,
                                           u32 max_sessions) {
    // Make the factory.
    const auto HandlerFactory = [handler]() { return handler; };

    // Register the service with the new factory.
    R_RETURN(this->RegisterNamedService(service_name, std::move(HandlerFactory), max_sessions));
}

Result ServerManager::ManageNamedPort(const std::string& service_name,
                                      SessionRequestHandlerFactory&& handler_factory,
                                      u32 max_sessions) {
    // Create a new port.
    auto* port = Kernel::KPort::Create(m_system.Kernel());
    port->Initialize(max_sessions, false, 0);

    // Register the port.
    Kernel::KPort::Register(m_system.Kernel(), port);

    // Ensure that our reference to the port is closed if we fail to register it.
    SCOPE_EXIT {
        port->GetClientPort().Close();
        port->GetServerPort().Close();
    };

    // Register the object name with the kernel.
    R_TRY(Kernel::KObjectName::NewFromName(m_system.Kernel(), std::addressof(port->GetClientPort()),
                                           service_name.c_str()));

    // Open a new reference to the server port.
    port->GetServerPort().Open();

    // Transfer ownership into a new port object.
    auto* server = new Port(std::addressof(port->GetServerPort()), std::move(handler_factory));

    // Begin tracking the port.
    {
        std::scoped_lock ll{m_deferred_list_mutex};
        m_servers.push_back(*server);
    }

    // Register to wait on the port.
    this->LinkToDeferredList(server);

    // We succeeded.
    R_SUCCEED();
}

Result ServerManager::ManageDeferral(Kernel::KEvent** out_event) {
    // Create a new event.
    m_deferral_event = Kernel::KEvent::Create(m_system.Kernel());
    ASSERT(m_deferral_event != nullptr);

    // Initialize the event.
    m_deferral_event->Initialize(nullptr);

    // Register the event.
    Kernel::KEvent::Register(m_system.Kernel(), m_deferral_event);

    // Set the output.
    *out_event = m_deferral_event;

    // Register to wait on the event.
    m_deferral_holder.emplace(std::addressof(m_deferral_event->GetReadableEvent()));
    m_deferral_holder->SetUserData(static_cast<uintptr_t>(UserDataTag::DeferEvent));
    this->LinkToDeferredList(std::addressof(*m_deferral_holder));

    // We succeeded.
    R_SUCCEED();
}

void ServerManager::StartAdditionalHostThreads(const char* name, size_t num_threads) {
    for (size_t i = 0; i < num_threads; i++) {
        auto thread_name = fmt::format("{}:{}", name, i + 1);
        m_threads.emplace_back(m_system.Kernel().RunOnHostCoreThread(
            std::move(thread_name), [&] { this->LoopProcessImpl(); }));
    }
}

Result ServerManager::LoopProcess() {
    SCOPE_EXIT {
        m_stopped.Set();
    };

    R_RETURN(this->LoopProcessImpl());
}

void ServerManager::LinkToDeferredList(MultiWaitHolder* holder) {
    // Link.
    {
        std::scoped_lock lk{m_deferred_list_mutex};
        holder->LinkToMultiWait(std::addressof(m_deferred_list));
    }

    // Signal the wakeup event.
    m_wakeup_event->Signal();
}

void ServerManager::LinkDeferred() {
    std::scoped_lock lk{m_deferred_list_mutex};
    m_multi_wait.MoveAll(std::addressof(m_deferred_list));
}

MultiWaitHolder* ServerManager::WaitSignaled() {
    // Ensure we are the only thread waiting for this server.
    std::scoped_lock lk{m_selection_mutex};

    while (true) {
        this->LinkDeferred();

        // If we're done, return before we start waiting.
        if (m_stop_source.stop_requested()) {
            return nullptr;
        }

        auto* selected = m_multi_wait.WaitAny(m_system.Kernel());
        if (selected == std::addressof(*m_wakeup_holder)) {
            // Clear and restart if we were woken up.
            m_wakeup_event->Clear();
        } else {
            // Unlink and handle the event.
            selected->UnlinkFromMultiWait();
            return selected;
        }
    }
}

Result ServerManager::Process(MultiWaitHolder* holder) {
    switch (static_cast<UserDataTag>(holder->GetUserData())) {
    case UserDataTag::Session:
        R_RETURN(this->OnSessionEvent(static_cast<Session*>(holder)));
    case UserDataTag::Port:
        R_RETURN(this->OnPortEvent(static_cast<Port*>(holder)));
    case UserDataTag::DeferEvent:
        R_RETURN(this->OnDeferralEvent());
    default:
        UNREACHABLE();
    }
}

bool ServerManager::WaitAndProcessImpl() {
    if (auto* signaled_holder = this->WaitSignaled(); signaled_holder != nullptr) {
        R_ASSERT(this->Process(signaled_holder));
        return true;
    } else {
        return false;
    }
}

Result ServerManager::LoopProcessImpl() {
    while (!m_stop_source.stop_requested()) {
        this->WaitAndProcessImpl();
    }

    R_SUCCEED();
}

Result ServerManager::OnPortEvent(Port* server) {
    // Accept a new server session.
    auto* server_port = static_cast<Kernel::KServerPort*>(server->GetNativeHandle());
    Kernel::KServerSession* server_session = server_port->AcceptSession();
    ASSERT(server_session != nullptr);

    // Create the session manager and install the handler.
    auto manager = std::make_shared<SessionRequestManager>(m_system.Kernel(), *this);
    manager->SetSessionHandler(server->CreateHandler());

    // Create and register the new session.
    this->RegisterSession(server_session, std::move(manager));

    // Resume tracking the port.
    this->LinkToDeferredList(server);

    // We succeeded.
    R_SUCCEED();
}

Result ServerManager::OnSessionEvent(Session* session) {
    Result res = ResultSuccess;

    // Try to receive a message.
    auto* server_session = static_cast<Kernel::KServerSession*>(session->GetNativeHandle());
    res = server_session->ReceiveRequestHLE(&session->GetContext(), session->GetManager());

    // If the session has been closed, we're done.
    if (res == Kernel::ResultSessionClosed) {
        this->DestroySession(session);
        R_SUCCEED();
    }

    R_ASSERT(res);

    // Complete the sync request with deferral handling.
    R_RETURN(this->CompleteSyncRequest(session));
}

Result ServerManager::CompleteSyncRequest(Session* session) {
    Result res = ResultSuccess;
    Result service_res = ResultSuccess;

    // Mark the request as not deferred.
    session->GetContext()->SetIsDeferred(false);

    // Complete the request. We have exclusive access to this session.
    auto* server_session = static_cast<Kernel::KServerSession*>(session->GetNativeHandle());
    service_res =
        session->GetManager()->CompleteSyncRequest(server_session, *session->GetContext());

    // If we've been deferred, we're done.
    if (session->GetContext()->GetIsDeferred()) {
        // Insert into deferred session list.
        std::scoped_lock ll{m_deferred_list_mutex};
        m_deferred_sessions.push_back(session);

        // Finish.
        R_SUCCEED();
    }

    // Send the reply.
    res = server_session->SendReplyHLE();

    // If the session has been closed, we're done.
    if (res == Kernel::ResultSessionClosed || service_res == IPC::ResultSessionClosed) {
        this->DestroySession(session);
        R_SUCCEED();
    }

    R_ASSERT(res);
    R_ASSERT(service_res);

    // We succeeded, so we can process future messages on this session.
    this->LinkToDeferredList(session);

    R_SUCCEED();
}

Result ServerManager::OnDeferralEvent() {
    // Clear event before grabbing the list.
    m_deferral_event->Clear();

    // Get and clear list.
    const auto deferrals = [&] {
        std::scoped_lock lk{m_deferred_list_mutex};
        return std::move(m_deferred_sessions);
    }();

    // Relink deferral event.
    this->LinkToDeferredList(std::addressof(*m_deferral_holder));

    // For each session, try again to complete the request.
    for (auto* session : deferrals) {
        R_ASSERT(this->CompleteSyncRequest(session));
    }

    R_SUCCEED();
}

void ServerManager::DestroySession(Session* session) {
    // Unlink.
    {
        std::scoped_lock lk{m_deferred_list_mutex};
        m_sessions.erase(m_sessions.iterator_to(*session));
    }

    // Free the session.
    delete session;
}

} // namespace Service
