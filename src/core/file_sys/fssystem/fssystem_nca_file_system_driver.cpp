// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/file_sys/fssystem/fssystem_aes_ctr_counter_extended_storage.h"
#include "core/file_sys/fssystem/fssystem_aes_ctr_storage.h"
#include "core/file_sys/fssystem/fssystem_aes_xts_storage.h"
#include "core/file_sys/fssystem/fssystem_alignment_matching_storage.h"
#include "core/file_sys/fssystem/fssystem_compressed_storage.h"
#include "core/file_sys/fssystem/fssystem_hierarchical_integrity_verification_storage.h"
#include "core/file_sys/fssystem/fssystem_hierarchical_sha256_storage.h"
#include "core/file_sys/fssystem/fssystem_indirect_storage.h"
#include "core/file_sys/fssystem/fssystem_integrity_romfs_storage.h"
#include "core/file_sys/fssystem/fssystem_memory_resource_buffer_hold_storage.h"
#include "core/file_sys/fssystem/fssystem_nca_file_system_driver.h"
#include "core/file_sys/fssystem/fssystem_sparse_storage.h"
#include "core/file_sys/fssystem/fssystem_switch_storage.h"
#include "core/file_sys/vfs/vfs_offset.h"
#include "core/file_sys/vfs/vfs_vector.h"

namespace FileSys {

namespace {

constexpr inline s32 IntegrityDataCacheCount = 24;
constexpr inline s32 IntegrityHashCacheCount = 8;

constexpr inline s32 IntegrityDataCacheCountForMeta = 16;
constexpr inline s32 IntegrityHashCacheCountForMeta = 2;

class SharedNcaBodyStorage : public IReadOnlyStorage {
    SUDACHI_NON_COPYABLE(SharedNcaBodyStorage);
    SUDACHI_NON_MOVEABLE(SharedNcaBodyStorage);

private:
    VirtualFile m_storage;
    std::shared_ptr<NcaReader> m_nca_reader;

public:
    SharedNcaBodyStorage(VirtualFile s, std::shared_ptr<NcaReader> r)
        : m_storage(std::move(s)), m_nca_reader(std::move(r)) {}

    virtual size_t Read(u8* buffer, size_t size, size_t offset) const override {
        // Validate pre-conditions.
        ASSERT(m_storage != nullptr);

        // Read from the base storage.
        return m_storage->Read(buffer, size, offset);
    }

