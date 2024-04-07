// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <mutex>
#include <optional>

#include "core/crypto/aes_util.h"
#include "core/crypto/key_manager.h"
#include "core/file_sys/fssystem/fs_i_storage.h"

namespace FileSys {

class AesXtsStorage : public IReadOnlyStorage {
    SUDACHI_NON_COPYABLE(AesXtsStorage);
    SUDACHI_NON_MOVEABLE(AesXtsStorage);

public:
    static constexpr size_t AesBlockSize = 0x10;
    static constexpr size_t KeySize = 0x20;
    static constexpr size_t IvSize = 0x10;

public:
    static void MakeAesXtsIv(void* dst, size_t dst_size, s64 offset, size_t block_size);

public:
    AesXtsStorage(VirtualFile base, const void* key1, const void* key2, size_t key_size,
                  const void* iv, size_t iv_size, size_t block_size);

    virtual size_t Read(u8* buffer, size_t size, size_t offset) const override;
    virtual size_t GetSize() const override;

private:
    VirtualFile m_base_storage;
    std::array<u8, KeySize> m_key;
    std::array<u8, IvSize> m_iv;
    const size_t m_block_size;
    std::mutex m_mutex;
    mutable std::optional<Core::Crypto::AESCipher<Core::Crypto::Key256>> m_cipher;
};

} // namespace FileSys
