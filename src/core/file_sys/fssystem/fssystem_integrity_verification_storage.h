// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <optional>

#include "core/file_sys/fssystem/fs_i_storage.h"
#include "core/file_sys/fssystem/fs_types.h"

namespace FileSys {

class IntegrityVerificationStorage : public IReadOnlyStorage {
    SUDACHI_NON_COPYABLE(IntegrityVerificationStorage);
    SUDACHI_NON_MOVEABLE(IntegrityVerificationStorage);

public:
    static constexpr s64 HashSize = 256 / 8;

    struct BlockHash {
        std::array<u8, HashSize> hash;
    };
    static_assert(std::is_trivial_v<BlockHash>);

public:
    IntegrityVerificationStorage()
        : m_verification_block_size(0), m_verification_block_order(0),
          m_upper_layer_verification_block_size(0), m_upper_layer_verification_block_order(0) {}
    virtual ~IntegrityVerificationStorage() override {
        this->Finalize();
    }

    void Initialize(VirtualFile hs, VirtualFile ds, s64 verif_block_size,
                    s64 upper_layer_verif_block_size, bool is_real_data);
    void Finalize();

    virtual size_t Read(u8* buffer, size_t size, size_t offset) const override;
    virtual size_t GetSize() const override;

    s64 GetBlockSize() const {
        return m_verification_block_size;
    }

private:
    static void SetValidationBit(BlockHash* hash) {
        ASSERT(hash != nullptr);
        hash->hash[HashSize - 1] |= 0x80;
    }

    static bool IsValidationBit(const BlockHash* hash) {
        ASSERT(hash != nullptr);
        return (hash->hash[HashSize - 1] & 0x80) != 0;
    }

private:
    VirtualFile m_hash_storage;
    VirtualFile m_data_storage;
    s64 m_verification_block_size;
    s64 m_verification_block_order;
    s64 m_upper_layer_verification_block_size;
    s64 m_upper_layer_verification_block_order;
    bool m_is_real_data;
};

} // namespace FileSys
