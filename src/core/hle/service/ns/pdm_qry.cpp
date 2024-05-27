// SPDX-FileCopyrightText: Copyright 2021 sudachi Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <memory>

#include "common/logging/log.h"
#include "common/uuid.h"
#include "core/hle/service/ipc_helpers.h"
#include "core/hle/service/ns/pdm_qry.h"
#include "core/hle/service/service.h"

namespace Service::NS {

PDM_QRY::PDM_QRY(Core::System& system_) : ServiceFramework{system_, "pdm:qry"} {
    // clang-format off
        static const FunctionInfo functions[] = {
            {0, nullptr, "QueryAppletEvent"},
            {1, nullptr, "QueryPlayStatistics"},
            {2, nullptr, "QueryPlayStatisticsByUserAccountId"},
            {3, nullptr, "QueryPlayStatisticsByNetworkServiceAccountId"},
            {4, nullptr, "QueryPlayStatisticsByApplicationId"},
            {5, &PDM_QRY::QueryPlayStatisticsByApplicationIdAndUserAccountId, "QueryPlayStatisticsByApplicationIdAndUserAccountId"},
            {6, nullptr, "QueryPlayStatisticsByApplicationIdAndNetworkServiceAccountId"},
            {7, nullptr, "QueryLastPlayTimeV0"},
            {8, nullptr, "QueryPlayEvent"},
            {9, nullptr, "GetAvailablePlayEventRange"},
            {10, nullptr, "QueryAccountEvent"},
            {11, nullptr, "QueryAccountPlayEvent"},
            {12, nullptr, "GetAvailableAccountPlayEventRange"},
            {13, nullptr, "QueryApplicationPlayStatisticsForSystemV0"},
            {14, nullptr, "QueryRecentlyPlayedApplication"},
            {15, nullptr, "GetRecentlyPlayedApplicationUpdateEvent"},
            {16, nullptr, "QueryApplicationPlayStatisticsByUserAccountIdForSystemV0"},
            {17, &PDM_QRY::QueryLastPlayTime, "QueryLastPlayTime"},
            {18, nullptr, "QueryApplicationPlayStatisticsForSystem"},
            {19, nullptr, "QueryApplicationPlayStatisticsByUserAccountIdForSystem"},
        };
    // clang-format on

    RegisterHandlers(functions);
}

PDM_QRY::~PDM_QRY() = default;

void PDM_QRY::QueryPlayStatisticsByApplicationIdAndUserAccountId(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto unknown = rp.Pop<bool>();
    rp.Pop<u8>(); // Padding
    const auto application_id = rp.Pop<u64>();
    const auto user_account_uid = rp.PopRaw<Common::UUID>();

    // TODO(German77): Read statistics of the game
    PlayStatistics statistics{
        .application_id = application_id,
        .total_launches = 1,
    };

    LOG_WARNING(Service_NS,
                "(STUBBED) called. unknown={}. application_id=0x{:016X}, user_account_uid=0x{}",
                unknown, application_id, user_account_uid.RawString());

    IPC::ResponseBuilder rb{ctx, 12};
    rb.Push(ResultSuccess);
    rb.PushRaw(statistics);
}

void PDM_QRY::QueryLastPlayTime(HLERequestContext& ctx) {
    const s32 total_entries = 1;

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.Push(total_entries);
}

} // namespace Service::NS
