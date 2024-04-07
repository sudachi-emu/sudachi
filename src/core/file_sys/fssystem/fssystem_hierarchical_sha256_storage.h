// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <mutex>

#include "core/file_sys/errors.h"
#include "core/file_sys/fssystem/fs_i_storage.h"
#include "core/file_sys/vfs/vfs.h"

namespace FileSys {

class HierarchicalSha256Storage : public IReadOnlyStorage {
    SUDACHI_NON_COPYABLE(HierarchicalSha256Storage);
    SUDACHI_NON_MOVEABLE(HierarchicalSha256Storage);

public:
    static constexpr s32 LayerCount = 3;
    static constexpr size_t HashSize = 256 / 8;

public:
    HierarchicalSha256Storage() : m_mutex() {}

    Result Initialize(VirtualFile* base_storages, s32 layer_count, size_t htbs, void* hash_buf,
                      size_t hash_buf_size);

    virtual size_t GetSize() const override {
        return m_base_storage->GetSize();
    }

    virtual size_t Read(u8* buffer, size_t length, size_t offset) const override;

private:
    VirtualFile m_base_storage;
    s64 m_base_storage_size;
    char* m_hash_buffer;
    size_t m_hash_buffer_size;
    s32 m_hash_target_block_size;
    s32 m_log_size_ratio;
    std::mutex m_mutex;
};

} // namespace FileSys
