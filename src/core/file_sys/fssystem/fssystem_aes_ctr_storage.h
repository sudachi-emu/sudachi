// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <optional>

#include "core/crypto/aes_util.h"
#include "core/crypto/key_manager.h"
#include "core/file_sys/errors.h"
#include "core/file_sys/fssystem/fs_i_storage.h"
#include "core/file_sys/vfs/vfs.h"

namespace FileSys {

class AesCtrStorage : public IStorage {
    SUDACHI_NON_COPYABLE(AesCtrStorage);
    SUDACHI_NON_MOVEABLE(AesCtrStorage);

public:
    static constexpr size_t BlockSize = 0x10;
    static constexpr size_t KeySize = 0x10;
    static constexpr size_t IvSize = 0x10;

public:
    static void MakeIv(void* dst, size_t dst_size, u64 upper, s64 offset);

public:
    AesCtrStorage(VirtualFile base, const void* key, size_t key_size, const void* iv,
                  size_t iv_size);

    virtual size_t Read(u8* buffer, size_t size, size_t offset) const override;
    virtual size_t Write(const u8* buffer, size_t size, size_t offset) override;
    virtual size_t GetSize() const override;

private:
    VirtualFile m_base_storage;
    std::array<u8, KeySize> m_key;
    std::array<u8, IvSize> m_iv;
    mutable std::optional<Core::Crypto::AESCipher<Core::Crypto::Key128>> m_cipher;
};

} // namespace FileSys
