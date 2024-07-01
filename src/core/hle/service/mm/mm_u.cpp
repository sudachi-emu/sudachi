// SPDX-FileCopyrightText: Copyright 2018 sudachi Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/logging/log.h"
#include "core/hle/service/ipc_helpers.h"
#include "core/hle/service/mm/mm_u.h"
#include "core/hle/service/server_manager.h"
#include "core/hle/service/sm/sm.h"

namespace Service::MM {

class MM_U final : public ServiceFramework<MM_U> {
public:
    explicit MM_U(Core::System& system_) : ServiceFramework{system_, "mm:u"} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, &MM_U::InitializeOld, "InitializeOld"},
            {1, &MM_U::FinalizeOld, "FinalizeOld"},
            {2, &MM_U::SetAndWaitOld, "SetAndWaitOld"},
            {3, &MM_U::GetOld, "GetOld"},
            {4, &MM_U::Initialize, "Initialize"},
            {5, &MM_U::Finalize, "Finalize"},
            {6, &MM_U::SetAndWait, "SetAndWait"},
            {7, &MM_U::Get, "Get"},
        };
        // clang-format on

        RegisterHandlers(functions);
    }

private:
    void InitializeOld(HLERequestContext& ctx) {
        LOG_DEBUG(Service_MM, "(STUBBED) called.");

        IPC::RequestParser rp{ctx};
        module = rp.PopEnum<Module>();
        priority = rp.Pop<Priority>();
        event_clear_mode = rp.Pop<u32>();

        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(ResultSuccess);
    }

    void FinalizeOld(HLERequestContext& ctx) {
        LOG_DEBUG(Service_MM, "(STUBBED) called.");

        IPC::RequestParser rp{ctx};
        module = rp.PopEnum<Module>();

        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(ResultSuccess);
    }

    void SetAndWaitOld(HLERequestContext& ctx) {
        LOG_DEBUG(Service_MM, "(STUBBED) called.");

        IPC::RequestParser rp{ctx};
        module = rp.PopEnum<Module>();
        min = rp.Pop<Setting>();
        max = rp.Pop<Setting>();

        current = min;
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(ResultSuccess);
    }

    void GetOld(HLERequestContext& ctx) {
        LOG_DEBUG(Service_MM, "(STUBBED) called.");

        IPC::RequestParser rp{ctx};
        module = rp.PopEnum<Module>();

        IPC::ResponseBuilder rb{ctx, 3};
        rb.Push(ResultSuccess);
        rb.Push(current);
    }

    void Initialize(HLERequestContext& ctx) {
        LOG_DEBUG(Service_MM, "(STUBBED) called.");

        IPC::RequestParser rp{ctx};
        module = rp.PopEnum<Module>();
        priority = rp.Pop<Priority>();
        event_clear_mode = rp.Pop<u32>();

        IPC::ResponseBuilder rb{ctx, 3};
        rb.Push(ResultSuccess);
        rb.Push(request_id);
    }

    void Finalize(HLERequestContext& ctx) {
        LOG_DEBUG(Service_MM, "(STUBBED) called.");

        IPC::RequestParser rp{ctx};
        request_id = rp.Pop<u32>();

        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(ResultSuccess);
    }

    void SetAndWait(HLERequestContext& ctx) {
        LOG_DEBUG(Service_MM, "(STUBBED) called.");

        IPC::RequestParser rp{ctx};
        request_id = rp.Pop<u32>();
        min = rp.Pop<Setting>();
        max = rp.Pop<Setting>();

        current = min;
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(ResultSuccess);
    }

    void Get(HLERequestContext& ctx) {
        LOG_DEBUG(Service_MM, "(STUBBED) called.");

        IPC::RequestParser rp{ctx};
        request_id = rp.Pop<u32>();

        IPC::ResponseBuilder rb{ctx, 3};
        rb.Push(ResultSuccess);
        rb.Push(current);
    }

    Module module{Module::TEST};
    Priority priority{0};
    Setting min{0}, max{0}, current{0};
    u32 request_id{0}, event_clear_mode{0};
};

void LoopProcess(Core::System& system) {
    auto server_manager = std::make_unique<ServerManager>(system);

    server_manager->RegisterNamedService("mm:u", std::make_shared<MM_U>(system));
    ServerManager::RunServer(std::move(server_manager));
}

} // namespace Service::MM
