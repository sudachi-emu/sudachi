// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/hle/service/mii/mii_database.h"
#include "core/hle/service/mii/mii_result.h"
#include "core/hle/service/mii/mii_util.h"

namespace Service::Mii {

u8 NintendoFigurineDatabase::GetDatabaseLength() const {
    return database_length;
}

bool NintendoFigurineDatabase::IsFull() const {
    return database_length >= MaxDatabaseLength;
}

StoreData NintendoFigurineDatabase::Get(std::size_t index) const {
    StoreData store_data = miis.at(index);

    // This hack is to make external database dumps compatible
    store_data.SetDeviceChecksum();

    return store_data;
}

u32 NintendoFigurineDatabase::GetCount(const DatabaseSessionMetadata& metadata) const {
    if (magic == MiiMagic) {
        return GetDatabaseLength();
    }

    u32 mii_count{};
    for (std::size_t index = 0; index < mii_count; ++index) {
        const auto& store_data = Get(index);
        if (!store_data.IsSpecial()) {
            mii_count++;
        }
    }

    return mii_count;
}

bool NintendoFigurineDatabase::GetIndexByCreatorId(u32& out_index,
                                                   const Common::UUID& create_id) const {
    for (std::size_t index = 0; index < database_length; ++index) {
        if (miis[index].GetCreateId() == create_id) {
            out_index = static_cast<u32>(index);
            return true;
        }
    }

    return false;
}

Result NintendoFigurineDatabase::Move(u32 current_index, u32 new_index) {
    if (current_index == new_index) {
        return ResultNotUpdated;
    }

    const StoreData store_data = miis[current_index];

    if (new_index > current_index) {
        // Shift left
        const u32 index_diff = new_index - current_index;
        for (std::size_t i = 0; i < index_diff; i++) {
            miis[current_index + i] = miis[current_index + i + 1];
        }
    } else {
        // Shift right
        const u32 index_diff = current_index - new_index;
        for (std::size_t i = 0; i < index_diff; i++) {
            miis[current_index - i] = miis[current_index - i - 1];
        }
    }

    miis[new_index] = store_data;
    crc = GenerateDatabaseCrc();
    return ResultSuccess;
}

void NintendoFigurineDatabase::Replace(u32 index, const StoreData& store_data) {
    miis[index] = store_data;
    crc = GenerateDatabaseCrc();
}

void NintendoFigurineDatabase::Add(const StoreData& store_data) {
    miis[database_length] = store_data;
    database_length++;
    crc = GenerateDatabaseCrc();
}

void NintendoFigurineDatabase::Delete(u32 index) {
    // Shift left
    const s32 new_database_size = database_length - 1;
    if (static_cast<s32>(index) < new_database_size) {
        for (std::size_t i = index; i < static_cast<std::size_t>(new_database_size); i++) {
            miis[i] = miis[i + 1];
        }
    }

    database_length = static_cast<u8>(new_database_size);
    crc = GenerateDatabaseCrc();
}

void NintendoFigurineDatabase::CleanDatabase() {
    miis = {};
    version = 1;
    magic = DatabaseMagic;
    database_length = 0;
    crc = GenerateDatabaseCrc();
}

void NintendoFigurineDatabase::CorruptCrc() {
    crc = GenerateDatabaseCrc();
    crc = ~crc;
}

Result NintendoFigurineDatabase::CheckIntegrity() {
    if (magic != DatabaseMagic) {
        return ResultInvalidDatabaseSignature;
    }

    if (version != 1) {
        return ResultInvalidDatabaseVersion;
    }

    if (crc != GenerateDatabaseCrc()) {
        return ResultInvalidDatabaseChecksum;
    }

    if (database_length >= MaxDatabaseLength) {
        return ResultInvalidDatabaseLength;
    }

    return ResultSuccess;
}

u16 NintendoFigurineDatabase::GenerateDatabaseCrc() {
    return MiiUtil::CalculateCrc16(&magic, sizeof(NintendoFigurineDatabase) - sizeof(crc));
}

} // namespace Service::Mii
