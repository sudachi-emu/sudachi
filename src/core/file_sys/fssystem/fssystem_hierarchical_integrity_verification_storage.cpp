// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/file_sys/fssystem/fssystem_hierarchical_integrity_verification_storage.h"
#include "core/file_sys/vfs/vfs_offset.h"

namespace FileSys {

HierarchicalIntegrityVerificationStorage::HierarchicalIntegrityVerificationStorage()
    : m_data_size(-1) {
    for (size_t i = 0; i < MaxLayers - 1; i++) {
        m_verify_storages[i] = std::make_shared<IntegrityVerificationStorage>();
    }
}

Result HierarchicalIntegrityVerificationStorage::Initialize(
    const HierarchicalIntegrityVerificationInformation& info,
    HierarchicalStorageInformation storage, int max_data_cache_entries, int max_hash_cache_entries,
    s8 buffer_level) {
    // Validate preconditions.
    ASSERT(IntegrityMinLayerCount <= info.max_layers && info.max_layers <= IntegrityMaxLayerCount);

    // Set member variables.
    m_max_layers = info.max_layers;

    // Initialize the top level verification storage.
    m_verify_storages[0]->Initialize(storage[HierarchicalStorageInformation::MasterStorage],
                                     storage[HierarchicalStorageInformation::Layer1Storage],
                                     static_cast<s64>(1) << info.info[0].block_order, HashSize,
                                     false);

    // Ensure we don't leak state if further initialization goes wrong.
    ON_RESULT_FAILURE {
        m_verify_storages[0]->Finalize();
        m_data_size = -1;
    };

    // Initialize the top level buffer storage.
    m_buffer_storages[0] = m_verify_storages[0];
    R_UNLESS(m_buffer_storages[0] != nullptr, ResultAllocationMemoryFailedAllocateShared);

    // Prepare to initialize the level storages.
    s32 level = 0;

    // Ensure we don't leak state if further initialization goes wrong.
    ON_RESULT_FAILURE_2 {
        m_verify_storages[level + 1]->Finalize();
        for (; level > 0; --level) {
            m_buffer_storages[level].reset();
            m_verify_storages[level]->Finalize();
        }
    };

    // Initialize the level storages.
    for (; level < m_max_layers - 3; ++level) {
        // Initialize the verification storage.
        auto buffer_storage =
            std::make_shared<OffsetVfsFile>(m_buffer_storages[level], info.info[level].size, 0);
        m_verify_storages[level + 1]->Initialize(
            std::move(buffer_storage), storage[level + 2],
            static_cast<s64>(1) << info.info[level + 1].block_order,
            static_cast<s64>(1) << info.info[level].block_order, false);

        // Initialize the buffer storage.
        m_buffer_storages[level + 1] = m_verify_storages[level + 1];
        R_UNLESS(m_buffer_storages[level + 1] != nullptr,
                 ResultAllocationMemoryFailedAllocateShared);
    }

    // Initialize the final level storage.
    {
        // Initialize the verification storage.
        auto buffer_storage =
            std::make_shared<OffsetVfsFile>(m_buffer_storages[level], info.info[level].size, 0);
        m_verify_storages[level + 1]->Initialize(
            std::move(buffer_storage), storage[level + 2],
            static_cast<s64>(1) << info.info[level + 1].block_order,
            static_cast<s64>(1) << info.info[level].block_order, true);

        // Initialize the buffer storage.
        m_buffer_storages[level + 1] = m_verify_storages[level + 1];
        R_UNLESS(m_buffer_storages[level + 1] != nullptr,
                 ResultAllocationMemoryFailedAllocateShared);
    }

    // Set the data size.
    m_data_size = info.info[level + 1].size;

    // We succeeded.
    R_SUCCEED();
}

void HierarchicalIntegrityVerificationStorage::Finalize() {
    if (m_data_size >= 0) {
        m_data_size = 0;

        for (s32 level = m_max_layers - 2; level >= 0; --level) {
            m_buffer_storages[level].reset();
            m_verify_storages[level]->Finalize();
        }

        m_data_size = -1;
    }
}

size_t HierarchicalIntegrityVerificationStorage::Read(u8* buffer, size_t size,
                                                      size_t offset) const {
    // Validate preconditions.
    ASSERT(m_data_size >= 0);

    // Succeed if zero-size.
    if (size == 0) {
        return size;
    }

    // Validate arguments.
    ASSERT(buffer != nullptr);

    // Read the data.
    return m_buffer_storages[m_max_layers - 2]->Read(buffer, size, offset);
}

size_t HierarchicalIntegrityVerificationStorage::GetSize() const {
    return m_data_size;
}

} // namespace FileSys
