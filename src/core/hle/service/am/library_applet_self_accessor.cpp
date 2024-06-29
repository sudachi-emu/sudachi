// SPDX-FileCopyrightText: Copyright 2024 sudachi Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/scope_exit.h"
#include "core/core_timing.h"
#include "core/file_sys/control_metadata.h"
#include "core/file_sys/patch_manager.h"
#include "core/file_sys/registered_cache.h"
#include "core/hle/service/acc/profile_manager.h"
#include "core/hle/service/am/am_results.h"
#include "core/hle/service/am/applet_data_broker.h"
#include "core/hle/service/am/applet_manager.h"
#include "core/hle/service/am/frontend/applet_cabinet.h"
#include "core/hle/service/am/frontend/applet_controller.h"
#include "core/hle/service/am/frontend/applet_mii_edit_types.h"
#include "core/hle/service/am/frontend/applet_software_keyboard_types.h"
#include "core/hle/service/am/frontend/applets.h"
#include "core/hle/service/am/library_applet_self_accessor.h"
#include "core/hle/service/am/storage.h"
#include "core/hle/service/ipc_helpers.h"
#include "core/hle/service/ns/ns.h"
#include "core/hle/service/sm/sm.h"
#include "hid_core/hid_types.h"

namespace Service::AM {

namespace {

AppletIdentityInfo GetCallerIdentity(std::shared_ptr<Applet> applet) {
    if (const auto caller_applet = applet->caller_applet.lock(); caller_applet) {
        // TODO: is this actually the application ID?
        return {
            .applet_id = caller_applet->applet_id,
            .application_id = caller_applet->program_id,
        };
    } else {
        return {
            .applet_id = AppletId::QLaunch,
            .application_id = 0x0100000000001000ull,
        };
    }
}

} // namespace

ILibraryAppletSelfAccessor::ILibraryAppletSelfAccessor(Core::System& system_,
                                                       std::shared_ptr<Applet> applet_)
    : ServiceFramework{system_, "ILibraryAppletSelfAccessor"}, applet{std::move(applet_)},
      broker{applet->caller_applet_broker} {
    // clang-format off
    static const FunctionInfo functions[] = {
        {0, &ILibraryAppletSelfAccessor::PopInData, "PopInData"},
        {1, &ILibraryAppletSelfAccessor::PushOutData, "PushOutData"},
        {2, &ILibraryAppletSelfAccessor::PopInteractiveInData, "PopInteractiveInData"},
        {3, &ILibraryAppletSelfAccessor::PushInteractiveOutData, "PushInteractiveOutData"},
        {5, &ILibraryAppletSelfAccessor::GetPopInDataEvent, "GetPopInDataEvent"},
        {6, &ILibraryAppletSelfAccessor::GetPopInteractiveInDataEvent, "GetPopInteractiveInDataEvent"},
        {10, &ILibraryAppletSelfAccessor::ExitProcessAndReturn, "ExitProcessAndReturn"},
        {11, &ILibraryAppletSelfAccessor::GetLibraryAppletInfo, "GetLibraryAppletInfo"},
        {12, &ILibraryAppletSelfAccessor::GetMainAppletIdentityInfo, "GetMainAppletIdentityInfo"},
        {13, &ILibraryAppletSelfAccessor::CanUseApplicationCore, "CanUseApplicationCore"},
        {14, &ILibraryAppletSelfAccessor::GetCallerAppletIdentityInfo, "GetCallerAppletIdentityInfo"},
        {15, nullptr, "GetMainAppletApplicationControlProperty"},
        {16, nullptr, "GetMainAppletStorageId"},
        {17, nullptr, "GetCallerAppletIdentityInfoStack"},
        {18, nullptr, "GetNextReturnDestinationAppletIdentityInfo"},
        {19, &ILibraryAppletSelfAccessor::GetDesirableKeyboardLayout, "GetDesirableKeyboardLayout"},
        {20, nullptr, "PopExtraStorage"},
        {25, nullptr, "GetPopExtraStorageEvent"},
        {30, &ILibraryAppletSelfAccessor::UnpopInData, "UnpopInData"},
        {31, nullptr, "UnpopExtraStorage"},
        {40, nullptr, "GetIndirectLayerProducerHandle"},
        {50, nullptr, "ReportVisibleError"},
        {51, nullptr, "ReportVisibleErrorWithErrorContext"},
        {60, &ILibraryAppletSelfAccessor::GetMainAppletApplicationDesiredLanguage, "GetMainAppletApplicationDesiredLanguage"},
        {70, &ILibraryAppletSelfAccessor::GetCurrentApplicationId, "GetCurrentApplicationId"},
        {80, nullptr, "RequestExitToSelf"},
        {90, nullptr, "CreateApplicationAndPushAndRequestToLaunch"},
        {100, nullptr, "CreateGameMovieTrimmer"},
        {101, nullptr, "ReserveResourceForMovieOperation"},
        {102, nullptr, "UnreserveResourceForMovieOperation"},
        {110, &ILibraryAppletSelfAccessor::GetMainAppletAvailableUsers, "GetMainAppletAvailableUsers"},
        {120, nullptr, "GetLaunchStorageInfoForDebug"},
        {130, nullptr, "GetGpuErrorDetectedSystemEvent"},
        {140, nullptr, "SetApplicationMemoryReservation"},
        {150, &ILibraryAppletSelfAccessor::ShouldSetGpuTimeSliceManually, "ShouldSetGpuTimeSliceManually"},
        {160, &ILibraryAppletSelfAccessor::Cmd160, "Cmd160"},
    };
    // clang-format on
    RegisterHandlers(functions);
}

ILibraryAppletSelfAccessor::~ILibraryAppletSelfAccessor() = default;

void ILibraryAppletSelfAccessor::PopInData(HLERequestContext& ctx) {
    LOG_INFO(Service_AM, "called");

    std::shared_ptr<IStorage> data;
    const auto res = broker->GetInData().Pop(&data);

    if (res.IsSuccess()) {
        IPC::ResponseBuilder rb{ctx, 2, 0, 1};
        rb.Push(res);
        rb.PushIpcInterface(std::move(data));
    } else {
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(res);
    }
}

void ILibraryAppletSelfAccessor::PushOutData(HLERequestContext& ctx) {
    LOG_INFO(Service_AM, "called");

    IPC::RequestParser rp{ctx};
    broker->GetOutData().Push(rp.PopIpcInterface<IStorage>().lock());

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void ILibraryAppletSelfAccessor::PopInteractiveInData(HLERequestContext& ctx) {
    LOG_INFO(Service_AM, "called");

    std::shared_ptr<IStorage> data;
    const auto res = broker->GetInteractiveInData().Pop(&data);

    if (res.IsSuccess()) {
        IPC::ResponseBuilder rb{ctx, 2, 0, 1};
        rb.Push(res);
        rb.PushIpcInterface(std::move(data));
    } else {
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(res);
    }
}

void ILibraryAppletSelfAccessor::PushInteractiveOutData(HLERequestContext& ctx) {
    LOG_INFO(Service_AM, "called");

    IPC::RequestParser rp{ctx};
    broker->GetInteractiveOutData().Push(rp.PopIpcInterface<IStorage>().lock());

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void ILibraryAppletSelfAccessor::GetPopInDataEvent(HLERequestContext& ctx) {
    LOG_INFO(Service_AM, "called");

    IPC::ResponseBuilder rb{ctx, 2, 1};
    rb.Push(ResultSuccess);
    rb.PushCopyObjects(broker->GetInData().GetEvent());
}

void ILibraryAppletSelfAccessor::GetPopInteractiveInDataEvent(HLERequestContext& ctx) {
    LOG_INFO(Service_AM, "called");

    IPC::ResponseBuilder rb{ctx, 2, 1};
    rb.Push(ResultSuccess);
    rb.PushCopyObjects(broker->GetInteractiveInData().GetEvent());
}

void ILibraryAppletSelfAccessor::ExitProcessAndReturn(HLERequestContext& ctx) {
    LOG_INFO(Service_AM, "called");

    system.GetAppletManager().TerminateAndRemoveApplet(applet->aruid);
    broker->SignalCompletion();

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void ILibraryAppletSelfAccessor::GetLibraryAppletInfo(HLERequestContext& ctx) {
    struct LibraryAppletInfo {
        AppletId applet_id;
        LibraryAppletMode library_applet_mode;
    };

    LOG_WARNING(Service_AM, "(STUBBED) called");

    const LibraryAppletInfo applet_info{
        .applet_id = applet->applet_id,
        .library_applet_mode = applet->library_applet_mode,
    };

    IPC::ResponseBuilder rb{ctx, 4};
    rb.Push(ResultSuccess);
    rb.PushRaw(applet_info);
}

void ILibraryAppletSelfAccessor::GetMainAppletIdentityInfo(HLERequestContext& ctx) {
    LOG_WARNING(Service_AM, "(STUBBED) called");

    const AppletIdentityInfo applet_info{
        .applet_id = AppletId::QLaunch,
        .application_id = 0x0100000000001000ull,
    };

    IPC::ResponseBuilder rb{ctx, 6};
    rb.Push(ResultSuccess);
    rb.PushRaw(applet_info);
}

void ILibraryAppletSelfAccessor::CanUseApplicationCore(HLERequestContext& ctx) {
    LOG_WARNING(Service_AM, "(STUBBED) called");

    // TODO: This appears to read the NPDM from state and check the core mask of the applet.
    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.Push<u8>(0);
}

void ILibraryAppletSelfAccessor::GetCallerAppletIdentityInfo(HLERequestContext& ctx) {
    LOG_WARNING(Service_AM, "(STUBBED) called");

    IPC::ResponseBuilder rb{ctx, 6};
    rb.Push(ResultSuccess);
    rb.PushRaw(GetCallerIdentity(applet));
}

void ILibraryAppletSelfAccessor::GetDesirableKeyboardLayout(HLERequestContext& ctx) {
    LOG_WARNING(Service_AM, "(STUBBED) called");

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.Push<u32>(0);
}

void ILibraryAppletSelfAccessor::UnpopInData(HLERequestContext& ctx) {
    LOG_WARNING(Service_AM, "(STUBBED) called");

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void ILibraryAppletSelfAccessor::GetMainAppletApplicationDesiredLanguage(HLERequestContext& ctx) {
    // FIXME: this is copied from IApplicationFunctions::GetDesiredLanguage
    auto identity = GetCallerIdentity(applet);

    // TODO(bunnei): This should be configurable
    LOG_DEBUG(Service_AM, "called");

    // Get supported languages from NACP, if possible
    // Default to 0 (all languages supported)
    u32 supported_languages = 0;

    const auto res = [this, identity] {
        const FileSys::PatchManager pm{identity.application_id, system.GetFileSystemController(),
                                       system.GetContentProvider()};
        auto metadata = pm.GetControlMetadata();
        if (metadata.first != nullptr) {
            return metadata;
        }

        const FileSys::PatchManager pm_update{FileSys::GetUpdateTitleID(identity.application_id),
                                              system.GetFileSystemController(),
                                              system.GetContentProvider()};
        return pm_update.GetControlMetadata();
    }();

    if (res.first != nullptr) {
        supported_languages = res.first->GetSupportedLanguages();
    }

    // Call IApplicationManagerInterface implementation.
    auto& service_manager = system.ServiceManager();
    auto ns_am2 = service_manager.GetService<NS::NS>("ns:am2");
    auto app_man = ns_am2->GetApplicationManagerInterface();

    // Get desired application language
    u8 desired_language{};
    const auto res_lang =
        app_man->GetApplicationDesiredLanguage(&desired_language, supported_languages);
    if (res_lang != ResultSuccess) {
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(res_lang);
        return;
    }

    // Convert to settings language code.
    u64 language_code{};
    const auto res_code =
        app_man->ConvertApplicationLanguageToLanguageCode(&language_code, desired_language);
    if (res_code != ResultSuccess) {
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(res_code);
        return;
    }

    LOG_DEBUG(Service_AM, "got desired_language={:016X}", language_code);

    IPC::ResponseBuilder rb{ctx, 4};
    rb.Push(ResultSuccess);
    rb.Push(language_code);
}

void ILibraryAppletSelfAccessor::GetCurrentApplicationId(HLERequestContext& ctx) {
    LOG_WARNING(Service_AM, "(STUBBED) called");

    u64 application_id = 0;
    if (auto caller_applet = applet->caller_applet.lock(); caller_applet) {
        application_id = caller_applet->program_id;
    }

    IPC::ResponseBuilder rb{ctx, 4};
    rb.Push(ResultSuccess);
    rb.Push(application_id);
}

void ILibraryAppletSelfAccessor::GetMainAppletAvailableUsers(HLERequestContext& ctx) {
    const Service::Account::ProfileManager manager{};
    bool is_empty{true};
    s32 user_count{-1};

    LOG_INFO(Service_AM, "called");

    if (manager.GetUserCount() > 0) {
        is_empty = false;
        user_count = static_cast<s32>(manager.GetUserCount());
        ctx.WriteBuffer(manager.GetAllUsers());
    }

    IPC::ResponseBuilder rb{ctx, 4};
    rb.Push(ResultSuccess);
    rb.Push<u8>(is_empty);
    rb.Push(user_count);
}

void ILibraryAppletSelfAccessor::ShouldSetGpuTimeSliceManually(HLERequestContext& ctx) {
    LOG_WARNING(Service_AM, "(STUBBED) called");

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.Push<u8>(0);
}

void ILibraryAppletSelfAccessor::Cmd160(HLERequestContext& ctx) {
    LOG_WARNING(Service_AM, "(STUBBED) called");

    IPC::ResponseBuilder rb{ctx, 4};
    rb.Push(ResultSuccess);
    rb.Push<u64>(0);
}

} // namespace Service::AM
