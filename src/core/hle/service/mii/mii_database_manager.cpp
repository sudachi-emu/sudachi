// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/assert.h"
#include "common/fs/file.h"
#include "common/fs/fs.h"
#include "common/fs/path_util.h"
#include "common/logging/log.h"
#include "common/string_util.h"

#include "core/hle/service/mii/mii_database_manager.h"
#include "core/hle/service/mii/mii_result.h"
#include "core/hle/service/mii/mii_util.h"
#include "core/hle/service/mii/types/char_info.h"
#include "core/hle/service/mii/types/store_data.h"

namespace Service::Mii {
const char* DbFileName = "MiiDatabase.dat";

DatabaseManager::DatabaseManager() {}

Result DatabaseManager::MountSaveData() {
    if (!is_save_data_mounted) {
        system_save_dir = Common::FS::GetSudachiPath(Common::FS::SudachiPath::NANDDir) /
                          "system/save/8000000000000030";
        if (is_test_db) {
            system_save_dir = Common::FS::GetSudachiPath(Common::FS::SudachiPath::NANDDir) /
                              "system/save/8000000000000031";
        }

        // mount point should be "mii:"

        if (!Common::FS::CreateDirs(system_save_dir)) {
            return ResultUnknown;
        }
    }

    is_save_data_mounted = true;
    return ResultSuccess;
}

Result DatabaseManager::Initialize(DatabaseSessionMetadata& metadata, bool& is_database_broken) {
    is_database_broken = false;
    if (!is_save_data_mounted) {
        return ResultInvalidArgument;
    }

    database.CleanDatabase();
    update_counter++;
    metadata.update_counter = update_counter;

    const Common::FS::IOFile db_file{system_save_dir / DbFileName, Common::FS::FileAccessMode::Read,
                                     Common::FS::FileType::BinaryFile};

    if (!db_file.IsOpen()) {
        return SaveDatabase();
    }

    if (Common::FS::GetSize(system_save_dir / DbFileName) != sizeof(NintendoFigurineDatabase)) {
        is_database_broken = true;
    }

    if (db_file.Read(database) != 1) {
        is_database_broken = true;
    }

    if (is_database_broken) {
        // Dragons happen here for simplicity just clean the database
        LOG_ERROR(Service_Mii, "Mii database is corrupted");
        database.CleanDatabase();
        return ResultUnknown;
    }

    const auto result = database.CheckIntegrity();

    if (result.IsError()) {
        LOG_ERROR(Service_Mii, "Mii database is corrupted 0x{:0x}", result.raw);
        database.CleanDatabase();
        return ResultSuccess;
    }

    LOG_INFO(Service_Mii, "Successfully loaded mii database. size={}",
             database.GetDatabaseLength());
    return ResultSuccess;
}

bool DatabaseManager::IsFullDatabase() const {
    return database.GetDatabaseLength() == MaxDatabaseLength;
}

bool DatabaseManager::IsModified() const {
    return is_moddified;
}

u64 DatabaseManager::GetUpdateCounter() const {
    return update_counter;
}

u32 DatabaseManager::GetCount(const DatabaseSessionMetadata& metadata) const {
    const u32 database_size = database.GetDatabaseLength();
    if (metadata.magic == MiiMagic) {
        return database_size;
    }

    // Special mii can't be used. Skip those.

    u32 mii_count{};
    for (std::size_t index = 0; index < database_size; ++index) {
        const auto& store_data = database.Get(index);
        if (store_data.IsSpecial()) {
            continue;
        }
        mii_count++;
    }

    return mii_count;
}

void DatabaseManager::Get(StoreData& out_store_data, std::size_t index,
                          const DatabaseSessionMetadata& metadata) const {
    if (metadata.magic == MiiMagic) {
        out_store_data = database.Get(index);
        return;
    }

    // The index refeers to the mii index without special mii.
    // Search on the database until we find it

    u32 virtual_index = 0;
    const u32 database_size = database.GetDatabaseLength();
    for (std::size_t i = 0; i < database_size; ++i) {
        const auto& store_data = database.Get(i);
        if (store_data.IsSpecial()) {
            continue;
        }
        if (virtual_index == index) {
            out_store_data = store_data;
            return;
        }
        virtual_index++;
    }

    // This function doesn't fail. It returns the first mii instead
    out_store_data = database.Get(0);
}

Result DatabaseManager::FindIndex(s32& out_index, const Common::UUID& create_id,
                                  bool is_special) const {
    u32 index{};
    const bool is_found = database.GetIndexByCreatorId(index, create_id);

    if (!is_found) {
        return ResultNotFound;
    }

    if (is_special) {
        out_index = index;
        return ResultSuccess;
    }

    if (database.Get(index).IsSpecial()) {
        return ResultNotFound;
    }

    out_index = 0;

    if (index < 1) {
        return ResultSuccess;
    }

    for (std::size_t i = 0; i < index; ++i) {
        if (database.Get(i).IsSpecial()) {
            continue;
        }
        out_index++;
    }
    return ResultSuccess;
}

Result DatabaseManager::FindIndex(const DatabaseSessionMetadata& metadata, u32& out_index,
                                  const Common::UUID& create_id) const {
    u32 index{};
    const bool is_found = database.GetIndexByCreatorId(index, create_id);

    if (!is_found) {
        return ResultNotFound;
    }

    if (metadata.magic == MiiMagic) {
        out_index = index;
        return ResultSuccess;
    }

    if (database.Get(index).IsSpecial()) {
        return ResultNotFound;
    }

    out_index = 0;

    if (index < 1) {
        return ResultSuccess;
    }

    // The index refeers to the mii index without special mii.
    // Search on the database until we find it

    for (std::size_t i = 0; i <= index; ++i) {
        const auto& store_data = database.Get(i);
        if (store_data.IsSpecial()) {
            continue;
        }
        out_index++;
    }
    return ResultSuccess;
}

Result DatabaseManager::FindMoveIndex(u32& out_index, u32 new_index,
                                      const Common::UUID& create_id) const {
    const auto database_size = database.GetDatabaseLength();

    if (database_size >= 1) {
        u32 virtual_index{};
        for (std::size_t i = 0; i < database_size; ++i) {
            const StoreData& store_data = database.Get(i);
            if (store_data.IsSpecial()) {
                continue;
            }
            if (virtual_index == new_index) {
                const bool is_found = database.GetIndexByCreatorId(out_index, create_id);
                if (!is_found) {
                    return ResultNotFound;
                }
                if (store_data.IsSpecial()) {
                    return ResultInvalidOperation;
                }
                return ResultSuccess;
            }
            virtual_index++;
        }
    }

    const bool is_found = database.GetIndexByCreatorId(out_index, create_id);
    if (!is_found) {
        return ResultNotFound;
    }
    const StoreData& store_data = database.Get(out_index);
    if (store_data.IsSpecial()) {
        return ResultInvalidOperation;
    }
    return ResultSuccess;
}

Result DatabaseManager::Move(DatabaseSessionMetadata& metadata, u32 new_index,
                             const Common::UUID& create_id) {
    u32 current_index{};
    if (metadata.magic == MiiMagic) {
        const bool is_found = database.GetIndexByCreatorId(current_index, create_id);
        if (!is_found) {
            return ResultNotFound;
        }
    } else {
        const auto result = FindMoveIndex(current_index, new_index, create_id);
        if (result.IsError()) {
            return result;
        }
    }

    const auto result = database.Move(current_index, new_index);
    if (result.IsFailure()) {
        return result;
    }

    is_moddified = true;
    update_counter++;
    metadata.update_counter = update_counter;
    return ResultSuccess;
}

Result DatabaseManager::AddOrReplace(DatabaseSessionMetadata& metadata,
                                     const StoreData& store_data) {
    if (store_data.IsValid() != ValidationResult::NoErrors) {
        return ResultInvalidStoreData;
    }
    if (metadata.magic != MiiMagic && store_data.IsSpecial()) {
        return ResultInvalidOperation;
    }

    u32 index{};
    const bool is_found = database.GetIndexByCreatorId(index, store_data.GetCreateId());
    if (is_found) {
        const StoreData& old_store_data = database.Get(index);

        if (store_data.IsSpecial() != old_store_data.IsSpecial()) {
            return ResultInvalidOperation;
        }

        database.Replace(index, store_data);
    } else {
        if (database.IsFull()) {
            return ResultDatabaseFull;
        }

        database.Add(store_data);
    }

    is_moddified = true;
    update_counter++;
    metadata.update_counter = update_counter;
    return ResultSuccess;
}

Result DatabaseManager::Delete(DatabaseSessionMetadata& metadata, const Common::UUID& create_id) {
    u32 index{};
    const bool is_found = database.GetIndexByCreatorId(index, create_id);
    if (!is_found) {
        return ResultNotFound;
    }

    if (metadata.magic != MiiMagic) {
        const auto& store_data = database.Get(index);
        if (store_data.IsSpecial()) {
            return ResultInvalidOperation;
        }
    }

    database.Delete(index);

    is_moddified = true;
    update_counter++;
    metadata.update_counter = update_counter;
    return ResultSuccess;
}

Result DatabaseManager::Append(DatabaseSessionMetadata& metadata, const CharInfo& char_info) {
    if (char_info.Verify() != ValidationResult::NoErrors) {
        return ResultInvalidCharInfo2;
    }
    if (char_info.GetType() == 1) {
        return ResultInvalidCharInfoType;
    }

    u32 index{};
    StoreData store_data{};

    // Loop until the mii we created is not on the database
    do {
        store_data.BuildWithCharInfo(char_info);
    } while (database.GetIndexByCreatorId(index, store_data.GetCreateId()));

    const Result result = store_data.Restore();

    if (result.IsSuccess() || result == ResultNotUpdated) {
        return AddOrReplace(metadata, store_data);
    }

    return result;
}

Result DatabaseManager::DestroyFile(DatabaseSessionMetadata& metadata) {
    database.CorruptCrc();

    is_moddified = true;
    update_counter++;
    metadata.update_counter = update_counter;

    const auto result = SaveDatabase();
    database.CleanDatabase();

    return result;
}

Result DatabaseManager::DeleteFile() {
    const bool result = Common::FS::RemoveFile(system_save_dir / DbFileName);
    // TODO: Return proper FS error here
    return result ? ResultSuccess : ResultUnknown;
}

void DatabaseManager::Format(DatabaseSessionMetadata& metadata) {
    database.CleanDatabase();
    is_moddified = true;
    update_counter++;
    metadata.update_counter = update_counter;
}

Result DatabaseManager::SaveDatabase() {
    // TODO: Replace unknown error codes with proper FS error codes when available

    if (!Common::FS::Exists(system_save_dir / DbFileName)) {
        if (!Common::FS::NewFile(system_save_dir / DbFileName)) {
            LOG_ERROR(Service_Mii, "Failed to create mii database");
            return ResultUnknown;
        }
    }

    const auto file_size = Common::FS::GetSize(system_save_dir / DbFileName);
    if (file_size != 0 && file_size != sizeof(NintendoFigurineDatabase)) {
        if (!Common::FS::RemoveFile(system_save_dir / DbFileName)) {
            LOG_ERROR(Service_Mii, "Failed to delete mii database");
            return ResultUnknown;
        }
        if (!Common::FS::NewFile(system_save_dir / DbFileName)) {
            LOG_ERROR(Service_Mii, "Failed to create mii database");
            return ResultUnknown;
        }
    }

    const Common::FS::IOFile db_file{system_save_dir / DbFileName,
                                     Common::FS::FileAccessMode::ReadWrite,
                                     Common::FS::FileType::BinaryFile};

    if (db_file.Write(database) != 1) {
        LOG_ERROR(Service_Mii, "Failed to save mii database");
        return ResultUnknown;
    }

    is_moddified = false;
    return ResultSuccess;
}

} // namespace Service::Mii
