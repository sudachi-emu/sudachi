// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "core/file_sys/fssystem/fssystem_hierarchical_integrity_verification_storage.h"
#include "core/file_sys/fssystem/fssystem_nca_header.h"
#include "core/file_sys/vfs/vfs_vector.h"

namespace FileSys {

constexpr inline size_t IntegrityLayerCountRomFs = 7;
constexpr inline size_t IntegrityHashLayerBlockSize = 16_KiB;

class IntegrityRomFsStorage : public IReadOnlyStorage {
public:
    IntegrityRomFsStorage() {}
    virtual ~IntegrityRomFsStorage() override {
        this->Finalize();
    }

    Result Initialize(
        HierarchicalIntegrityVerificationInformation level_hash_info, Hash master_hash,
        HierarchicalIntegrityVerificationStorage::HierarchicalStorageInformation storage_info,
        int max_data_cache_entries, int max_hash_cache_entries, s8 buffer_level);
    void Finalize();

    virtual size_t Read(u8* buffer, size_t size, size_t offset) const override {
        return m_integrity_storage.Read(buffer, size, offset);
    }

    virtual size_t GetSize() const override {
        return m_integrity_storage.GetSize();
    }

private:
    HierarchicalIntegrityVerificationStorage m_integrity_storage;
    Hash m_master_hash;
    std::shared_ptr<ArrayVfsFile<sizeof(Hash)>> m_master_hash_storage;
};

} // namespace FileSys
