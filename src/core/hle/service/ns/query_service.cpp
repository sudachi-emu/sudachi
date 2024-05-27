// SPDX-FileCopyrightText: Copyright 2021 sudachi Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/logging/log.h"
#include "common/uuid.h"
#include "core/hle/service/cmif_serialization.h"
#include "core/hle/service/ns/query_service.h"
#include "core/hle/service/service.h"

namespace Service::NS {

IQueryService::IQueryService(Core::System& system_) : ServiceFramework{system_, "pdm:qry"} {
    // clang-format off
    static const FunctionInfo functions[] = {
        {0, nullptr, "QueryAppletEvent"},
        {1, nullptr, "QueryPlayStatistics"},
        {2, nullptr, "QueryPlayStatisticsByUserAccountId"},
        {3, nullptr, "QueryPlayStatisticsByNetworkServiceAccountId"},
        {4, nullptr, "QueryPlayStatisticsByApplicationId"},
        {5, D<&IQueryService::QueryPlayStatisticsByApplicationIdAndUserAccountId>, "QueryPlayStatisticsByApplicationIdAndUserAccountId"},
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
        {17, D<&IQueryService::QueryLastPlayTime>, "QueryLastPlayTime"},
        {18, nullptr, "QueryApplicationPlayStatisticsForSystem"},
        {19, nullptr, "QueryApplicationPlayStatisticsByUserAccountIdForSystem"},
    };
    // clang-format on

    RegisterHandlers(functions);
}

IQueryService::~IQueryService() = default;

Result IQueryService::QueryPlayStatisticsByApplicationIdAndUserAccountId(
    Out<PlayStatistics> out_play_statistics, bool unknown, u64 application_id, Uid account_id) {
    // TODO(German77): Read statistics of the game
    *out_play_statistics = {
        .application_id = application_id,
        .total_launches = 1,
    };

    LOG_WARNING(Service_NS, "(STUBBED) called. unknown={}. application_id={:016X}, account_id={}",
                unknown, application_id, account_id.uuid.FormattedString());
    R_SUCCEED();
}

Result IQueryService::QueryLastPlayTime(
    Out<s32> out_entries, u8 unknown,
    OutArray<LastPlayTime, BufferAttr_HipcMapAlias> out_last_play_times,
    InArray<s32, BufferAttr_HipcMapAlias> application_ids) {
    *out_entries = 1;
    *out_last_play_times = {};
    R_SUCCEED();
}

} // namespace Service::NS
