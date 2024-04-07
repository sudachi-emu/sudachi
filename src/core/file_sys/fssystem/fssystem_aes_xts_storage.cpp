// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/alignment.h"
#include "common/swap.h"
#include "core/file_sys/errors.h"
#include "core/file_sys/fssystem/fssystem_aes_xts_storage.h"
#include "core/file_sys/fssystem/fssystem_pooled_buffer.h"
#include "core/file_sys/fssystem/fssystem_utility.h"

namespace FileSys {

void AesXtsStorage::MakeAesXtsIv(void* dst, size_t dst_size, s64 offset, size_t block_size) {
    ASSERT(dst != nullptr);
    ASSERT(dst_size == IvSize);
    ASSERT(offset >= 0);

    const uintptr_t out_addr = reinterpret_cast<uintptr_t>(dst);

    *reinterpret_cast<s64_be*>(out_addr + sizeof(s64)) = offset / block_size;
}

AesXtsStorage::AesXtsStorage(VirtualFile base, const void* key1, const void* key2, size_t key_size,
                             const void* iv, size_t iv_size, size_t block_size)
    : m_base_storage(std::move(base)), m_block_size(block_size), m_mutex() {
    ASSERT(m_base_storage != nullptr);
    ASSERT(key1 != nullptr);
    ASSERT(key2 != nullptr);
    ASSERT(iv != nullptr);
    ASSERT(key_size == KeySize);
    ASSERT(iv_size == IvSize);
    ASSERT(Common::IsAligned(m_block_size, AesBlockSize));

    std::memcpy(m_key.data() + 0, key1, KeySize / 2);
    std::memcpy(m_key.data() + 0x10, key2, KeySize / 2);
    std::memcpy(m_iv.data(), iv, IvSize);

    m_cipher.emplace(m_key, Core::Crypto::Mode::XTS);
}

size_t AesXtsStorage::Read(u8* buffer, size_t size, size_t offset) const {
    // Allow zero-size reads.
    if (size == 0) {
        return size;
    }

    // Ensure buffer is valid.
    ASSERT(buffer != nullptr);

    // We can only read at block aligned offsets.
    ASSERT(Common::IsAligned(offset, AesBlockSize));
    ASSERT(Common::IsAligned(size, AesBlockSize));

    // Read the data.
    m_base_storage->Read(buffer, size, offset);

    // Setup the counter.
    std::array<u8, IvSize> ctr;
    std::memcpy(ctr.data(), m_iv.data(), IvSize);
    AddCounter(ctr.data(), IvSize, offset / m_block_size);

    // Handle any unaligned data before the start.
    size_t processed_size = 0;
    if ((offset % m_block_size) != 0) {
        // Determine the size of the pre-data read.
        const size_t skip_size =
            static_cast<size_t>(offset - Common::AlignDown(offset, m_block_size));
        const size_t data_size = std::min(size, m_block_size - skip_size);

        // Decrypt into a pooled buffer.
        {
            PooledBuffer tmp_buf(m_block_size, m_block_size);
            ASSERT(tmp_buf.GetSize() >= m_block_size);

            std::memset(tmp_buf.GetBuffer(), 0, skip_size);
            std::memcpy(tmp_buf.GetBuffer() + skip_size, buffer, data_size);

            m_cipher->SetIV(ctr);
            m_cipher->Transcode(tmp_buf.GetBuffer(), m_block_size, tmp_buf.GetBuffer(),
                                Core::Crypto::Op::Decrypt);

            std::memcpy(buffer, tmp_buf.GetBuffer() + skip_size, data_size);
        }

        AddCounter(ctr.data(), IvSize, 1);
        processed_size += data_size;
        ASSERT(processed_size == std::min(size, m_block_size - skip_size));
    }

    // Decrypt aligned chunks.
    char* cur = reinterpret_cast<char*>(buffer) + processed_size;
    size_t remaining = size - processed_size;
    while (remaining > 0) {
        const size_t cur_size = std::min(m_block_size, remaining);

        m_cipher->SetIV(ctr);
        m_cipher->Transcode(cur, cur_size, cur, Core::Crypto::Op::Decrypt);

        remaining -= cur_size;
        cur += cur_size;

        AddCounter(ctr.data(), IvSize, 1);
    }

    return size;
}

size_t AesXtsStorage::GetSize() const {
    return m_base_storage->GetSize();
}

} // namespace FileSys
