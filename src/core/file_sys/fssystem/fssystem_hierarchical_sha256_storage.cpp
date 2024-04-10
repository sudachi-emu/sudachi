// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/alignment.h"
#include "common/scope_exit.h"
#include "core/file_sys/fssystem/fssystem_hierarchical_sha256_storage.h"

namespace FileSys {

namespace {

s32 Log2(s32 value) {
    ASSERT(value > 0);
    ASSERT(Common::IsPowerOfTwo(value));

    s32 log = 0;
    while ((value >>= 1) > 0) {
        ++log;
    }
    return log;
}

} // namespace

Result HierarchicalSha256Storage::Initialize(VirtualFile* base_storages, s32 layer_count,
                                             size_t htbs, void* hash_buf, size_t hash_buf_size) {
    // Validate preconditions.
    ASSERT(layer_count == LayerCount);
    ASSERT(Common::IsPowerOfTwo(htbs));
    ASSERT(hash_buf != nullptr);

    // Set size tracking members.
    m_hash_target_block_size = static_cast<s32>(htbs);
    m_log_size_ratio = Log2(m_hash_target_block_size / HashSize);

    // Get the base storage size.
    m_base_storage_size = base_storages[2]->GetSize();
    {
        auto size_guard = SCOPE_GUARD {
            m_base_storage_size = 0;
        };
        R_UNLESS(m_base_storage_size <= static_cast<s64>(HashSize)
                                            << m_log_size_ratio << m_log_size_ratio,
                 ResultHierarchicalSha256BaseStorageTooLarge);
        size_guard.Cancel();
    }

    // Set hash buffer tracking members.
    m_base_storage = base_storages[2];
    m_hash_buffer = static_cast<char*>(hash_buf);
    m_hash_buffer_size = hash_buf_size;

    // Read the master hash.
    std::array<u8, HashSize> master_hash{};
    base_storages[0]->ReadObject(std::addressof(master_hash));

    // Read and validate the data being hashed.
    s64 hash_storage_size = base_storages[1]->GetSize();
    ASSERT(Common::IsAligned(hash_storage_size, HashSize));
    ASSERT(hash_storage_size <= m_hash_target_block_size);
    ASSERT(hash_storage_size <= static_cast<s64>(m_hash_buffer_size));

    base_storages[1]->Read(reinterpret_cast<u8*>(m_hash_buffer),
                           static_cast<size_t>(hash_storage_size), 0);

    R_SUCCEED();
}

size_t HierarchicalSha256Storage::Read(u8* buffer, size_t size, size_t offset) const {
    // Succeed if zero-size.
    if (size == 0) {
        return size;
    }

    // Validate that we have a buffer to read into.
    ASSERT(buffer != nullptr);

    // Read the data.
    return m_base_storage->Read(buffer, size, offset);
}

} // namespace FileSys
