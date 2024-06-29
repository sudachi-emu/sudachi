// SPDX-FileCopyrightText: Copyright 2018 sudachi Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <queue>
#include "common/logging/log.h"
#include "common/uuid.h"
#include "core/core.h"
#include "core/hle/kernel/k_event.h"
#include "core/hle/service/acc/errors.h"
#include "core/hle/service/friend/friend.h"
#include "core/hle/service/friend/friend_interface.h"
#include "core/hle/service/ipc_helpers.h"
#include "core/hle/service/kernel_helpers.h"
#include "core/hle/service/server_manager.h"

namespace Service::Friend {

class IFriendService final : public ServiceFramework<IFriendService> {
public:
    explicit IFriendService(Core::System& system_)
        : ServiceFramework{system_, "IFriendService"}, service_context{system, "IFriendService"} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, &IFriendService::GetCompletionEvent, "GetCompletionEvent"},
            {1, nullptr, "Cancel"},
            {10100, nullptr, "GetFriendListIds"},
            {10101, &IFriendService::GetFriendList, "GetFriendList"},
            {10102, nullptr, "UpdateFriendInfo"},
            {10110, nullptr, "GetFriendProfileImage"},
            {10111, nullptr, "GetFriendProfileImageWithImageSize"}, // 18.0.0+
            {10120, &IFriendService::CheckFriendListAvailability, "CheckFriendListAvailability"},
            {10121, nullptr, "EnsureFriendListAvailable"},
            {10200, nullptr, "SendFriendRequestForApplication"},
            {10211, nullptr, "AddFacedFriendRequestForApplication"},
            {10400, &IFriendService::GetBlockedUserListIds, "GetBlockedUserListIds"},
            {10420, &IFriendService::CheckBlockedUserListAvailability, "CheckBlockedUserListAvailability"},
            {10421, nullptr, "EnsureBlockedUserListAvailable"},
            {10500, nullptr, "GetProfileList"},
            {10501, nullptr, "GetProfileListV2"}, // 18.0.0+
            {10600, nullptr, "DeclareOpenOnlinePlaySession"},
            {10601, &IFriendService::DeclareCloseOnlinePlaySession, "DeclareCloseOnlinePlaySession"},
            {10610, &IFriendService::UpdateUserPresence, "UpdateUserPresence"},
            {10700, &IFriendService::GetPlayHistoryRegistrationKey, "GetPlayHistoryRegistrationKey"},
            {10701, nullptr, "GetPlayHistoryRegistrationKeyWithNetworkServiceAccountId"},
            {10702, nullptr, "AddPlayHistory"},
            {11000, nullptr, "GetProfileImageUrl"},
            {11001, nullptr, "GetProfileImageUrlV2"}, // 18.0.0+
            {20100, &IFriendService::GetFriendCount, "GetFriendCount"},
            {20101, &IFriendService::GetNewlyFriendCount, "GetNewlyFriendCount"},
            {20102, nullptr, "GetFriendDetailedInfo"},
            {20103, nullptr, "SyncFriendList"},
            {20104, nullptr, "RequestSyncFriendList"},
            {20110, nullptr, "LoadFriendSetting"},
            {20200, &IFriendService::GetReceivedFriendRequestCount, "GetReceivedFriendRequestCount"},
            {20201, nullptr, "GetFriendRequestList"},
            {20300, nullptr, "GetFriendCandidateList"},
            {20301, nullptr, "GetNintendoNetworkIdInfo"},
            {20302, nullptr, "GetSnsAccountLinkage"},
            {20303, nullptr, "GetSnsAccountProfile"},
            {20304, nullptr, "GetSnsAccountFriendList"},
            {20400, nullptr, "GetBlockedUserList"},
            {20401, nullptr, "SyncBlockedUserList"},
            {20500, nullptr, "GetProfileExtraList"},
            {20501, nullptr, "GetRelationship"},
            {20600, &IFriendService::GetUserPresenceView, "GetUserPresenceView"},
            {20700, nullptr, "GetPlayHistoryList"},
            {20701, &IFriendService::GetPlayHistoryStatistics, "GetPlayHistoryStatistics"},
            {20800, nullptr, "LoadUserSetting"},
            {20801, nullptr, "SyncUserSetting"},
            {20900, nullptr, "RequestListSummaryOverlayNotification"},
            {21000, nullptr, "GetExternalApplicationCatalog"},
            {22000, nullptr, "GetReceivedFriendInvitationList"},
            {22001, nullptr, "GetReceivedFriendInvitationDetailedInfo"},
            {22010, &IFriendService::GetReceivedFriendInvitationCountCache, "GetReceivedFriendInvitationCountCache"},
            {30100, nullptr, "DropFriendNewlyFlags"},
            {30101, nullptr, "DeleteFriend"},
            {30110, nullptr, "DropFriendNewlyFlag"},
            {30120, nullptr, "ChangeFriendFavoriteFlag"},
            {30121, nullptr, "ChangeFriendOnlineNotificationFlag"},
            {30200, nullptr, "SendFriendRequest"},
            {30201, nullptr, "SendFriendRequestWithApplicationInfo"},
            {30202, nullptr, "CancelFriendRequest"},
            {30203, nullptr, "AcceptFriendRequest"},
            {30204, nullptr, "RejectFriendRequest"},
            {30205, nullptr, "ReadFriendRequest"},
            {30210, nullptr, "GetFacedFriendRequestRegistrationKey"},
            {30211, nullptr, "AddFacedFriendRequest"},
            {30212, nullptr, "CancelFacedFriendRequest"},
            {30213, nullptr, "GetFacedFriendRequestProfileImage"},
            {30214, nullptr, "GetFacedFriendRequestProfileImageFromPath"},
            {30215, nullptr, "SendFriendRequestWithExternalApplicationCatalogId"},
            {30216, nullptr, "ResendFacedFriendRequest"},
            {30217, nullptr, "SendFriendRequestWithNintendoNetworkIdInfo"},
            {30300, nullptr, "GetSnsAccountLinkPageUrl"},
            {30301, nullptr, "UnlinkSnsAccount"},
            {30400, nullptr, "BlockUser"},
            {30401, nullptr, "BlockUserWithApplicationInfo"},
            {30402, nullptr, "UnblockUser"},
            {30500, nullptr, "GetProfileExtraFromFriendCode"},
            {30700, nullptr, "DeletePlayHistory"},
            {30810, nullptr, "ChangePresencePermission"},
            {30811, nullptr, "ChangeFriendRequestReception"},
            {30812, nullptr, "ChangePlayLogPermission"},
            {30820, nullptr, "IssueFriendCode"},
            {30830, nullptr, "ClearPlayLog"},
            {30900, nullptr, "SendFriendInvitation"},
            {30910, nullptr, "ReadFriendInvitation"},
            {30911, nullptr, "ReadAllFriendInvitations"},
            {40100, nullptr, "DeleteFriendListCache"},
            {40400, nullptr, "DeleteBlockedUserListCache"},
            {49900, nullptr, "DeleteNetworkServiceAccountCache"},
        };
        // clang-format on

        RegisterHandlers(functions);

        completion_event = service_context.CreateEvent("IFriendService:CompletionEvent");
    }

    ~IFriendService() override {
        service_context.CloseEvent(completion_event);
    }

