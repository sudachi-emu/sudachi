// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "core/hle/result.h"
#include "core/hle/service/mii/types/store_data.h"

namespace Service::Mii {

constexpr std::size_t MaxDatabaseLength{100};
constexpr u32 MiiMagic{0xa523b78f};
constexpr u32 DatabaseMagic{0x4244464e}; // NFDB

class NintendoFigurineDatabase {
public:
    /// Returns the total mii count.
    u8 GetDatabaseLength() const;

    /// Returns true if database is full.
    bool IsFull() const;

    /// Returns the mii of the specified index.
    StoreData Get(std::size_t index) const;

    /// Returns the total mii count. Ignoring special mii.
    u32 GetCount(const DatabaseSessionMetadata& metadata) const;

    /// Returns the index of a mii. If the mii isn't found returns false.
    bool GetIndexByCreatorId(u32& out_index, const Common::UUID& create_id) const;

    /// Moves the location of a specific mii.
    Result Move(u32 current_index, u32 new_index);

    /// Replaces mii with new data.
    void Replace(u32 index, const StoreData& store_data);

    /// Adds a new mii to the end of the database.
    void Add(const StoreData& store_data);

    /// Removes mii from database and shifts left the remainding data.
    void Delete(u32 index);

    /// Deletes all contents with a fresh database
    void CleanDatabase();

    /// Intentionally sets a bad checksum
    void CorruptCrc();

    /// Returns success if database is valid otherwise returns the corresponding error code.
    Result CheckIntegrity();

private:
    /// Returns the checksum of the database
    u16 GenerateDatabaseCrc();

    u32 magic{}; // 'NFDB'
    std::array<StoreData, MaxDatabaseLength> miis{};
    u8 version{};
    u8 database_length{};
    u16 crc{};
};
static_assert(sizeof(NintendoFigurineDatabase) == 0x1A98,
              "NintendoFigurineDatabase has incorrect size.");

}; // namespace Service::Mii