    virtual size_t GetSize() const override {
        // Validate pre-conditions.
        ASSERT(m_storage != nullptr);

        return m_storage->GetSize();
    }
};

inline s64 GetFsOffset(const NcaReader& reader, s32 fs_index) {
    return static_cast<s64>(reader.GetFsOffset(fs_index));
}

inline s64 GetFsEndOffset(const NcaReader& reader, s32 fs_index) {
    return static_cast<s64>(reader.GetFsEndOffset(fs_index));
}

using Sha256DataRegion = NcaFsHeader::Region;
using IntegrityLevelInfo = NcaFsHeader::HashData::IntegrityMetaInfo::LevelHashInfo;
using IntegrityDataInfo = IntegrityLevelInfo::HierarchicalIntegrityVerificationLevelInformation;

} // namespace

Result NcaFileSystemDriver::OpenStorageWithContext(VirtualFile* out,
                                                   NcaFsHeaderReader* out_header_reader,
                                                   s32 fs_index, StorageContext* ctx) {
    // Open storage.
    R_RETURN(this->OpenStorageImpl(out, out_header_reader, fs_index, ctx));
}

Result NcaFileSystemDriver::OpenStorageImpl(VirtualFile* out, NcaFsHeaderReader* out_header_reader,
                                            s32 fs_index, StorageContext* ctx) {
    // Validate preconditions.
    ASSERT(out != nullptr);
    ASSERT(out_header_reader != nullptr);
    ASSERT(0 <= fs_index && fs_index < NcaHeader::FsCountMax);

    // Validate the fs index.
    R_UNLESS(m_reader->HasFsInfo(fs_index), ResultPartitionNotFound);

    // Initialize our header reader for the fs index.
    R_TRY(out_header_reader->Initialize(*m_reader, fs_index));

    // Declare the storage we're opening.
    VirtualFile storage;

    // Process sparse layer.
    s64 fs_data_offset = 0;
    if (out_header_reader->ExistsSparseLayer()) {
        // Get the sparse info.
        const auto& sparse_info = out_header_reader->GetSparseInfo();

        // Create based on whether we have a meta hash layer.
        if (out_header_reader->ExistsSparseMetaHashLayer()) {
            // Create the sparse storage with verification.
            R_TRY(this->CreateSparseStorageWithVerification(
                std::addressof(storage), std::addressof(fs_data_offset),
                ctx != nullptr ? std::addressof(ctx->current_sparse_storage) : nullptr,
                ctx != nullptr ? std::addressof(ctx->sparse_storage_meta_storage) : nullptr,
                ctx != nullptr ? std::addressof(ctx->sparse_layer_info_storage) : nullptr, fs_index,
                out_header_reader->GetAesCtrUpperIv(), sparse_info,
                out_header_reader->GetSparseMetaDataHashDataInfo(),
                out_header_reader->GetSparseMetaHashType()));
        } else {
            // Create the sparse storage.
            R_TRY(this->CreateSparseStorage(
                std::addressof(storage), std::addressof(fs_data_offset),
                ctx != nullptr ? std::addressof(ctx->current_sparse_storage) : nullptr,
                ctx != nullptr ? std::addressof(ctx->sparse_storage_meta_storage) : nullptr,
                fs_index, out_header_reader->GetAesCtrUpperIv(), sparse_info));
        }
    } else {
        // Get the data offsets.
        fs_data_offset = GetFsOffset(*m_reader, fs_index);
        const auto fs_end_offset = GetFsEndOffset(*m_reader, fs_index);

        // Validate that we're within range.
        const auto data_size = fs_end_offset - fs_data_offset;
        R_UNLESS(data_size > 0, ResultInvalidNcaHeader);

        // Create the body substorage.
        R_TRY(this->CreateBodySubStorage(std::addressof(storage), fs_data_offset, data_size));

        // Potentially save the body substorage to our context.
        if (ctx != nullptr) {
            ctx->body_substorage = storage;
        }
    }

    // Process patch layer.
    const auto& patch_info = out_header_reader->GetPatchInfo();
    VirtualFile patch_meta_aes_ctr_ex_meta_storage;
    VirtualFile patch_meta_indirect_meta_storage;
    if (out_header_reader->ExistsPatchMetaHashLayer()) {
        // Check the meta hash type.
        R_UNLESS(out_header_reader->GetPatchMetaHashType() ==
                     NcaFsHeader::MetaDataHashType::HierarchicalIntegrity,
                 ResultRomNcaInvalidPatchMetaDataHashType);

        // Create the patch meta storage.
        R_TRY(this->CreatePatchMetaStorage(
            std::addressof(patch_meta_aes_ctr_ex_meta_storage),
            std::addressof(patch_meta_indirect_meta_storage),
            ctx != nullptr ? std::addressof(ctx->patch_layer_info_storage) : nullptr, storage,
            fs_data_offset, out_header_reader->GetAesCtrUpperIv(), patch_info,
            out_header_reader->GetPatchMetaDataHashDataInfo()));
    }

    if (patch_info.HasAesCtrExTable()) {
        // Check the encryption type.
        ASSERT(out_header_reader->GetEncryptionType() == NcaFsHeader::EncryptionType::None ||
               out_header_reader->GetEncryptionType() == NcaFsHeader::EncryptionType::AesCtrEx ||
               out_header_reader->GetEncryptionType() ==
                   NcaFsHeader::EncryptionType::AesCtrExSkipLayerHash);

        // Create the ex meta storage.
        VirtualFile aes_ctr_ex_storage_meta_storage = patch_meta_aes_ctr_ex_meta_storage;
        if (aes_ctr_ex_storage_meta_storage == nullptr) {
            // If we don't have a meta storage, we must not have a patch meta hash layer.
            ASSERT(!out_header_reader->ExistsPatchMetaHashLayer());

            R_TRY(this->CreateAesCtrExStorageMetaStorage(
                std::addressof(aes_ctr_ex_storage_meta_storage), storage, fs_data_offset,
                out_header_reader->GetEncryptionType(), out_header_reader->GetAesCtrUpperIv(),
                patch_info));
        }

        // Create the ex storage.
        VirtualFile aes_ctr_ex_storage;
        R_TRY(this->CreateAesCtrExStorage(
            std::addressof(aes_ctr_ex_storage),
            ctx != nullptr ? std::addressof(ctx->aes_ctr_ex_storage) : nullptr, std::move(storage),
            aes_ctr_ex_storage_meta_storage, fs_data_offset, out_header_reader->GetAesCtrUpperIv(),
            patch_info));

        // Set the base storage as the ex storage.
        storage = std::move(aes_ctr_ex_storage);

        // Potentially save storages to our context.
        if (ctx != nullptr) {
            ctx->aes_ctr_ex_storage_meta_storage = aes_ctr_ex_storage_meta_storage;
            ctx->aes_ctr_ex_storage_data_storage = storage;
            ctx->fs_data_storage = storage;
        }
    } else {
        // Create the appropriate storage for the encryption type.
        switch (out_header_reader->GetEncryptionType()) {
        case NcaFsHeader::EncryptionType::None:
            // If there's no encryption, use the base storage we made previously.
            break;
        case NcaFsHeader::EncryptionType::AesXts:
            R_TRY(this->CreateAesXtsStorage(std::addressof(storage), std::move(storage),
                                            fs_data_offset));
            break;
        case NcaFsHeader::EncryptionType::AesCtr:
            R_TRY(this->CreateAesCtrStorage(std::addressof(storage), std::move(storage),
                                            fs_data_offset, out_header_reader->GetAesCtrUpperIv(),
                                            AlignmentStorageRequirement::None));
            break;
        case NcaFsHeader::EncryptionType::AesCtrSkipLayerHash: {
            // Create the aes ctr storage.
            VirtualFile aes_ctr_storage;
            R_TRY(this->CreateAesCtrStorage(std::addressof(aes_ctr_storage), storage,
                                            fs_data_offset, out_header_reader->GetAesCtrUpperIv(),
                                            AlignmentStorageRequirement::None));

            // Create region switch storage.
            R_TRY(this->CreateRegionSwitchStorage(std::addressof(storage), out_header_reader,
                                                  std::move(storage), std::move(aes_ctr_storage)));
        } break;
        default:
            R_THROW(ResultInvalidNcaFsHeaderEncryptionType);
        }

        // Potentially save storages to our context.
        if (ctx != nullptr) {
            ctx->fs_data_storage = storage;
        }
    }

    // Process indirect layer.
    if (patch_info.HasIndirectTable()) {
        // Create the indirect meta storage.
        VirtualFile indirect_storage_meta_storage = patch_meta_indirect_meta_storage;
        if (indirect_storage_meta_storage == nullptr) {
            // If we don't have a meta storage, we must not have a patch meta hash layer.
            ASSERT(!out_header_reader->ExistsPatchMetaHashLayer());

            R_TRY(this->CreateIndirectStorageMetaStorage(
                std::addressof(indirect_storage_meta_storage), storage, patch_info));
        }

        // Potentially save the indirect meta storage to our context.
        if (ctx != nullptr) {
            ctx->indirect_storage_meta_storage = indirect_storage_meta_storage;
        }

        // Get the original indirectable storage.
        VirtualFile original_indirectable_storage;
        if (m_original_reader != nullptr && m_original_reader->HasFsInfo(fs_index)) {
            // Create a driver for the original.
            NcaFileSystemDriver original_driver(m_original_reader);

            // Create a header reader for the original.
            NcaFsHeaderReader original_header_reader;
            R_TRY(original_header_reader.Initialize(*m_original_reader, fs_index));

            // Open original indirectable storage.
            R_TRY(original_driver.OpenIndirectableStorageAsOriginal(
                std::addressof(original_indirectable_storage),
                std::addressof(original_header_reader), ctx));
        } else if (ctx != nullptr && ctx->external_original_storage != nullptr) {
            // Use the external original storage.
            original_indirectable_storage = ctx->external_original_storage;
        } else {
            // Allocate a dummy memory storage as original storage.
            original_indirectable_storage = std::make_shared<VectorVfsFile>();
            R_UNLESS(original_indirectable_storage != nullptr,
                     ResultAllocationMemoryFailedAllocateShared);
        }

        // Create the indirect storage.
        VirtualFile indirect_storage;
        R_TRY(this->CreateIndirectStorage(
            std::addressof(indirect_storage),
            ctx != nullptr ? std::addressof(ctx->indirect_storage) : nullptr, std::move(storage),
            std::move(original_indirectable_storage), std::move(indirect_storage_meta_storage),
            patch_info));

        // Set storage as the indirect storage.
        storage = std::move(indirect_storage);
    }

    // Check if we're sparse or requested to skip the integrity layer.
    if (out_header_reader->ExistsSparseLayer() || (ctx != nullptr && ctx->open_raw_storage)) {
        *out = std::move(storage);
        R_SUCCEED();
    }

    // Create the non-raw storage.
    R_RETURN(this->CreateStorageByRawStorage(out, out_header_reader, std::move(storage), ctx));
}

Result NcaFileSystemDriver::CreateStorageByRawStorage(VirtualFile* out,
                                                      const NcaFsHeaderReader* header_reader,
                                                      VirtualFile raw_storage,
                                                      StorageContext* ctx) {
    // Initialize storage as raw storage.
    VirtualFile storage = std::move(raw_storage);

    // Process hash/integrity layer.
    switch (header_reader->GetHashType()) {
    case NcaFsHeader::HashType::HierarchicalSha256Hash:
        R_TRY(this->CreateSha256Storage(std::addressof(storage), std::move(storage),
                                        header_reader->GetHashData().hierarchical_sha256_data));
        break;
    case NcaFsHeader::HashType::HierarchicalIntegrityHash:
        R_TRY(this->CreateIntegrityVerificationStorage(
            std::addressof(storage), std::move(storage),
            header_reader->GetHashData().integrity_meta_info));
        break;
    default:
        R_THROW(ResultInvalidNcaFsHeaderHashType);
    }

    // Process compression layer.
    if (header_reader->ExistsCompressionLayer()) {
        R_TRY(this->CreateCompressedStorage(
            std::addressof(storage),
            ctx != nullptr ? std::addressof(ctx->compressed_storage) : nullptr,
            ctx != nullptr ? std::addressof(ctx->compressed_storage_meta_storage) : nullptr,
            std::move(storage), header_reader->GetCompressionInfo()));
    }

    // Set output storage.
    *out = std::move(storage);
    R_SUCCEED();
}

Result NcaFileSystemDriver::OpenIndirectableStorageAsOriginal(
    VirtualFile* out, const NcaFsHeaderReader* header_reader, StorageContext* ctx) {
    // Get the fs index.
    const auto fs_index = header_reader->GetFsIndex();

    // Declare the storage we're opening.
    VirtualFile storage;

    // Process sparse layer.
    s64 fs_data_offset = 0;
    if (header_reader->ExistsSparseLayer()) {
        // Get the sparse info.
        const auto& sparse_info = header_reader->GetSparseInfo();

        // Create based on whether we have a meta hash layer.
        if (header_reader->ExistsSparseMetaHashLayer()) {
            // Create the sparse storage with verification.
            R_TRY(this->CreateSparseStorageWithVerification(
                std::addressof(storage), std::addressof(fs_data_offset),
                ctx != nullptr ? std::addressof(ctx->original_sparse_storage) : nullptr,
                ctx != nullptr ? std::addressof(ctx->sparse_storage_meta_storage) : nullptr,
                ctx != nullptr ? std::addressof(ctx->sparse_layer_info_storage) : nullptr, fs_index,
                header_reader->GetAesCtrUpperIv(), sparse_info,
                header_reader->GetSparseMetaDataHashDataInfo(),
                header_reader->GetSparseMetaHashType()));
        } else {
            // Create the sparse storage.
            R_TRY(this->CreateSparseStorage(
                std::addressof(storage), std::addressof(fs_data_offset),
                ctx != nullptr ? std::addressof(ctx->original_sparse_storage) : nullptr,
                ctx != nullptr ? std::addressof(ctx->sparse_storage_meta_storage) : nullptr,
                fs_index, header_reader->GetAesCtrUpperIv(), sparse_info));
        }
    } else {
        // Get the data offsets.
        fs_data_offset = GetFsOffset(*m_reader, fs_index);
        const auto fs_end_offset = GetFsEndOffset(*m_reader, fs_index);

        // Validate that we're within range.
        const auto data_size = fs_end_offset - fs_data_offset;
        R_UNLESS(data_size > 0, ResultInvalidNcaHeader);

        // Create the body substorage.
        R_TRY(this->CreateBodySubStorage(std::addressof(storage), fs_data_offset, data_size));
    }

    // Create the appropriate storage for the encryption type.
    switch (header_reader->GetEncryptionType()) {
    case NcaFsHeader::EncryptionType::None:
        // If there's no encryption, use the base storage we made previously.
        break;
    case NcaFsHeader::EncryptionType::AesXts:
        R_TRY(
            this->CreateAesXtsStorage(std::addressof(storage), std::move(storage), fs_data_offset));
        break;
    case NcaFsHeader::EncryptionType::AesCtr:
        R_TRY(this->CreateAesCtrStorage(std::addressof(storage), std::move(storage), fs_data_offset,
                                        header_reader->GetAesCtrUpperIv(),
                                        AlignmentStorageRequirement::CacheBlockSize));
        break;
    default:
        R_THROW(ResultInvalidNcaFsHeaderEncryptionType);
    }

    // Set output storage.
    *out = std::move(storage);
    R_SUCCEED();
}

Result NcaFileSystemDriver::CreateBodySubStorage(VirtualFile* out, s64 offset, s64 size) {
    // Create the body storage.
    auto body_storage =
        std::make_shared<SharedNcaBodyStorage>(m_reader->GetSharedBodyStorage(), m_reader);
    R_UNLESS(body_storage != nullptr, ResultAllocationMemoryFailedAllocateShared);

    // Get the body storage size.
    s64 body_size = body_storage->GetSize();

    // Check that we're within range.
    R_UNLESS(offset + size <= body_size, ResultNcaBaseStorageOutOfRangeB);

    // Create substorage.
    auto body_substorage = std::make_shared<OffsetVfsFile>(std::move(body_storage), size, offset);
    R_UNLESS(body_substorage != nullptr, ResultAllocationMemoryFailedAllocateShared);

    // Set the output storage.
    *out = std::move(body_substorage);
    R_SUCCEED();
}

Result NcaFileSystemDriver::CreateAesCtrStorage(
    VirtualFile* out, VirtualFile base_storage, s64 offset, const NcaAesCtrUpperIv& upper_iv,
    AlignmentStorageRequirement alignment_storage_requirement) {
    // Check pre-conditions.
    ASSERT(out != nullptr);
    ASSERT(base_storage != nullptr);

    // Create the iv.
    std::array<u8, AesCtrStorage::IvSize> iv{};
    AesCtrStorage::MakeIv(iv.data(), sizeof(iv), upper_iv.value, offset);

    // Create the ctr storage.
    VirtualFile aes_ctr_storage;
    if (m_reader->HasExternalDecryptionKey()) {
        aes_ctr_storage = std::make_shared<AesCtrStorage>(
            std::move(base_storage), m_reader->GetExternalDecryptionKey(), AesCtrStorage::KeySize,
            iv.data(), AesCtrStorage::IvSize);
        R_UNLESS(aes_ctr_storage != nullptr, ResultAllocationMemoryFailedAllocateShared);
    } else {
        // Create software decryption storage.
        auto sw_storage = std::make_shared<AesCtrStorage>(
            base_storage, m_reader->GetDecryptionKey(NcaHeader::DecryptionKey_AesCtr),
            AesCtrStorage::KeySize, iv.data(), AesCtrStorage::IvSize);
        R_UNLESS(sw_storage != nullptr, ResultAllocationMemoryFailedAllocateShared);

        aes_ctr_storage = std::move(sw_storage);
    }

    // Create alignment matching storage.
    auto aligned_storage = std::make_shared<AlignmentMatchingStorage<NcaHeader::CtrBlockSize, 1>>(
        std::move(aes_ctr_storage));
    R_UNLESS(aligned_storage != nullptr, ResultAllocationMemoryFailedAllocateShared);

    // Set the out storage.
    *out = std::move(aligned_storage);
    R_SUCCEED();
}

Result NcaFileSystemDriver::CreateAesXtsStorage(VirtualFile* out, VirtualFile base_storage,
                                                s64 offset) {
    // Check pre-conditions.
    ASSERT(out != nullptr);
    ASSERT(base_storage != nullptr);

    // Create the iv.
    std::array<u8, AesXtsStorage::IvSize> iv{};
    AesXtsStorage::MakeAesXtsIv(iv.data(), sizeof(iv), offset, NcaHeader::XtsBlockSize);

    // Make the aes xts storage.
    const auto* const key1 = m_reader->GetDecryptionKey(NcaHeader::DecryptionKey_AesXts1);
    const auto* const key2 = m_reader->GetDecryptionKey(NcaHeader::DecryptionKey_AesXts2);
    auto xts_storage =
        std::make_shared<AesXtsStorage>(std::move(base_storage), key1, key2, AesXtsStorage::KeySize,
                                        iv.data(), AesXtsStorage::IvSize, NcaHeader::XtsBlockSize);
    R_UNLESS(xts_storage != nullptr, ResultAllocationMemoryFailedAllocateShared);

    // Create alignment matching storage.
    auto aligned_storage = std::make_shared<AlignmentMatchingStorage<NcaHeader::XtsBlockSize, 1>>(
        std::move(xts_storage));
    R_UNLESS(aligned_storage != nullptr, ResultAllocationMemoryFailedAllocateShared);

    // Set the out storage.
    *out = std::move(xts_storage);
    R_SUCCEED();
}

Result NcaFileSystemDriver::CreateSparseStorageMetaStorage(VirtualFile* out,
                                                           VirtualFile base_storage, s64 offset,
                                                           const NcaAesCtrUpperIv& upper_iv,
                                                           const NcaSparseInfo& sparse_info) {
    // Validate preconditions.
    ASSERT(out != nullptr);
    ASSERT(base_storage != nullptr);

    // Get the base storage size.
    s64 base_size = base_storage->GetSize();

    // Get the meta extents.
    const auto meta_offset = sparse_info.bucket.offset;
    const auto meta_size = sparse_info.bucket.size;
    R_UNLESS(meta_offset + meta_size - offset <= base_size, ResultNcaBaseStorageOutOfRangeB);

    // Create the encrypted storage.
    auto enc_storage =
        std::make_shared<OffsetVfsFile>(std::move(base_storage), meta_size, meta_offset);
    R_UNLESS(enc_storage != nullptr, ResultAllocationMemoryFailedAllocateShared);

    // Create the decrypted storage.
    VirtualFile decrypted_storage;
    R_TRY(this->CreateAesCtrStorage(std::addressof(decrypted_storage), std::move(enc_storage),
                                    offset + meta_offset, sparse_info.MakeAesCtrUpperIv(upper_iv),
                                    AlignmentStorageRequirement::None));

    // Create buffered storage.
    std::vector<u8> meta_data(meta_size);
    decrypted_storage->Read(meta_data.data(), meta_size, 0);

    auto buffered_storage = std::make_shared<VectorVfsFile>(std::move(meta_data));
    R_UNLESS(buffered_storage != nullptr, ResultAllocationMemoryFailedAllocateShared);

    // Set the output.
    *out = std::move(buffered_storage);
    R_SUCCEED();
}

Result NcaFileSystemDriver::CreateSparseStorageCore(std::shared_ptr<SparseStorage>* out,
                                                    VirtualFile base_storage, s64 base_size,
                                                    VirtualFile meta_storage,
                                                    const NcaSparseInfo& sparse_info,
                                                    bool external_info) {
    // Validate preconditions.
    ASSERT(out != nullptr);
    ASSERT(base_storage != nullptr);
    ASSERT(meta_storage != nullptr);

    // Read and verify the bucket tree header.
    BucketTree::Header header;
    std::memcpy(std::addressof(header), sparse_info.bucket.header.data(), sizeof(header));
    R_TRY(header.Verify());

    // Determine storage extents.
    const auto node_offset = 0;
    const auto node_size = SparseStorage::QueryNodeStorageSize(header.entry_count);
    const auto entry_offset = node_offset + node_size;
    const auto entry_size = SparseStorage::QueryEntryStorageSize(header.entry_count);

    // Create the sparse storage.
    auto sparse_storage = std::make_shared<SparseStorage>();
    R_UNLESS(sparse_storage != nullptr, ResultAllocationMemoryFailedAllocateShared);

    // Sanity check that we can be doing this.
    ASSERT(header.entry_count != 0);

    // Initialize the sparse storage.
    R_TRY(sparse_storage->Initialize(
        std::make_shared<OffsetVfsFile>(meta_storage, node_size, node_offset),
        std::make_shared<OffsetVfsFile>(meta_storage, entry_size, entry_offset),
        header.entry_count));

    // If not external, set the data storage.
    if (!external_info) {
        sparse_storage->SetDataStorage(
            std::make_shared<OffsetVfsFile>(std::move(base_storage), base_size, 0));
    }

    // Set the output.
    *out = std::move(sparse_storage);
    R_SUCCEED();
}

Result NcaFileSystemDriver::CreateSparseStorage(VirtualFile* out, s64* out_fs_data_offset,
                                                std::shared_ptr<SparseStorage>* out_sparse_storage,
                                                VirtualFile* out_meta_storage, s32 index,
                                                const NcaAesCtrUpperIv& upper_iv,
                                                const NcaSparseInfo& sparse_info) {
    // Validate preconditions.
    ASSERT(out != nullptr);
    ASSERT(out_fs_data_offset != nullptr);

    // Check the sparse info generation.
    R_UNLESS(sparse_info.generation != 0, ResultInvalidNcaHeader);

    // Read and verify the bucket tree header.
    BucketTree::Header header;
    std::memcpy(std::addressof(header), sparse_info.bucket.header.data(), sizeof(header));
    R_TRY(header.Verify());

    // Determine the storage extents.
    const auto fs_offset = GetFsOffset(*m_reader, index);
    const auto fs_end_offset = GetFsEndOffset(*m_reader, index);
    const auto fs_size = fs_end_offset - fs_offset;

    // Create the sparse storage.
    std::shared_ptr<SparseStorage> sparse_storage;
    if (header.entry_count != 0) {
        // Create the body substorage.
        VirtualFile body_substorage;
        R_TRY(this->CreateBodySubStorage(std::addressof(body_substorage),
                                         sparse_info.physical_offset,
                                         sparse_info.GetPhysicalSize()));

        // Create the meta storage.
        VirtualFile meta_storage;
        R_TRY(this->CreateSparseStorageMetaStorage(std::addressof(meta_storage), body_substorage,
                                                   sparse_info.physical_offset, upper_iv,
                                                   sparse_info));

        // Potentially set the output meta storage.
        if (out_meta_storage != nullptr) {
            *out_meta_storage = meta_storage;
        }

        // Create the sparse storage.
        R_TRY(this->CreateSparseStorageCore(std::addressof(sparse_storage), body_substorage,
                                            sparse_info.GetPhysicalSize(), std::move(meta_storage),
                                            sparse_info, false));
    } else {
        // If there are no entries, there's nothing to actually do.
        sparse_storage = std::make_shared<SparseStorage>();
        R_UNLESS(sparse_storage != nullptr, ResultAllocationMemoryFailedAllocateShared);

        sparse_storage->Initialize(fs_size);
    }

    // Potentially set the output sparse storage.
    if (out_sparse_storage != nullptr) {
        *out_sparse_storage = sparse_storage;
    }

    // Set the output fs data offset.
    *out_fs_data_offset = fs_offset;

    // Set the output storage.
    *out = std::move(sparse_storage);
    R_SUCCEED();
}

Result NcaFileSystemDriver::CreateSparseStorageMetaStorageWithVerification(
    VirtualFile* out, VirtualFile* out_layer_info_storage, VirtualFile base_storage, s64 offset,
    const NcaAesCtrUpperIv& upper_iv, const NcaSparseInfo& sparse_info,
    const NcaMetaDataHashDataInfo& meta_data_hash_data_info) {
    // Validate preconditions.
    ASSERT(out != nullptr);
    ASSERT(base_storage != nullptr);

    // Get the base storage size.
    s64 base_size = base_storage->GetSize();

    // Get the meta extents.
    const auto meta_offset = sparse_info.bucket.offset;
    const auto meta_size = sparse_info.bucket.size;
    R_UNLESS(meta_offset + meta_size - offset <= base_size, ResultNcaBaseStorageOutOfRangeB);

    // Get the meta data hash data extents.
    const s64 meta_data_hash_data_offset = meta_data_hash_data_info.offset;
    const s64 meta_data_hash_data_size =
        Common::AlignUp<s64>(meta_data_hash_data_info.size, NcaHeader::CtrBlockSize);
    R_UNLESS(meta_data_hash_data_offset + meta_data_hash_data_size <= base_size,
             ResultNcaBaseStorageOutOfRangeB);

    // Check that the meta is before the hash data.
    R_UNLESS(meta_offset + meta_size <= meta_data_hash_data_offset,
             ResultRomNcaInvalidSparseMetaDataHashDataOffset);

    // Check that offsets are appropriately aligned.
    R_UNLESS(Common::IsAligned<s64>(meta_data_hash_data_offset, NcaHeader::CtrBlockSize),
             ResultRomNcaInvalidSparseMetaDataHashDataOffset);
    R_UNLESS(Common::IsAligned<s64>(meta_offset, NcaHeader::CtrBlockSize),
             ResultInvalidNcaFsHeader);

    // Create the meta storage.
    auto enc_storage = std::make_shared<OffsetVfsFile>(
        std::move(base_storage),
        meta_data_hash_data_offset + meta_data_hash_data_size - meta_offset, meta_offset);
    R_UNLESS(enc_storage != nullptr, ResultAllocationMemoryFailedAllocateShared);

    // Create the decrypted storage.
    VirtualFile decrypted_storage;
    R_TRY(this->CreateAesCtrStorage(std::addressof(decrypted_storage), std::move(enc_storage),
                                    offset + meta_offset, sparse_info.MakeAesCtrUpperIv(upper_iv),
                                    AlignmentStorageRequirement::None));

    // Create the verification storage.
    VirtualFile integrity_storage;
    Result rc = this->CreateIntegrityVerificationStorageForMeta(
        std::addressof(integrity_storage), out_layer_info_storage, std::move(decrypted_storage),
        meta_offset, meta_data_hash_data_info);
    if (rc == ResultInvalidNcaMetaDataHashDataSize) {
        R_THROW(ResultRomNcaInvalidSparseMetaDataHashDataSize);
    }
    if (rc == ResultInvalidNcaMetaDataHashDataHash) {
        R_THROW(ResultRomNcaInvalidSparseMetaDataHashDataHash);
    }
    R_TRY(rc);

    // Create the meta storage.
    auto meta_storage = std::make_shared<OffsetVfsFile>(std::move(integrity_storage), meta_size, 0);
    R_UNLESS(meta_storage != nullptr, ResultAllocationMemoryFailedAllocateShared);

    // Set the output.
    *out = std::move(meta_storage);
    R_SUCCEED();
}

Result NcaFileSystemDriver::CreateSparseStorageWithVerification(
    VirtualFile* out, s64* out_fs_data_offset, std::shared_ptr<SparseStorage>* out_sparse_storage,
    VirtualFile* out_meta_storage, VirtualFile* out_layer_info_storage, s32 index,
    const NcaAesCtrUpperIv& upper_iv, const NcaSparseInfo& sparse_info,
    const NcaMetaDataHashDataInfo& meta_data_hash_data_info,
    NcaFsHeader::MetaDataHashType meta_data_hash_type) {
    // Validate preconditions.
    ASSERT(out != nullptr);
    ASSERT(out_fs_data_offset != nullptr);

    // Check the sparse info generation.
    R_UNLESS(sparse_info.generation != 0, ResultInvalidNcaHeader);

    // Read and verify the bucket tree header.
    BucketTree::Header header;
    std::memcpy(std::addressof(header), sparse_info.bucket.header.data(), sizeof(header));
    R_TRY(header.Verify());

    // Determine the storage extents.
    const auto fs_offset = GetFsOffset(*m_reader, index);
    const auto fs_end_offset = GetFsEndOffset(*m_reader, index);
    const auto fs_size = fs_end_offset - fs_offset;

    // Create the sparse storage.
    std::shared_ptr<SparseStorage> sparse_storage;
    if (header.entry_count != 0) {
        // Create the body substorage.
        VirtualFile body_substorage;
        R_TRY(this->CreateBodySubStorage(
            std::addressof(body_substorage), sparse_info.physical_offset,
            Common::AlignUp<s64>(static_cast<s64>(meta_data_hash_data_info.offset) +
                                     static_cast<s64>(meta_data_hash_data_info.size),
                                 NcaHeader::CtrBlockSize)));

        // Check the meta data hash type.
        R_UNLESS(meta_data_hash_type == NcaFsHeader::MetaDataHashType::HierarchicalIntegrity,
                 ResultRomNcaInvalidSparseMetaDataHashType);

        // Create the meta storage.
        VirtualFile meta_storage;
        R_TRY(this->CreateSparseStorageMetaStorageWithVerification(
            std::addressof(meta_storage), out_layer_info_storage, body_substorage,
            sparse_info.physical_offset, upper_iv, sparse_info, meta_data_hash_data_info));

        // Potentially set the output meta storage.
        if (out_meta_storage != nullptr) {
            *out_meta_storage = meta_storage;
        }

        // Create the sparse storage.
        R_TRY(this->CreateSparseStorageCore(std::addressof(sparse_storage), body_substorage,
                                            sparse_info.GetPhysicalSize(), std::move(meta_storage),
                                            sparse_info, false));
    } else {
        // If there are no entries, there's nothing to actually do.
        sparse_storage = std::make_shared<SparseStorage>();
        R_UNLESS(sparse_storage != nullptr, ResultAllocationMemoryFailedAllocateShared);

        sparse_storage->Initialize(fs_size);
    }

    // Potentially set the output sparse storage.
    if (out_sparse_storage != nullptr) {
        *out_sparse_storage = sparse_storage;
    }

    // Set the output fs data offset.
    *out_fs_data_offset = fs_offset;

    // Set the output storage.
    *out = std::move(sparse_storage);
    R_SUCCEED();
}

Result NcaFileSystemDriver::CreateAesCtrExStorageMetaStorage(
    VirtualFile* out, VirtualFile base_storage, s64 offset,
    NcaFsHeader::EncryptionType encryption_type, const NcaAesCtrUpperIv& upper_iv,
    const NcaPatchInfo& patch_info) {
    // Validate preconditions.
    ASSERT(out != nullptr);
    ASSERT(base_storage != nullptr);
    ASSERT(encryption_type == NcaFsHeader::EncryptionType::None ||
           encryption_type == NcaFsHeader::EncryptionType::AesCtrEx ||
           encryption_type == NcaFsHeader::EncryptionType::AesCtrExSkipLayerHash);
    ASSERT(patch_info.HasAesCtrExTable());

    // Validate patch info extents.
    R_UNLESS(patch_info.indirect_size > 0, ResultInvalidNcaPatchInfoIndirectSize);
    R_UNLESS(patch_info.aes_ctr_ex_size > 0, ResultInvalidNcaPatchInfoAesCtrExSize);
    R_UNLESS(patch_info.indirect_size + patch_info.indirect_offset <= patch_info.aes_ctr_ex_offset,
             ResultInvalidNcaPatchInfoAesCtrExOffset);

    // Get the base storage size.
    s64 base_size = base_storage->GetSize();

    // Get and validate the meta extents.
    const s64 meta_offset = patch_info.aes_ctr_ex_offset;
    const s64 meta_size =
        Common::AlignUp(static_cast<s64>(patch_info.aes_ctr_ex_size), NcaHeader::XtsBlockSize);
    R_UNLESS(meta_offset + meta_size <= base_size, ResultNcaBaseStorageOutOfRangeB);

    // Create the encrypted storage.
    auto enc_storage =
        std::make_shared<OffsetVfsFile>(std::move(base_storage), meta_size, meta_offset);
    R_UNLESS(enc_storage != nullptr, ResultAllocationMemoryFailedAllocateShared);

    // Create the decrypted storage.
    VirtualFile decrypted_storage;
    if (encryption_type != NcaFsHeader::EncryptionType::None) {
        R_TRY(this->CreateAesCtrStorage(std::addressof(decrypted_storage), std::move(enc_storage),
                                        offset + meta_offset, upper_iv,
                                        AlignmentStorageRequirement::None));
    } else {
        // If encryption type is none, don't do any decryption.
        decrypted_storage = std::move(enc_storage);
    }

    // Create meta storage.
    auto meta_storage = std::make_shared<OffsetVfsFile>(decrypted_storage, meta_size, 0);
    R_UNLESS(meta_storage != nullptr, ResultAllocationMemoryFailedAllocateShared);

    // Create buffered storage.
    std::vector<u8> meta_data(meta_size);
    meta_storage->Read(meta_data.data(), meta_size, 0);

    auto buffered_storage = std::make_shared<VectorVfsFile>(std::move(meta_data));
    R_UNLESS(buffered_storage != nullptr, ResultAllocationMemoryFailedAllocateShared);

    // Set the output.
    *out = std::move(buffered_storage);
    R_SUCCEED();
}

Result NcaFileSystemDriver::CreateAesCtrExStorage(
    VirtualFile* out, std::shared_ptr<AesCtrCounterExtendedStorage>* out_ext,
    VirtualFile base_storage, VirtualFile meta_storage, s64 counter_offset,
    const NcaAesCtrUpperIv& upper_iv, const NcaPatchInfo& patch_info) {
    // Validate pre-conditions.
    ASSERT(out != nullptr);
    ASSERT(base_storage != nullptr);
    ASSERT(meta_storage != nullptr);
    ASSERT(patch_info.HasAesCtrExTable());

    // Read the bucket tree header.
    BucketTree::Header header;
    std::memcpy(std::addressof(header), patch_info.aes_ctr_ex_header.data(), sizeof(header));
    R_TRY(header.Verify());

    // Determine the bucket extents.
    const auto entry_count = header.entry_count;
    const s64 data_offset = 0;
    const s64 data_size = patch_info.aes_ctr_ex_offset;
    const s64 node_offset = 0;
    const s64 node_size = AesCtrCounterExtendedStorage::QueryNodeStorageSize(entry_count);
    const s64 entry_offset = node_offset + node_size;
    const s64 entry_size = AesCtrCounterExtendedStorage::QueryEntryStorageSize(entry_count);

    // Create bucket storages.
    auto data_storage =
        std::make_shared<OffsetVfsFile>(std::move(base_storage), data_size, data_offset);
    auto node_storage = std::make_shared<OffsetVfsFile>(meta_storage, node_size, node_offset);
    auto entry_storage = std::make_shared<OffsetVfsFile>(meta_storage, entry_size, entry_offset);

    // Get the secure value.
    const auto secure_value = upper_iv.part.secure_value;

    // Create the aes ctr ex storage.
    VirtualFile aes_ctr_ex_storage;
    if (m_reader->HasExternalDecryptionKey()) {
        // Create the decryptor.
        std::unique_ptr<AesCtrCounterExtendedStorage::IDecryptor> decryptor;
        R_TRY(AesCtrCounterExtendedStorage::CreateSoftwareDecryptor(std::addressof(decryptor)));

        // Create the aes ctr ex storage.
        auto impl_storage = std::make_shared<AesCtrCounterExtendedStorage>();
        R_UNLESS(impl_storage != nullptr, ResultAllocationMemoryFailedAllocateShared);

        // Initialize the aes ctr ex storage.
        R_TRY(impl_storage->Initialize(m_reader->GetExternalDecryptionKey(), AesCtrStorage::KeySize,
                                       secure_value, counter_offset, data_storage, node_storage,
                                       entry_storage, entry_count, std::move(decryptor)));

        // Potentially set the output implementation storage.
        if (out_ext != nullptr) {
            *out_ext = impl_storage;
        }

        // Set the implementation storage.
        aes_ctr_ex_storage = std::move(impl_storage);
    } else {
        // Create the software decryptor.
        std::unique_ptr<AesCtrCounterExtendedStorage::IDecryptor> sw_decryptor;
        R_TRY(AesCtrCounterExtendedStorage::CreateSoftwareDecryptor(std::addressof(sw_decryptor)));

        // Make the software storage.
        auto sw_storage = std::make_shared<AesCtrCounterExtendedStorage>();
        R_UNLESS(sw_storage != nullptr, ResultAllocationMemoryFailedAllocateShared);

        // Initialize the software storage.
        R_TRY(sw_storage->Initialize(m_reader->GetDecryptionKey(NcaHeader::DecryptionKey_AesCtr),
                                     AesCtrStorage::KeySize, secure_value, counter_offset,
                                     data_storage, node_storage, entry_storage, entry_count,
                                     std::move(sw_decryptor)));

        // Potentially set the output implementation storage.
        if (out_ext != nullptr) {
            *out_ext = sw_storage;
        }

        // Set the implementation storage.
        aes_ctr_ex_storage = std::move(sw_storage);
    }

    // Create an alignment-matching storage.
    using AlignedStorage = AlignmentMatchingStorage<NcaHeader::CtrBlockSize, 1>;
    auto aligned_storage = std::make_shared<AlignedStorage>(std::move(aes_ctr_ex_storage));
    R_UNLESS(aligned_storage != nullptr, ResultAllocationMemoryFailedAllocateShared);

    // Set the output.
    *out = std::move(aligned_storage);
    R_SUCCEED();
}

Result NcaFileSystemDriver::CreateIndirectStorageMetaStorage(VirtualFile* out,
                                                             VirtualFile base_storage,
                                                             const NcaPatchInfo& patch_info) {
    // Validate preconditions.
    ASSERT(out != nullptr);
    ASSERT(base_storage != nullptr);
    ASSERT(patch_info.HasIndirectTable());

    // Get the base storage size.
    s64 base_size = base_storage->GetSize();

    // Check that we're within range.
    R_UNLESS(patch_info.indirect_offset + patch_info.indirect_size <= base_size,
             ResultNcaBaseStorageOutOfRangeE);

    // Create the meta storage.
    auto meta_storage = std::make_shared<OffsetVfsFile>(base_storage, patch_info.indirect_size,
                                                        patch_info.indirect_offset);
    R_UNLESS(meta_storage != nullptr, ResultAllocationMemoryFailedAllocateShared);

    // Create buffered storage.
    std::vector<u8> meta_data(patch_info.indirect_size);
    meta_storage->Read(meta_data.data(), patch_info.indirect_size, 0);

    auto buffered_storage = std::make_shared<VectorVfsFile>(std::move(meta_data));
    R_UNLESS(buffered_storage != nullptr, ResultAllocationMemoryFailedAllocateShared);

    // Set the output.
    *out = std::move(buffered_storage);
    R_SUCCEED();
}

Result NcaFileSystemDriver::CreateIndirectStorage(
    VirtualFile* out, std::shared_ptr<IndirectStorage>* out_ind, VirtualFile base_storage,
    VirtualFile original_data_storage, VirtualFile meta_storage, const NcaPatchInfo& patch_info) {
    // Validate preconditions.
    ASSERT(out != nullptr);
    ASSERT(base_storage != nullptr);
    ASSERT(meta_storage != nullptr);
    ASSERT(patch_info.HasIndirectTable());

    // Read the bucket tree header.
    BucketTree::Header header;
    std::memcpy(std::addressof(header), patch_info.indirect_header.data(), sizeof(header));
    R_TRY(header.Verify());

    // Determine the storage sizes.
    const auto node_size = IndirectStorage::QueryNodeStorageSize(header.entry_count);
    const auto entry_size = IndirectStorage::QueryEntryStorageSize(header.entry_count);
    R_UNLESS(node_size + entry_size <= patch_info.indirect_size,
             ResultInvalidNcaIndirectStorageOutOfRange);

    // Get the indirect data size.
    const s64 indirect_data_size = patch_info.indirect_offset;
    ASSERT(Common::IsAligned(indirect_data_size, NcaHeader::XtsBlockSize));

    // Create the indirect data storage.
    auto indirect_data_storage =
        std::make_shared<OffsetVfsFile>(base_storage, indirect_data_size, 0);
    R_UNLESS(indirect_data_storage != nullptr, ResultAllocationMemoryFailedAllocateShared);

    // Create the indirect storage.
    auto indirect_storage = std::make_shared<IndirectStorage>();
    R_UNLESS(indirect_storage != nullptr, ResultAllocationMemoryFailedAllocateShared);

    // Initialize the indirect storage.
    R_TRY(indirect_storage->Initialize(
        std::make_shared<OffsetVfsFile>(meta_storage, node_size, 0),
        std::make_shared<OffsetVfsFile>(meta_storage, entry_size, node_size), header.entry_count));

    // Get the original data size.
    s64 original_data_size = original_data_storage->GetSize();

    // Set the indirect storages.
    indirect_storage->SetStorage(
        0, std::make_shared<OffsetVfsFile>(original_data_storage, original_data_size, 0));
    indirect_storage->SetStorage(
        1, std::make_shared<OffsetVfsFile>(indirect_data_storage, indirect_data_size, 0));

    // If necessary, set the output indirect storage.
    if (out_ind != nullptr) {
        *out_ind = indirect_storage;
    }

    // Set the output.
    *out = std::move(indirect_storage);
    R_SUCCEED();
}

Result NcaFileSystemDriver::CreatePatchMetaStorage(
    VirtualFile* out_aes_ctr_ex_meta, VirtualFile* out_indirect_meta,
    VirtualFile* out_layer_info_storage, VirtualFile base_storage, s64 offset,
    const NcaAesCtrUpperIv& upper_iv, const NcaPatchInfo& patch_info,
    const NcaMetaDataHashDataInfo& meta_data_hash_data_info) {
    // Validate preconditions.
    ASSERT(out_aes_ctr_ex_meta != nullptr);
    ASSERT(out_indirect_meta != nullptr);
    ASSERT(base_storage != nullptr);
    ASSERT(patch_info.HasAesCtrExTable());
    ASSERT(patch_info.HasIndirectTable());
    ASSERT(Common::IsAligned<s64>(patch_info.aes_ctr_ex_size, NcaHeader::XtsBlockSize));

    // Validate patch info extents.
    R_UNLESS(patch_info.indirect_size > 0, ResultInvalidNcaPatchInfoIndirectSize);
    R_UNLESS(patch_info.aes_ctr_ex_size >= 0, ResultInvalidNcaPatchInfoAesCtrExSize);
    R_UNLESS(patch_info.indirect_size + patch_info.indirect_offset <= patch_info.aes_ctr_ex_offset,
             ResultInvalidNcaPatchInfoAesCtrExOffset);
    R_UNLESS(patch_info.aes_ctr_ex_offset + patch_info.aes_ctr_ex_size <=
                 meta_data_hash_data_info.offset,
             ResultRomNcaInvalidPatchMetaDataHashDataOffset);

    // Get the base storage size.
    s64 base_size = base_storage->GetSize();

    // Check that extents remain within range.
    R_UNLESS(patch_info.indirect_offset + patch_info.indirect_size <= base_size,
             ResultNcaBaseStorageOutOfRangeE);
    R_UNLESS(patch_info.aes_ctr_ex_offset + patch_info.aes_ctr_ex_size <= base_size,
             ResultNcaBaseStorageOutOfRangeB);

    // Check that metadata hash data extents remain within range.
    const s64 meta_data_hash_data_offset = meta_data_hash_data_info.offset;
    const s64 meta_data_hash_data_size =
        Common::AlignUp<s64>(meta_data_hash_data_info.size, NcaHeader::CtrBlockSize);
    R_UNLESS(meta_data_hash_data_offset + meta_data_hash_data_size <= base_size,
             ResultNcaBaseStorageOutOfRangeB);

    // Create the encrypted storage.
    auto enc_storage = std::make_shared<OffsetVfsFile>(
        std::move(base_storage),
        meta_data_hash_data_offset + meta_data_hash_data_size - patch_info.indirect_offset,
        patch_info.indirect_offset);
    R_UNLESS(enc_storage != nullptr, ResultAllocationMemoryFailedAllocateShared);

    // Create the decrypted storage.
    VirtualFile decrypted_storage;
    R_TRY(this->CreateAesCtrStorage(std::addressof(decrypted_storage), std::move(enc_storage),
                                    offset + patch_info.indirect_offset, upper_iv,
                                    AlignmentStorageRequirement::None));

    // Create the verification storage.
    VirtualFile integrity_storage;
    Result rc = this->CreateIntegrityVerificationStorageForMeta(
        std::addressof(integrity_storage), out_layer_info_storage, std::move(decrypted_storage),
        patch_info.indirect_offset, meta_data_hash_data_info);
    if (rc == ResultInvalidNcaMetaDataHashDataSize) {
        R_THROW(ResultRomNcaInvalidPatchMetaDataHashDataSize);
    }
    if (rc == ResultInvalidNcaMetaDataHashDataHash) {
        R_THROW(ResultRomNcaInvalidPatchMetaDataHashDataHash);
    }
    R_TRY(rc);

    // Create the indirect meta storage.
    auto indirect_meta_storage =
        std::make_shared<OffsetVfsFile>(integrity_storage, patch_info.indirect_size,
                                        patch_info.indirect_offset - patch_info.indirect_offset);
    R_UNLESS(indirect_meta_storage != nullptr, ResultAllocationMemoryFailedAllocateShared);

    // Create the aes ctr ex meta storage.
    auto aes_ctr_ex_meta_storage =
        std::make_shared<OffsetVfsFile>(integrity_storage, patch_info.aes_ctr_ex_size,
                                        patch_info.aes_ctr_ex_offset - patch_info.indirect_offset);
    R_UNLESS(aes_ctr_ex_meta_storage != nullptr, ResultAllocationMemoryFailedAllocateShared);

    // Set the output.
    *out_aes_ctr_ex_meta = std::move(aes_ctr_ex_meta_storage);
    *out_indirect_meta = std::move(indirect_meta_storage);
    R_SUCCEED();
}

Result NcaFileSystemDriver::CreateSha256Storage(
    VirtualFile* out, VirtualFile base_storage,
    const NcaFsHeader::HashData::HierarchicalSha256Data& hash_data) {
    // Validate preconditions.
    ASSERT(out != nullptr);
    ASSERT(base_storage != nullptr);

    // Define storage types.
    using VerificationStorage = HierarchicalSha256Storage;

    // Validate the hash data.
    R_UNLESS(Common::IsPowerOfTwo(hash_data.hash_block_size),
             ResultInvalidHierarchicalSha256BlockSize);
    R_UNLESS(hash_data.hash_layer_count == VerificationStorage::LayerCount - 1,
             ResultInvalidHierarchicalSha256LayerCount);

    // Get the regions.
    const auto& hash_region = hash_data.hash_layer_region[0];
    const auto& data_region = hash_data.hash_layer_region[1];

    // Determine buffer sizes.
    constexpr s32 CacheBlockCount = 2;
    const auto hash_buffer_size = static_cast<size_t>(hash_region.size);
    const auto cache_buffer_size = CacheBlockCount * hash_data.hash_block_size;
    const auto total_buffer_size = hash_buffer_size + cache_buffer_size;

    // Make a buffer holder storage.
    auto buffer_hold_storage = std::make_shared<MemoryResourceBufferHoldStorage>(
        std::move(base_storage), total_buffer_size);
    R_UNLESS(buffer_hold_storage != nullptr, ResultAllocationMemoryFailedAllocateShared);
    R_UNLESS(buffer_hold_storage->IsValid(), ResultAllocationMemoryFailedInNcaFileSystemDriverI);

    // Get storage size.
    s64 base_size = buffer_hold_storage->GetSize();

    // Check that we're within range.
    R_UNLESS(hash_region.offset + hash_region.size <= base_size, ResultNcaBaseStorageOutOfRangeC);
    R_UNLESS(data_region.offset + data_region.size <= base_size, ResultNcaBaseStorageOutOfRangeC);

    // Create the master hash storage.
    auto master_hash_storage =
        std::make_shared<ArrayVfsFile<sizeof(Hash)>>(hash_data.fs_data_master_hash.value);

    // Make the verification storage.
    auto verification_storage = std::make_shared<VerificationStorage>();
    R_UNLESS(verification_storage != nullptr, ResultAllocationMemoryFailedAllocateShared);

    // Make layer storages.
    std::array<VirtualFile, VerificationStorage::LayerCount> layer_storages{
        std::make_shared<OffsetVfsFile>(master_hash_storage, sizeof(Hash), 0),
        std::make_shared<OffsetVfsFile>(buffer_hold_storage, hash_region.size, hash_region.offset),
        std::make_shared<OffsetVfsFile>(buffer_hold_storage, data_region.size, data_region.offset),
    };

    // Initialize the verification storage.
    R_TRY(verification_storage->Initialize(layer_storages.data(), VerificationStorage::LayerCount,
                                           hash_data.hash_block_size,
                                           buffer_hold_storage->GetBuffer(), hash_buffer_size));

    // Set the output.
    *out = std::move(verification_storage);
    R_SUCCEED();
}

Result NcaFileSystemDriver::CreateIntegrityVerificationStorage(
    VirtualFile* out, VirtualFile base_storage,
    const NcaFsHeader::HashData::IntegrityMetaInfo& meta_info) {
    R_RETURN(this->CreateIntegrityVerificationStorageImpl(
        out, base_storage, meta_info, 0, IntegrityDataCacheCount, IntegrityHashCacheCount,
        HierarchicalIntegrityVerificationStorage::GetDefaultDataCacheBufferLevel(
            meta_info.level_hash_info.max_layers)));
}

Result NcaFileSystemDriver::CreateIntegrityVerificationStorageForMeta(
    VirtualFile* out, VirtualFile* out_layer_info_storage, VirtualFile base_storage, s64 offset,
    const NcaMetaDataHashDataInfo& meta_data_hash_data_info) {
    // Validate preconditions.
    ASSERT(out != nullptr);

    // Check the meta data hash data size.
    R_UNLESS(meta_data_hash_data_info.size == sizeof(NcaMetaDataHashData),
             ResultInvalidNcaMetaDataHashDataSize);

    // Read the meta data hash data.
    NcaMetaDataHashData meta_data_hash_data;
    base_storage->ReadObject(std::addressof(meta_data_hash_data),
                             meta_data_hash_data_info.offset - offset);

    // Set the out layer info storage, if necessary.
    if (out_layer_info_storage != nullptr) {
        auto layer_info_storage = std::make_shared<OffsetVfsFile>(
            base_storage,
            meta_data_hash_data_info.offset + meta_data_hash_data_info.size -
                meta_data_hash_data.layer_info_offset,
            meta_data_hash_data.layer_info_offset - offset);
        R_UNLESS(layer_info_storage != nullptr, ResultAllocationMemoryFailedAllocateShared);

        *out_layer_info_storage = std::move(layer_info_storage);
    }

    // Create the meta storage.
    auto meta_storage = std::make_shared<OffsetVfsFile>(
        std::move(base_storage), meta_data_hash_data_info.offset - offset, 0);
    R_UNLESS(meta_storage != nullptr, ResultAllocationMemoryFailedAllocateShared);

    // Create the integrity verification storage.
    R_RETURN(this->CreateIntegrityVerificationStorageImpl(
        out, std::move(meta_storage), meta_data_hash_data.integrity_meta_info,
        meta_data_hash_data.layer_info_offset - offset, IntegrityDataCacheCountForMeta,
        IntegrityHashCacheCountForMeta, 0));
}

Result NcaFileSystemDriver::CreateIntegrityVerificationStorageImpl(
    VirtualFile* out, VirtualFile base_storage,
    const NcaFsHeader::HashData::IntegrityMetaInfo& meta_info, s64 layer_info_offset,
    int max_data_cache_entries, int max_hash_cache_entries, s8 buffer_level) {
    // Validate preconditions.
    ASSERT(out != nullptr);
    ASSERT(base_storage != nullptr);
    ASSERT(layer_info_offset >= 0);

    // Define storage types.
    using VerificationStorage = HierarchicalIntegrityVerificationStorage;
    using StorageInfo = VerificationStorage::HierarchicalStorageInformation;

    // Validate the meta info.
    HierarchicalIntegrityVerificationInformation level_hash_info;
    std::memcpy(std::addressof(level_hash_info), std::addressof(meta_info.level_hash_info),
                sizeof(level_hash_info));

    R_UNLESS(IntegrityMinLayerCount <= level_hash_info.max_layers,
             ResultInvalidNcaHierarchicalIntegrityVerificationLayerCount);
    R_UNLESS(level_hash_info.max_layers <= IntegrityMaxLayerCount,
             ResultInvalidNcaHierarchicalIntegrityVerificationLayerCount);

    // Get the base storage size.
    s64 base_storage_size = base_storage->GetSize();

    // Create storage info.
    StorageInfo storage_info;
    for (s32 i = 0; i < static_cast<s32>(level_hash_info.max_layers - 2); ++i) {
        const auto& layer_info = level_hash_info.info[i];
        R_UNLESS(layer_info_offset + layer_info.offset + layer_info.size <= base_storage_size,
                 ResultNcaBaseStorageOutOfRangeD);

        storage_info[i + 1] = std::make_shared<OffsetVfsFile>(
            base_storage, layer_info.size, layer_info_offset + layer_info.offset);
    }

    // Set the last layer info.
    const auto& layer_info = level_hash_info.info[level_hash_info.max_layers - 2];
    const s64 last_layer_info_offset = layer_info_offset > 0 ? 0LL : layer_info.offset.Get();
    R_UNLESS(last_layer_info_offset + layer_info.size <= base_storage_size,
             ResultNcaBaseStorageOutOfRangeD);
    if (layer_info_offset > 0) {
        R_UNLESS(last_layer_info_offset + layer_info.size <= layer_info_offset,
                 ResultRomNcaInvalidIntegrityLayerInfoOffset);
    }
    storage_info.SetDataStorage(std::make_shared<OffsetVfsFile>(
        std::move(base_storage), layer_info.size, last_layer_info_offset));

    // Make the integrity romfs storage.
    auto integrity_storage = std::make_shared<IntegrityRomFsStorage>();
    R_UNLESS(integrity_storage != nullptr, ResultAllocationMemoryFailedAllocateShared);

    // Initialize the integrity storage.
    R_TRY(integrity_storage->Initialize(level_hash_info, meta_info.master_hash, storage_info,
                                        max_data_cache_entries, max_hash_cache_entries,
                                        buffer_level));

    // Set the output.
    *out = std::move(integrity_storage);
    R_SUCCEED();
}

Result NcaFileSystemDriver::CreateRegionSwitchStorage(VirtualFile* out,
                                                      const NcaFsHeaderReader* header_reader,
                                                      VirtualFile inside_storage,
                                                      VirtualFile outside_storage) {
    // Check pre-conditions.
    ASSERT(header_reader->GetHashType() == NcaFsHeader::HashType::HierarchicalIntegrityHash);

    // Create the region.
    RegionSwitchStorage::Region region = {};
    R_TRY(header_reader->GetHashTargetOffset(std::addressof(region.size)));

    // Create the region switch storage.
    auto region_switch_storage = std::make_shared<RegionSwitchStorage>(
        std::move(inside_storage), std::move(outside_storage), region);
    R_UNLESS(region_switch_storage != nullptr, ResultAllocationMemoryFailedAllocateShared);

    // Set the output.
    *out = std::move(region_switch_storage);
    R_SUCCEED();
}

Result NcaFileSystemDriver::CreateCompressedStorage(VirtualFile* out,
                                                    std::shared_ptr<CompressedStorage>* out_cmp,
                                                    VirtualFile* out_meta, VirtualFile base_storage,
                                                    const NcaCompressionInfo& compression_info) {
    R_RETURN(this->CreateCompressedStorage(out, out_cmp, out_meta, std::move(base_storage),
                                           compression_info, m_reader->GetDecompressor()));
}

Result NcaFileSystemDriver::CreateCompressedStorage(VirtualFile* out,
                                                    std::shared_ptr<CompressedStorage>* out_cmp,
                                                    VirtualFile* out_meta, VirtualFile base_storage,
                                                    const NcaCompressionInfo& compression_info,
                                                    GetDecompressorFunction get_decompressor) {
    // Check pre-conditions.
    ASSERT(out != nullptr);
    ASSERT(base_storage != nullptr);
    ASSERT(get_decompressor != nullptr);

    // Read and verify the bucket tree header.
    BucketTree::Header header;
    std::memcpy(std::addressof(header), compression_info.bucket.header.data(), sizeof(header));
    R_TRY(header.Verify());

    // Determine the storage extents.
    const auto table_offset = compression_info.bucket.offset;
    const auto table_size = compression_info.bucket.size;
    const auto node_size = CompressedStorage::QueryNodeStorageSize(header.entry_count);
    const auto entry_size = CompressedStorage::QueryEntryStorageSize(header.entry_count);
    R_UNLESS(node_size + entry_size <= table_size, ResultInvalidCompressedStorageSize);

    // If we should, set the output meta storage.
    if (out_meta != nullptr) {
        auto meta_storage = std::make_shared<OffsetVfsFile>(base_storage, table_size, table_offset);
        R_UNLESS(meta_storage != nullptr, ResultAllocationMemoryFailedAllocateShared);

        *out_meta = std::move(meta_storage);
    }

    // Allocate the compressed storage.
    auto compressed_storage = std::make_shared<CompressedStorage>();
    R_UNLESS(compressed_storage != nullptr, ResultAllocationMemoryFailedAllocateShared);

    // Initialize the compressed storage.
    R_TRY(compressed_storage->Initialize(
        std::make_shared<OffsetVfsFile>(base_storage, table_offset, 0),
        std::make_shared<OffsetVfsFile>(base_storage, node_size, table_offset),
        std::make_shared<OffsetVfsFile>(base_storage, entry_size, table_offset + node_size),
        header.entry_count, 64_KiB, 640_KiB, get_decompressor, 16_KiB, 16_KiB, 32));

    // Potentially set the output compressed storage.
    if (out_cmp) {
        *out_cmp = compressed_storage;
    }

    // Set the output.
    *out = std::move(compressed_storage);
    R_SUCCEED();
}

} // namespace FileSys
