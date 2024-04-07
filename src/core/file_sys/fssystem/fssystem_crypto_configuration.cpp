// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/crypto/aes_util.h"
#include "core/crypto/key_manager.h"
#include "core/file_sys/fssystem/fssystem_crypto_configuration.h"

namespace FileSys {

namespace {

void GenerateKey(void* dst_key, size_t dst_key_size, const void* src_key, size_t src_key_size,
                 s32 key_type) {
    if (key_type == static_cast<s32>(KeyType::ZeroKey)) {
        std::memset(dst_key, 0, dst_key_size);
        return;
    }

    if (key_type == static_cast<s32>(KeyType::InvalidKey) ||
        key_type < static_cast<s32>(KeyType::ZeroKey) ||
        key_type >= static_cast<s32>(KeyType::NcaExternalKey)) {
        std::memset(dst_key, 0xFF, dst_key_size);
        return;
    }

    const auto& instance = Core::Crypto::KeyManager::Instance();

    if (key_type == static_cast<s32>(KeyType::NcaHeaderKey1) ||
        key_type == static_cast<s32>(KeyType::NcaHeaderKey2)) {
        const s32 key_index = static_cast<s32>(KeyType::NcaHeaderKey2) == key_type;
        const auto key = instance.GetKey(Core::Crypto::S256KeyType::Header);
        std::memcpy(dst_key, key.data() + key_index * 0x10, std::min(dst_key_size, key.size() / 2));
        return;
    }

    const s32 key_generation =
        std::max(key_type / NcaCryptoConfiguration::KeyAreaEncryptionKeyIndexCount, 1) - 1;
    const s32 key_index = key_type % NcaCryptoConfiguration::KeyAreaEncryptionKeyIndexCount;

    Core::Crypto::AESCipher<Core::Crypto::Key128> cipher(
        instance.GetKey(Core::Crypto::S128KeyType::KeyArea, key_generation, key_index),
        Core::Crypto::Mode::ECB);
    cipher.Transcode(reinterpret_cast<const u8*>(src_key), src_key_size,
                     reinterpret_cast<u8*>(dst_key), Core::Crypto::Op::Decrypt);
}

} // namespace

const NcaCryptoConfiguration& GetCryptoConfiguration() {
    static const NcaCryptoConfiguration configuration = {
        .header_1_sign_key_moduli{},
        .header_1_sign_key_public_exponent{},
        .key_area_encryption_key_source{},
        .header_encryption_key_source{},
        .header_encrypted_encryption_keys{},
        .generate_key = GenerateKey,
        .verify_sign1{},
        .is_plaintext_header_available{},
        .is_available_sw_key{},
    };

    return configuration;
}

} // namespace FileSys
