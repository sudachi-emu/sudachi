// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/scope_exit.h"
#include "core/core.h"
#include "core/hle/kernel/k_client_port.h"
#include "core/hle/kernel/k_client_session.h"
#include "core/hle/kernel/k_light_client_session.h"
#include "core/hle/kernel/k_object_name.h"
#include "core/hle/kernel/k_port.h"
#include "core/hle/kernel/k_process.h"
#include "core/hle/kernel/svc.h"

namespace Kernel::Svc {

Result ConnectToNamedPort(Core::System& system, Handle* out, u64 user_name) {
    // Copy the provided name from user memory to kernel memory.
    auto string_name =
        GetCurrentMemory(system.Kernel()).ReadCString(user_name, KObjectName::NameLengthMax);

    std::array<char, KObjectName::NameLengthMax> name{};
    std::strncpy(name.data(), string_name.c_str(), KObjectName::NameLengthMax - 1);

    // Validate that the name is valid.
    R_UNLESS(name[sizeof(name) - 1] == '\x00', ResultOutOfRange);

    // Get the current handle table.
    auto& handle_table = GetCurrentProcess(system.Kernel()).GetHandleTable();

    // Find the client port.
    auto port = KObjectName::Find<KClientPort>(system.Kernel(), name.data());
    R_UNLESS(port.IsNotNull(), ResultNotFound);

    // Reserve a handle for the port.
    // NOTE: Nintendo really does write directly to the output handle here.
    R_TRY(handle_table.Reserve(out));
    ON_RESULT_FAILURE {
        handle_table.Unreserve(*out);
    };

    // Create a session.
    KClientSession* session;
    R_TRY(port->CreateSession(std::addressof(session)));

    // Register the session in the table, close the extra reference.
    handle_table.Register(*out, session);
    session->Close();

    // We succeeded.
    R_SUCCEED();
}

Result CreatePort(Core::System& system, Handle* out_server, Handle* out_client,
                  int32_t max_sessions, bool is_light, uint64_t name) {
    auto& kernel = system.Kernel();

    // Ensure max sessions is valid.
    R_UNLESS(max_sessions > 0, ResultOutOfRange);

    // Get the current handle table.
    auto& handle_table = GetCurrentProcess(kernel).GetHandleTable();

    // Create a new port.
    KPort* port = KPort::Create(kernel);
    R_UNLESS(port != nullptr, ResultOutOfResource);

    // Initialize the port.
    port->Initialize(max_sessions, is_light, name);

    // Ensure that we clean up the port (and its only references are handle table) on function end.
    SCOPE_EXIT {
        port->GetServerPort().Close();
        port->GetClientPort().Close();
    };

    // Register the port.
    KPort::Register(kernel, port);

    // Add the client to the handle table.
    R_TRY(handle_table.Add(out_client, std::addressof(port->GetClientPort())));

    // Ensure that we maintain a clean handle state on exit.
    ON_RESULT_FAILURE {
        handle_table.Remove(*out_client);
    };

    // Add the server to the handle table.
    R_RETURN(handle_table.Add(out_server, std::addressof(port->GetServerPort())));
}

Result ConnectToPort(Core::System& system, Handle* out, Handle port) {
    // Get the current handle table.
    auto& handle_table = GetCurrentProcess(system.Kernel()).GetHandleTable();

    // Get the client port.
    KScopedAutoObject client_port = handle_table.GetObject<KClientPort>(port);
    R_UNLESS(client_port.IsNotNull(), ResultInvalidHandle);

    // Reserve a handle for the port.
    // NOTE: Nintendo really does write directly to the output handle here.
    R_TRY(handle_table.Reserve(out));
    ON_RESULT_FAILURE {
        handle_table.Unreserve(*out);
    };

    // Create the session.
    KAutoObject* session;
    if (client_port->IsLight()) {
        R_TRY(client_port->CreateLightSession(
            reinterpret_cast<KLightClientSession**>(std::addressof(session))));
    } else {
        R_TRY(client_port->CreateSession(
            reinterpret_cast<KClientSession**>(std::addressof(session))));
    }

    // Register the session.
    handle_table.Register(*out, session);
    session->Close();

    // We succeeded.
    R_SUCCEED();
}

Result ManageNamedPort(Core::System& system, Handle* out_server_handle, uint64_t user_name,
                       int32_t max_sessions) {
    // Copy the provided name from user memory to kernel memory.
    auto string_name =
        GetCurrentMemory(system.Kernel()).ReadCString(user_name, KObjectName::NameLengthMax);

    // Copy the provided name from user memory to kernel memory.
    std::array<char, KObjectName::NameLengthMax> name{};
    std::strncpy(name.data(), string_name.c_str(), KObjectName::NameLengthMax - 1);

    // Validate that sessions and name are valid.
    R_UNLESS(max_sessions >= 0, ResultOutOfRange);
    R_UNLESS(name[sizeof(name) - 1] == '\x00', ResultOutOfRange);

    if (max_sessions > 0) {
        // Get the current handle table.
        auto& handle_table = GetCurrentProcess(system.Kernel()).GetHandleTable();

        // Create a new port.
        KPort* port = KPort::Create(system.Kernel());
        R_UNLESS(port != nullptr, ResultOutOfResource);

        // Initialize the new port.
        port->Initialize(max_sessions, false, 0);

        // Register the port.
        KPort::Register(system.Kernel(), port);

        // Ensure that our only reference to the port is in the handle table when we're done.
        SCOPE_EXIT {
            port->GetClientPort().Close();
            port->GetServerPort().Close();
        };

        // Register the handle in the table.
        R_TRY(handle_table.Add(out_server_handle, std::addressof(port->GetServerPort())));
        ON_RESULT_FAILURE {
            handle_table.Remove(*out_server_handle);
        };

        // Create a new object name.
        R_TRY(KObjectName::NewFromName(system.Kernel(), std::addressof(port->GetClientPort()),
                                       name.data()));
    } else /* if (max_sessions == 0) */ {
        // Ensure that this else case is correct.
        ASSERT(max_sessions == 0);

        // If we're closing, there's no server handle.
        *out_server_handle = InvalidHandle;

        // Delete the object.
        R_TRY(KObjectName::Delete<KClientPort>(system.Kernel(), name.data()));
    }

    R_SUCCEED();
}

Result ConnectToNamedPort64(Core::System& system, Handle* out_handle, uint64_t name) {
    R_RETURN(ConnectToNamedPort(system, out_handle, name));
}

Result CreatePort64(Core::System& system, Handle* out_server_handle, Handle* out_client_handle,
                    int32_t max_sessions, bool is_light, uint64_t name) {
    R_RETURN(
        CreatePort(system, out_server_handle, out_client_handle, max_sessions, is_light, name));
}

Result ManageNamedPort64(Core::System& system, Handle* out_server_handle, uint64_t name,
                         int32_t max_sessions) {
    R_RETURN(ManageNamedPort(system, out_server_handle, name, max_sessions));
}

Result ConnectToPort64(Core::System& system, Handle* out_handle, Handle port) {
    R_RETURN(ConnectToPort(system, out_handle, port));
}

Result ConnectToNamedPort64From32(Core::System& system, Handle* out_handle, uint32_t name) {
    R_RETURN(ConnectToNamedPort(system, out_handle, name));
}

Result CreatePort64From32(Core::System& system, Handle* out_server_handle,
                          Handle* out_client_handle, int32_t max_sessions, bool is_light,
                          uint32_t name) {
    R_RETURN(
        CreatePort(system, out_server_handle, out_client_handle, max_sessions, is_light, name));
}

Result ManageNamedPort64From32(Core::System& system, Handle* out_server_handle, uint32_t name,
                               int32_t max_sessions) {
    R_RETURN(ManageNamedPort(system, out_server_handle, name, max_sessions));
}

Result ConnectToPort64From32(Core::System& system, Handle* out_handle, Handle port) {
    R_RETURN(ConnectToPort(system, out_handle, port));
}

} // namespace Kernel::Svc