private:
    enum class PresenceFilter : u32 {
        None = 0,
        Online = 1,
        OnlinePlay = 2,
        OnlineOrOnlinePlay = 3,
    };

    struct SizedFriendFilter {
        PresenceFilter presence;
        u8 is_favorite;
        u8 same_app;
        u8 same_app_played;
        u8 arbitrary_app_played;
        u64 group_id;
    };
    static_assert(sizeof(SizedFriendFilter) == 0x10, "SizedFriendFilter is an invalid size");

    void GetCompletionEvent(HLERequestContext& ctx) {
        LOG_DEBUG(Service_Friend, "called");

        auto& readable_event = completion_event->GetReadableEvent();

        IPC::ResponseBuilder rb{ctx, 2, 1};
        rb.Push(readable_event.Signal());
        rb.PushCopyObjects(readable_event);
    }

    void GetFriendList(HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx};
        const auto friend_offset = rp.Pop<u32>();
        const auto uuid = rp.PopRaw<Common::UUID>();
        [[maybe_unused]] const auto filter = rp.PopRaw<SizedFriendFilter>();
        const auto pid = rp.Pop<u64>();
        LOG_WARNING(Service_Friend, "(STUBBED) called, offset={}, uuid=0x{}, pid={}", friend_offset,
                    uuid.RawString(), pid);

        IPC::ResponseBuilder rb{ctx, 3};
        rb.Push(ResultSuccess);

        rb.Push<u32>(0); // Friend count
        // TODO(ogniK): Return a buffer of u64s which are the "NetworkServiceAccountId"
    }

    void CheckFriendListAvailability(HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx};
        const auto uuid{rp.PopRaw<Common::UUID>()};

        LOG_WARNING(Service_Friend, "(STUBBED) called, uuid=0x{}", uuid.RawString());

        IPC::ResponseBuilder rb{ctx, 3};
        rb.Push(ResultSuccess);
        rb.Push(true);
    }

    void GetBlockedUserListIds(HLERequestContext& ctx) {
        // This is safe to stub, as there should be no adverse consequences from reporting no
        // blocked users.
        LOG_WARNING(Service_Friend, "(STUBBED) called");
        IPC::ResponseBuilder rb{ctx, 3};
        rb.Push(ResultSuccess);
        rb.Push<u32>(0); // Indicates there are no blocked users
    }

    void CheckBlockedUserListAvailability(HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx};
        const auto uuid{rp.PopRaw<Common::UUID>()};

        LOG_WARNING(Service_Friend, "(STUBBED) called, uuid=0x{}", uuid.RawString());

        IPC::ResponseBuilder rb{ctx, 3};
        rb.Push(ResultSuccess);
        rb.Push(true);
    }

    void DeclareCloseOnlinePlaySession(HLERequestContext& ctx) {
        // Stub used by Splatoon 2
        LOG_WARNING(Service_Friend, "(STUBBED) called");
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(ResultSuccess);
    }

    void UpdateUserPresence(HLERequestContext& ctx) {
        // Stub used by Retro City Rampage
        LOG_WARNING(Service_Friend, "(STUBBED) called");
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(ResultSuccess);
    }

    void GetPlayHistoryRegistrationKey(HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx};
        const auto local_play = rp.Pop<bool>();
        const auto uuid = rp.PopRaw<Common::UUID>();

        LOG_WARNING(Service_Friend, "(STUBBED) called, local_play={}, uuid=0x{}", local_play,
                    uuid.RawString());

        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(ResultSuccess);
    }

    void GetFriendCount(HLERequestContext& ctx) {
        LOG_DEBUG(Service_Friend, "(STUBBED) called");

        IPC::ResponseBuilder rb{ctx, 3};
        rb.Push(ResultSuccess);
        rb.Push(0);
    }

    void GetNewlyFriendCount(HLERequestContext& ctx) {
        LOG_DEBUG(Service_Friend, "(STUBBED) called");

        IPC::ResponseBuilder rb{ctx, 3};
        rb.Push(ResultSuccess);
        rb.Push(0);
    }

    void GetReceivedFriendRequestCount(HLERequestContext& ctx) {
        LOG_DEBUG(Service_Friend, "(STUBBED) called");

        IPC::ResponseBuilder rb{ctx, 3};
        rb.Push(ResultSuccess);
        rb.Push(0);
    }

    void GetPlayHistoryStatistics(HLERequestContext& ctx) {
        LOG_ERROR(Service_Friend, "(STUBBED) called, check in out");

        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(ResultSuccess);
    }

    void GetReceivedFriendInvitationCountCache(HLERequestContext& ctx) {
        LOG_DEBUG(Service_Friend, "(STUBBED) called, check in out");

        IPC::ResponseBuilder rb{ctx, 3};
        rb.Push(ResultSuccess);
        rb.Push(0);
    }

    void GetUserPresenceView(HLERequestContext& ctx) {
        LOG_DEBUG(Service_Friend, "(STUBBED) called, check in out");

        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(ResultSuccess);
    }

    KernelHelpers::ServiceContext service_context;

    Kernel::KEvent* completion_event;
};

