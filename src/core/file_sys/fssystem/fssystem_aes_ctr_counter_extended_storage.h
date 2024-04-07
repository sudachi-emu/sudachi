// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <optional>

#include "common/literals.h"
#include "core/file_sys/fssystem/fs_i_storage.h"
#include "core/file_sys/fssystem/fssystem_bucket_tree.h"

namespace FileSys {

using namespace Common::Literals;

class AesCtrCounterExtendedStorage : public IReadOnlyStorage {
    SUDACHI_NON_COPYABLE(AesCtrCounterExtendedStorage);
    SUDACHI_NON_MOVEABLE(AesCtrCounterExtendedStorage);

public:
    static constexpr size_t BlockSize = 0x10;
    static constexpr size_t KeySize = 0x10;
    static constexpr size_t IvSize = 0x10;
    static constexpr size_t NodeSize = 16_KiB;

    class IDecryptor {
    public:
        virtual ~IDecryptor() {}
        virtual void Decrypt(u8* buf, size_t buf_size, const std::array<u8, KeySize>& key,
                             const std::array<u8, IvSize>& iv) = 0;
    };

    struct Entry {
        enum class Encryption : u8 {
            Encrypted = 0,
            NotEncrypted = 1,
        };

        std::array<u8, sizeof(s64)> offset;
        Encryption encryption_value;
        std::array<u8, 3> reserved;
        s32 generation;

        void SetOffset(s64 value) {
            std::memcpy(this->offset.data(), std::addressof(value), sizeof(s64));
        }

        s64 GetOffset() const {
            s64 value;
            std::memcpy(std::addressof(value), this->offset.data(), sizeof(s64));
            return value;
        }
    };
    static_assert(sizeof(Entry) == 0x10);
    static_assert(alignof(Entry) == 4);
    static_assert(std::is_trivial_v<Entry>);

public:
    static constexpr s64 QueryHeaderStorageSize() {
        return BucketTree::QueryHeaderStorageSize();
    }

    static constexpr s64 QueryNodeStorageSize(s32 entry_count) {
        return BucketTree::QueryNodeStorageSize(NodeSize, sizeof(Entry), entry_count);
    }

    static constexpr s64 QueryEntryStorageSize(s32 entry_count) {
        return BucketTree::QueryEntryStorageSize(NodeSize, sizeof(Entry), entry_count);
    }

    static Result CreateSoftwareDecryptor(std::unique_ptr<IDecryptor>* out);

public:
    AesCtrCounterExtendedStorage()
        : m_table(), m_data_storage(), m_secure_value(), m_counter_offset(), m_decryptor() {}
    virtual ~AesCtrCounterExtendedStorage() {
        this->Finalize();
    }

    Result Initialize(const void* key, size_t key_size, u32 secure_value, s64 counter_offset,
                      VirtualFile data_storage, VirtualFile node_storage, VirtualFile entry_storage,
                      s32 entry_count, std::unique_ptr<IDecryptor>&& decryptor);
    void Finalize();

    bool IsInitialized() const {
        return m_table.IsInitialized();
    }

    virtual size_t Read(u8* buffer, size_t size, size_t offset) const override;

    virtual size_t GetSize() const override {
        BucketTree::Offsets offsets;
        ASSERT(R_SUCCEEDED(m_table.GetOffsets(std::addressof(offsets))));

        return offsets.end_offset;
    }

    Result GetEntryList(Entry* out_entries, s32* out_entry_count, s32 entry_count, s64 offset,
                        s64 size);

private:
    Result Initialize(const void* key, size_t key_size, u32 secure_value, VirtualFile data_storage,
                      VirtualFile table_storage);

private:
    mutable BucketTree m_table;
    VirtualFile m_data_storage;
    std::array<u8, KeySize> m_key;
    u32 m_secure_value;
    s64 m_counter_offset;
    std::unique_ptr<IDecryptor> m_decryptor;
};

} // namespace FileSys
