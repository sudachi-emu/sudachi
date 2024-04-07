// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "common/fs/fs.h"
#include "core/hle/result.h"
#include "core/hle/service/mii/mii_database.h"

namespace Service::Mii {
class CharInfo;
class StoreData;

class DatabaseManager {
public:
    DatabaseManager();
    Result MountSaveData();
    Result Initialize(DatabaseSessionMetadata& metadata, bool& is_database_broken);

    bool IsFullDatabase() const;
    bool IsModified() const;
    u64 GetUpdateCounter() const;

    void Get(StoreData& out_store_data, std::size_t index,
             const DatabaseSessionMetadata& metadata) const;
    u32 GetCount(const DatabaseSessionMetadata& metadata) const;

    Result FindIndex(s32& out_index, const Common::UUID& create_id, bool is_special) const;
    Result FindIndex(const DatabaseSessionMetadata& metadata, u32& out_index,
                     const Common::UUID& create_id) const;
    Result FindMoveIndex(u32& out_index, u32 new_index, const Common::UUID& create_id) const;

    Result Move(DatabaseSessionMetadata& metadata, u32 current_index,
                const Common::UUID& create_id);
    Result AddOrReplace(DatabaseSessionMetadata& metadata, const StoreData& out_store_data);
    Result Delete(DatabaseSessionMetadata& metadata, const Common::UUID& create_id);
    Result Append(DatabaseSessionMetadata& metadata, const CharInfo& char_info);

    Result DestroyFile(DatabaseSessionMetadata& metadata);
    Result DeleteFile();
    void Format(DatabaseSessionMetadata& metadata);

    Result SaveDatabase();

private:
    // This is the global value of
    // nn::settings::fwdbg::GetSettingsItemValue("is_db_test_mode_enabled");
    bool is_test_db{};

    bool is_moddified{};
    bool is_save_data_mounted{};
    u64 update_counter{};
    NintendoFigurineDatabase database{};

    std::filesystem::path system_save_dir{};
};

}; // namespace Service::Mii