class INotificationService final : public ServiceFramework<INotificationService> {
public:
    explicit INotificationService(Core::System& system_, Common::UUID uuid_)
        : ServiceFramework{system_, "INotificationService"}, uuid{uuid_},
          service_context{system_, "INotificationService"} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, &INotificationService::GetEvent, "GetEvent"},
            {1, &INotificationService::Clear, "Clear"},
            {2, &INotificationService::Pop, "Pop"}
        };
        // clang-format on

        RegisterHandlers(functions);

        notification_event = service_context.CreateEvent("INotificationService:NotifyEvent");
    }

    ~INotificationService() override {
        service_context.CloseEvent(notification_event);
    }

private:
    void GetEvent(HLERequestContext& ctx) {
        LOG_DEBUG(Service_Friend, "called");

        IPC::ResponseBuilder rb{ctx, 2, 1};
        rb.Push(ResultSuccess);
        rb.PushCopyObjects(notification_event->GetReadableEvent());
    }

    void Clear(HLERequestContext& ctx) {
        LOG_DEBUG(Service_Friend, "called");
        while (!notifications.empty()) {
            notifications.pop();
        }
        std::memset(&states, 0, sizeof(States));

        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(ResultSuccess);
    }

    void Pop(HLERequestContext& ctx) {
        LOG_DEBUG(Service_Friend, "called");

        if (notifications.empty()) {
            LOG_ERROR(Service_Friend, "No notifications in queue!");
            IPC::ResponseBuilder rb{ctx, 2};
            rb.Push(Account::ResultNoNotifications);
            return;
        }

        const auto notification = notifications.front();
        notifications.pop();

        switch (notification.notification_type) {
        case NotificationTypes::HasUpdatedFriendsList:
            states.has_updated_friends = false;
            break;
        case NotificationTypes::HasReceivedFriendRequest:
            states.has_received_friend_request = false;
            break;
        default:
            // HOS seems not have an error case for an unknown notification
            LOG_WARNING(Service_Friend, "Unknown notification {:08X}",
                        notification.notification_type);
            break;
        }

        IPC::ResponseBuilder rb{ctx, 6};
        rb.Push(ResultSuccess);
        rb.PushRaw<SizedNotificationInfo>(notification);
    }

    enum class NotificationTypes : u32 {
        HasUpdatedFriendsList = 0x65,
        HasReceivedFriendRequest = 0x1
    };

    struct SizedNotificationInfo {
        NotificationTypes notification_type;
        INSERT_PADDING_WORDS(
            1); // TODO(ogniK): This doesn't seem to be used within any IPC returns as of now
        u64_le account_id;
    };
    static_assert(sizeof(SizedNotificationInfo) == 0x10,
                  "SizedNotificationInfo is an incorrect size");

    struct States {
        bool has_updated_friends;
        bool has_received_friend_request;
    };

    Common::UUID uuid;
    KernelHelpers::ServiceContext service_context;

    Kernel::KEvent* notification_event;
    std::queue<SizedNotificationInfo> notifications;
    States states{};
};

