// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/file_sys/fssystem/fssystem_integrity_romfs_storage.h"

namespace FileSys {

Result IntegrityRomFsStorage::Initialize(
    HierarchicalIntegrityVerificationInformation level_hash_info, Hash master_hash,
    HierarchicalIntegrityVerificationStorage::HierarchicalStorageInformation storage_info,
    int max_data_cache_entries, int max_hash_cache_entries, s8 buffer_level) {
    // Set master hash.
    m_master_hash = master_hash;
    m_master_hash_storage = std::make_shared<ArrayVfsFile<sizeof(Hash)>>(m_master_hash.value);
    R_UNLESS(m_master_hash_storage != nullptr,
             ResultAllocationMemoryFailedInIntegrityRomFsStorageA);

    // Set the master hash storage.
    storage_info[0] = m_master_hash_storage;

    // Initialize our integrity storage.
    R_RETURN(m_integrity_storage.Initialize(level_hash_info, storage_info, max_data_cache_entries,
                                            max_hash_cache_entries, buffer_level));
}

void IntegrityRomFsStorage::Finalize() {
    m_integrity_storage.Finalize();
}

} // namespace FileSys
