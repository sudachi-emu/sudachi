// SPDX-FileCopyrightText: Copyright 2024 sudachi Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "common/uuid.h"
#include "core/hle/service/am/am_types.h"
#include "core/hle/service/cmif_types.h"
#include "core/hle/service/service.h"

namespace FileSys {
enum class StorageId : u8;
}

namespace Kernel {
class KReadableEvent;
}

namespace Service::AM {

class AppletDataBroker;
struct Applet;
class IStorage;

struct LibraryAppletInfo {
    AppletId applet_id;
    LibraryAppletMode library_applet_mode;
};
static_assert(sizeof(LibraryAppletInfo) == 0x8, "LibraryAppletInfo has incorrect size.");

struct ErrorCode {
    u32 category;
    u32 number;
};
static_assert(sizeof(ErrorCode) == 0x8, "ErrorCode has incorrect size.");

struct ErrorContext {
    u8 type;
    INSERT_PADDING_BYTES_NOINIT(0x7);
    std::array<u8, 0x1f4> data;
    Result result;
};
static_assert(sizeof(ErrorContext) == 0x200, "ErrorContext has incorrect size.");

class ILibraryAppletSelfAccessor final : public ServiceFramework<ILibraryAppletSelfAccessor> {
public:
    explicit ILibraryAppletSelfAccessor(Core::System& system_, std::shared_ptr<Applet> applet);
    ~ILibraryAppletSelfAccessor() override;

private:
    Result PopInData(Out<SharedPointer<IStorage>> out_storage);
    Result PushOutData(SharedPointer<IStorage> storage);
    Result PopInteractiveInData(Out<SharedPointer<IStorage>> out_storage);
    Result PushInteractiveOutData(SharedPointer<IStorage> storage);
    Result GetPopInDataEvent(OutCopyHandle<Kernel::KReadableEvent> out_event);
    Result GetPopInteractiveInDataEvent(OutCopyHandle<Kernel::KReadableEvent> out_event);
    Result GetLibraryAppletInfo(Out<LibraryAppletInfo> out_library_applet_info);
    Result GetMainAppletIdentityInfo(Out<AppletIdentityInfo> out_identity_info);
    Result CanUseApplicationCore(Out<bool> out_can_use_application_core);
    Result GetMainAppletApplicationControlProperty(
        OutLargeData<std::array<u8, 0x4000>, BufferAttr_HipcMapAlias> out_nacp);
    Result GetMainAppletStorageId(Out<FileSys::StorageId> out_storage_id);
    Result ExitProcessAndReturn();
    Result GetCallerAppletIdentityInfo(Out<AppletIdentityInfo> out_identity_info);
    Result GetCallerAppletIdentityInfoStack(
        Out<s32> out_count,
        OutArray<AppletIdentityInfo, BufferAttr_HipcMapAlias> out_identity_info);
    Result GetDesirableKeyboardLayout(Out<u32> out_desirable_layout);
    Result UnpopInData(SharedPointer<IStorage> storage);
    Result ReportVisibleError(ErrorCode error_code);
    Result ReportVisibleErrorWithErrorContext(
        ErrorCode error_code, InLargeData<ErrorContext, BufferAttr_HipcMapAlias> error_context);
    Result GetMainAppletApplicationDesiredLanguage(Out<u64> out_desired_language);
    Result GetCurrentApplicationId(Out<u64> out_application_id);
    Result GetMainAppletAvailableUsers(Out<bool> out_can_select_any_user, Out<s32> out_users_count,
                                       OutArray<Common::UUID, BufferAttr_HipcMapAlias> out_users);
    Result ShouldSetGpuTimeSliceManually(Out<bool> out_should_set_gpu_time_slice_manually);
    Result Cmd160(Out<u64> out_unknown0);

    const std::shared_ptr<Applet> m_applet;
    const std::shared_ptr<AppletDataBroker> m_broker;
};

} // namespace Service::AM