void Module::Interface::CreateFriendService(HLERequestContext& ctx) {
    IPC::ResponseBuilder rb{ctx, 2, 0, 1};
    rb.Push(ResultSuccess);
    rb.PushIpcInterface<IFriendService>(system);
    LOG_DEBUG(Service_Friend, "called");
}

void Module::Interface::CreateNotificationService(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    auto uuid = rp.PopRaw<Common::UUID>();

    LOG_DEBUG(Service_Friend, "called, uuid=0x{}", uuid.RawString());

    IPC::ResponseBuilder rb{ctx, 2, 0, 1};
    rb.Push(ResultSuccess);
    rb.PushIpcInterface<INotificationService>(system, uuid);
}

Module::Interface::Interface(std::shared_ptr<Module> module_, Core::System& system_,
                             const char* name)
    : ServiceFramework{system_, name}, module{std::move(module_)} {}

Module::Interface::~Interface() = default;

void LoopProcess(Core::System& system) {
    auto server_manager = std::make_unique<ServerManager>(system);
    auto module = std::make_shared<Module>();

    server_manager->RegisterNamedService("friend:a",
                                         std::make_shared<Friend>(module, system, "friend:a"));
    server_manager->RegisterNamedService("friend:m",
                                         std::make_shared<Friend>(module, system, "friend:m"));
    server_manager->RegisterNamedService("friend:s",
                                         std::make_shared<Friend>(module, system, "friend:s"));
    server_manager->RegisterNamedService("friend:u",
                                         std::make_shared<Friend>(module, system, "friend:u"));
    server_manager->RegisterNamedService("friend:v",
                                         std::make_shared<Friend>(module, system, "friend:v"));

    ServerManager::RunServer(std::move(server_manager));
}

} // namespace Service::Friend
