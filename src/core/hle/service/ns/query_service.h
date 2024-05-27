// SPDX-FileCopyrightText: Copyright 2021 sudachi Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "common/uuid.h"
#include "core/hle/service/cmif_types.h"
#include "core/hle/service/ns/ns_types.h"
#include "core/hle/service/service.h"

namespace Service::NS {

struct PlayStatistics {
    u64 application_id{};
    u32 first_entry_index{};
    u32 first_timestamp_user{};
    u32 first_timestamp_network{};
    u32 last_entry_index{};
    u32 last_timestamp_user{};
    u32 last_timestamp_network{};
    u32 play_time_in_minutes{};
    u32 total_launches{};
};
static_assert(sizeof(PlayStatistics) == 0x28, "PlayStatistics is an invalid size");

struct LastPlayTime {};

class IQueryService final : public ServiceFramework<IQueryService> {
public:
    explicit IQueryService(Core::System& system_);
    ~IQueryService() override;

private:
    Result QueryPlayStatisticsByApplicationIdAndUserAccountId(
        Out<PlayStatistics> out_play_statistics, bool unknown, u64 application_id, Uid account_id);
    Result QueryLastPlayTime(Out<s32> out_entries, u8 unknown,
                             OutArray<LastPlayTime, BufferAttr_HipcMapAlias> out_last_play_times,
                             InArray<s32, BufferAttr_HipcMapAlias> application_ids);
};

} // namespace Service::NS
