// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/core.h"
#include "core/hle/kernel/k_light_client_session.h"
#include "core/hle/kernel/k_light_server_session.h"
#include "core/hle/kernel/k_process.h"
#include "core/hle/kernel/k_thread.h"
#include "core/hle/kernel/svc.h"
#include "core/hle/kernel/svc_results.h"

namespace Kernel::Svc {

Result SendSyncRequestLight(Core::System& system, Handle session_handle, u32* args) {
    // Get the light client session from its handle.
    KScopedAutoObject session = GetCurrentProcess(system.Kernel())
                                    .GetHandleTable()
                                    .GetObject<KLightClientSession>(session_handle);
    R_UNLESS(session.IsNotNull(), ResultInvalidHandle);

    // Send the request.
    R_TRY(session->SendSyncRequest(args));

    R_SUCCEED();
}

Result ReplyAndReceiveLight(Core::System& system, Handle session_handle, u32* args) {
    // Get the light server session from its handle.
    KScopedAutoObject session = GetCurrentProcess(system.Kernel())
                                    .GetHandleTable()
                                    .GetObject<KLightServerSession>(session_handle);
    R_UNLESS(session.IsNotNull(), ResultInvalidHandle);

    // Handle the request.
    R_TRY(session->ReplyAndReceive(args));

    R_SUCCEED();
}

Result SendSyncRequestLight64(Core::System& system, Handle session_handle, u32* args) {
    R_RETURN(SendSyncRequestLight(system, session_handle, args));
}

Result ReplyAndReceiveLight64(Core::System& system, Handle session_handle, u32* args) {
    R_RETURN(ReplyAndReceiveLight(system, session_handle, args));
}

Result SendSyncRequestLight64From32(Core::System& system, Handle session_handle, u32* args) {
    R_RETURN(SendSyncRequestLight(system, session_handle, args));
}

Result ReplyAndReceiveLight64From32(Core::System& system, Handle session_handle, u32* args) {
    R_RETURN(ReplyAndReceiveLight(system, session_handle, args));
}

// Custom ABI implementation for light IPC.

template <typename F>
static void SvcWrap_LightIpc(Core::System& system, std::span<uint64_t, 8> args, F&& cb) {
    std::array<u32, 7> ipc_args{};

    Handle session_handle = static_cast<Handle>(args[0]);
    for (int i = 0; i < 7; i++) {
        ipc_args[i] = static_cast<u32>(args[i + 1]);
    }

    Result ret = cb(system, session_handle, ipc_args.data());

    args[0] = ret.raw;
    for (int i = 0; i < 7; i++) {
        args[i + 1] = ipc_args[i];
    }
}

void SvcWrap_SendSyncRequestLight64(Core::System& system, std::span<uint64_t, 8> args) {
    SvcWrap_LightIpc(system, args, SendSyncRequestLight64);
}

void SvcWrap_ReplyAndReceiveLight64(Core::System& system, std::span<uint64_t, 8> args) {
    SvcWrap_LightIpc(system, args, ReplyAndReceiveLight64);
}

void SvcWrap_SendSyncRequestLight64From32(Core::System& system, std::span<uint64_t, 8> args) {
    SvcWrap_LightIpc(system, args, SendSyncRequestLight64From32);
}

void SvcWrap_ReplyAndReceiveLight64From32(Core::System& system, std::span<uint64_t, 8> args) {
    SvcWrap_LightIpc(system, args, ReplyAndReceiveLight64From32);
}

} // namespace Kernel::Svc
