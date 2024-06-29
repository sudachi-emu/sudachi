// SPDX-FileCopyrightText: Copyright 2024 sudachi Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/hle/service/am/frontend/applets.h"
#include "core/hle/service/am/library_applet_accessor.h"
#include "core/hle/service/am/process_winding_controller.h"
#include "core/hle/service/am/service/storage.h"
#include "core/hle/service/ipc_helpers.h"

namespace Service::AM {

IProcessWindingController::IProcessWindingController(Core::System& system_,
                                                     std::shared_ptr<Applet> applet_)
    : ServiceFramework{system_, "IProcessWindingController"}, applet{std::move(applet_)} {
    // clang-format off
    static const FunctionInfo functions[] = {
        {0, &IProcessWindingController::GetLaunchReason, "GetLaunchReason"},
        {11, &IProcessWindingController::OpenCallingLibraryApplet, "OpenCallingLibraryApplet"},
        {21, &IProcessWindingController::PushContext, "PushContext"},
        {22, nullptr, "PopContext"},
        {23, nullptr, "CancelWindingReservation"},
        {30, &IProcessWindingController::WindAndDoReserved, "WindAndDoReserved"},
        {40, &IProcessWindingController::ReserveToStartAndWaitAndUnwindThis, "ReserveToStartAndWaitAndUnwindThis"},
        {41, nullptr, "ReserveToStartAndWait"},
    };
    // clang-format on

    RegisterHandlers(functions);
}

IProcessWindingController::~IProcessWindingController() = default;

void IProcessWindingController::GetLaunchReason(HLERequestContext& ctx) {
    LOG_WARNING(Service_AM, "(STUBBED) called");

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.PushRaw(applet->launch_reason);
}

void IProcessWindingController::OpenCallingLibraryApplet(HLERequestContext& ctx) {
    const auto caller_applet = applet->caller_applet.lock();
    if (caller_applet == nullptr) {
        LOG_ERROR(Service_AM, "No calling applet available");

        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(ResultUnknown);
        return;
    }

    IPC::ResponseBuilder rb{ctx, 2, 0, 1};
    rb.Push(ResultSuccess);
    rb.PushIpcInterface<ILibraryAppletAccessor>(system, applet->caller_applet_broker,
                                                caller_applet);
}

void IProcessWindingController::PushContext(HLERequestContext& ctx) {
    LOG_WARNING(Service_AM, "(STUBBED) called");

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void IProcessWindingController::ReserveToStartAndWaitAndUnwindThis(HLERequestContext& ctx) {
    LOG_WARNING(Service_AM, "(STUBBED) called");

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void IProcessWindingController::WindAndDoReserved(HLERequestContext& ctx) {
    LOG_WARNING(Service_AM, "(STUBBED) called");

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

} // namespace Service::AM
