// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "core/file_sys/fssystem/fssystem_compression_common.h"
#include "core/file_sys/fssystem/fssystem_nca_header.h"
#include "core/file_sys/vfs/vfs.h"

namespace FileSys {

class CompressedStorage;
class AesCtrCounterExtendedStorage;
class IndirectStorage;
class SparseStorage;

struct NcaCryptoConfiguration;

using KeyGenerationFunction = void (*)(void* dst_key, size_t dst_key_size, const void* src_key,
                                       size_t src_key_size, s32 key_type);
using VerifySign1Function = bool (*)(const void* sig, size_t sig_size, const void* data,
                                     size_t data_size, u8 generation);

struct NcaCryptoConfiguration {
    static constexpr size_t Rsa2048KeyModulusSize = 2048 / 8;
    static constexpr size_t Rsa2048KeyPublicExponentSize = 3;
    static constexpr size_t Rsa2048KeyPrivateExponentSize = Rsa2048KeyModulusSize;

    static constexpr size_t Aes128KeySize = 128 / 8;

    static constexpr size_t Header1SignatureKeyGenerationMax = 1;

    static constexpr s32 KeyAreaEncryptionKeyIndexCount = 3;
    static constexpr s32 HeaderEncryptionKeyCount = 2;

    static constexpr u8 KeyAreaEncryptionKeyIndexZeroKey = 0xFF;

    static constexpr size_t KeyGenerationMax = 32;

    std::array<const u8*, Header1SignatureKeyGenerationMax + 1> header_1_sign_key_moduli;
    std::array<u8, Rsa2048KeyPublicExponentSize> header_1_sign_key_public_exponent;
    std::array<std::array<u8, Aes128KeySize>, KeyAreaEncryptionKeyIndexCount>
        key_area_encryption_key_source;
    std::array<u8, Aes128KeySize> header_encryption_key_source;
    std::array<std::array<u8, Aes128KeySize>, HeaderEncryptionKeyCount>
        header_encrypted_encryption_keys;
    KeyGenerationFunction generate_key;
    VerifySign1Function verify_sign1;
    bool is_plaintext_header_available;
    bool is_available_sw_key;
};
static_assert(std::is_trivial_v<NcaCryptoConfiguration>);

struct NcaCompressionConfiguration {
    GetDecompressorFunction get_decompressor;
};
static_assert(std::is_trivial_v<NcaCompressionConfiguration>);

constexpr inline s32 KeyAreaEncryptionKeyCount =
    NcaCryptoConfiguration::KeyAreaEncryptionKeyIndexCount *
    NcaCryptoConfiguration::KeyGenerationMax;

enum class KeyType : s32 {
    ZeroKey = -2,
    InvalidKey = -1,
    NcaHeaderKey1 = KeyAreaEncryptionKeyCount + 0,
    NcaHeaderKey2 = KeyAreaEncryptionKeyCount + 1,
    NcaExternalKey = KeyAreaEncryptionKeyCount + 2,
    SaveDataDeviceUniqueMac = KeyAreaEncryptionKeyCount + 3,
    SaveDataSeedUniqueMac = KeyAreaEncryptionKeyCount + 4,
    SaveDataTransferMac = KeyAreaEncryptionKeyCount + 5,
};

constexpr inline bool IsInvalidKeyTypeValue(s32 key_type) {
    return key_type < 0;
}

constexpr inline s32 GetKeyTypeValue(u8 key_index, u8 key_generation) {
    if (key_index == NcaCryptoConfiguration::KeyAreaEncryptionKeyIndexZeroKey) {
        return static_cast<s32>(KeyType::ZeroKey);
    }

    if (key_index >= NcaCryptoConfiguration::KeyAreaEncryptionKeyIndexCount) {
        return static_cast<s32>(KeyType::InvalidKey);
    }

    return NcaCryptoConfiguration::KeyAreaEncryptionKeyIndexCount * key_generation + key_index;
}

class NcaReader {
    SUDACHI_NON_COPYABLE(NcaReader);
    SUDACHI_NON_MOVEABLE(NcaReader);

public:
    NcaReader();
    ~NcaReader();

    Result Initialize(VirtualFile base_storage, const NcaCryptoConfiguration& crypto_cfg,
                      const NcaCompressionConfiguration& compression_cfg);

    VirtualFile GetSharedBodyStorage();
    u32 GetMagic() const;
    NcaHeader::DistributionType GetDistributionType() const;
    NcaHeader::ContentType GetContentType() const;
    u8 GetHeaderSign1KeyGeneration() const;
    u8 GetKeyGeneration() const;
    u8 GetKeyIndex() const;
    u64 GetContentSize() const;
    u64 GetProgramId() const;
    u32 GetContentIndex() const;
    u32 GetSdkAddonVersion() const;
    void GetRightsId(u8* dst, size_t dst_size) const;
    bool HasFsInfo(s32 index) const;
    s32 GetFsCount() const;
    const Hash& GetFsHeaderHash(s32 index) const;
    void GetFsHeaderHash(Hash* dst, s32 index) const;
    void GetFsInfo(NcaHeader::FsInfo* dst, s32 index) const;
    u64 GetFsOffset(s32 index) const;
    u64 GetFsEndOffset(s32 index) const;
    u64 GetFsSize(s32 index) const;
    void GetEncryptedKey(void* dst, size_t size) const;
    const void* GetDecryptionKey(s32 index) const;
    bool HasValidInternalKey() const;
    bool HasInternalDecryptionKeyForAesHw() const;
    bool IsSoftwareAesPrioritized() const;
    void PrioritizeSoftwareAes();
    bool IsAvailableSwKey() const;
    bool HasExternalDecryptionKey() const;
    const void* GetExternalDecryptionKey() const;
    void SetExternalDecryptionKey(const void* src, size_t size);
    void GetRawData(void* dst, size_t dst_size) const;
    NcaHeader::EncryptionType GetEncryptionType() const;
    Result ReadHeader(NcaFsHeader* dst, s32 index) const;

    GetDecompressorFunction GetDecompressor() const;

    bool GetHeaderSign1Valid() const;

    void GetHeaderSign2(void* dst, size_t size) const;

private:
    NcaHeader m_header;
    std::array<std::array<u8, NcaCryptoConfiguration::Aes128KeySize>,
               NcaHeader::DecryptionKey_Count>
        m_decryption_keys;
    VirtualFile m_body_storage;
    VirtualFile m_header_storage;
    std::array<u8, NcaCryptoConfiguration::Aes128KeySize> m_external_decryption_key;
    bool m_is_software_aes_prioritized;
    bool m_is_available_sw_key;
    NcaHeader::EncryptionType m_header_encryption_type;
    bool m_is_header_sign1_signature_valid;
    GetDecompressorFunction m_get_decompressor;
};

class NcaFsHeaderReader {
    SUDACHI_NON_COPYABLE(NcaFsHeaderReader);
    SUDACHI_NON_MOVEABLE(NcaFsHeaderReader);

public:
    NcaFsHeaderReader() : m_fs_index(-1) {
        std::memset(std::addressof(m_data), 0, sizeof(m_data));
    }

    Result Initialize(const NcaReader& reader, s32 index);
    bool IsInitialized() const {
        return m_fs_index >= 0;
    }

    void GetRawData(void* dst, size_t dst_size) const;

    NcaFsHeader::HashData& GetHashData();
    const NcaFsHeader::HashData& GetHashData() const;
    u16 GetVersion() const;
    s32 GetFsIndex() const;
    NcaFsHeader::FsType GetFsType() const;
    NcaFsHeader::HashType GetHashType() const;
    NcaFsHeader::EncryptionType GetEncryptionType() const;
    NcaPatchInfo& GetPatchInfo();
    const NcaPatchInfo& GetPatchInfo() const;
    const NcaAesCtrUpperIv GetAesCtrUpperIv() const;

    bool IsSkipLayerHashEncryption() const;
    Result GetHashTargetOffset(s64* out) const;

    bool ExistsSparseLayer() const;
    NcaSparseInfo& GetSparseInfo();
    const NcaSparseInfo& GetSparseInfo() const;

    bool ExistsCompressionLayer() const;
    NcaCompressionInfo& GetCompressionInfo();
    const NcaCompressionInfo& GetCompressionInfo() const;

    bool ExistsPatchMetaHashLayer() const;
    NcaMetaDataHashDataInfo& GetPatchMetaDataHashDataInfo();
    const NcaMetaDataHashDataInfo& GetPatchMetaDataHashDataInfo() const;
    NcaFsHeader::MetaDataHashType GetPatchMetaHashType() const;

    bool ExistsSparseMetaHashLayer() const;
    NcaMetaDataHashDataInfo& GetSparseMetaDataHashDataInfo();
    const NcaMetaDataHashDataInfo& GetSparseMetaDataHashDataInfo() const;
    NcaFsHeader::MetaDataHashType GetSparseMetaHashType() const;

private:
    NcaFsHeader m_data;
    s32 m_fs_index;
};

class NcaFileSystemDriver {
    SUDACHI_NON_COPYABLE(NcaFileSystemDriver);
    SUDACHI_NON_MOVEABLE(NcaFileSystemDriver);

public:
    struct StorageContext {
        bool open_raw_storage;
        VirtualFile body_substorage;
        std::shared_ptr<SparseStorage> current_sparse_storage;
        VirtualFile sparse_storage_meta_storage;
        std::shared_ptr<SparseStorage> original_sparse_storage;
        void* external_current_sparse_storage;
        void* external_original_sparse_storage;
        VirtualFile aes_ctr_ex_storage_meta_storage;
        VirtualFile aes_ctr_ex_storage_data_storage;
        std::shared_ptr<AesCtrCounterExtendedStorage> aes_ctr_ex_storage;
        VirtualFile indirect_storage_meta_storage;
        std::shared_ptr<IndirectStorage> indirect_storage;
        VirtualFile fs_data_storage;
        VirtualFile compressed_storage_meta_storage;
        std::shared_ptr<CompressedStorage> compressed_storage;

        VirtualFile patch_layer_info_storage;
        VirtualFile sparse_layer_info_storage;

        VirtualFile external_original_storage;
    };

private:
    enum class AlignmentStorageRequirement {
        CacheBlockSize = 0,
        None = 1,
    };

public:
    static Result SetupFsHeaderReader(NcaFsHeaderReader* out, const NcaReader& reader,
                                      s32 fs_index);

public:
    NcaFileSystemDriver(std::shared_ptr<NcaReader> reader) : m_original_reader(), m_reader(reader) {
        ASSERT(m_reader != nullptr);
    }

    NcaFileSystemDriver(std::shared_ptr<NcaReader> original_reader,
                        std::shared_ptr<NcaReader> reader)
        : m_original_reader(original_reader), m_reader(reader) {
        ASSERT(m_reader != nullptr);
    }

    Result OpenStorageWithContext(VirtualFile* out, NcaFsHeaderReader* out_header_reader,
                                  s32 fs_index, StorageContext* ctx);

    Result OpenStorage(VirtualFile* out, NcaFsHeaderReader* out_header_reader, s32 fs_index) {
        // Create a storage context.
        StorageContext ctx{};

        // Open the storage.
        R_RETURN(OpenStorageWithContext(out, out_header_reader, fs_index, std::addressof(ctx)));
    }

public:
    Result CreateStorageByRawStorage(VirtualFile* out, const NcaFsHeaderReader* header_reader,
                                     VirtualFile raw_storage, StorageContext* ctx);

private:
    Result OpenStorageImpl(VirtualFile* out, NcaFsHeaderReader* out_header_reader, s32 fs_index,
                           StorageContext* ctx);

    Result OpenIndirectableStorageAsOriginal(VirtualFile* out,
                                             const NcaFsHeaderReader* header_reader,
                                             StorageContext* ctx);

    Result CreateBodySubStorage(VirtualFile* out, s64 offset, s64 size);

    Result CreateAesCtrStorage(VirtualFile* out, VirtualFile base_storage, s64 offset,
                               const NcaAesCtrUpperIv& upper_iv,
                               AlignmentStorageRequirement alignment_storage_requirement);
    Result CreateAesXtsStorage(VirtualFile* out, VirtualFile base_storage, s64 offset);

    Result CreateSparseStorageMetaStorage(VirtualFile* out, VirtualFile base_storage, s64 offset,
                                          const NcaAesCtrUpperIv& upper_iv,
                                          const NcaSparseInfo& sparse_info);
    Result CreateSparseStorageCore(std::shared_ptr<SparseStorage>* out, VirtualFile base_storage,
                                   s64 base_size, VirtualFile meta_storage,
                                   const NcaSparseInfo& sparse_info, bool external_info);
    Result CreateSparseStorage(VirtualFile* out, s64* out_fs_data_offset,
                               std::shared_ptr<SparseStorage>* out_sparse_storage,
                               VirtualFile* out_meta_storage, s32 index,
                               const NcaAesCtrUpperIv& upper_iv, const NcaSparseInfo& sparse_info);

    Result CreateSparseStorageMetaStorageWithVerification(
        VirtualFile* out, VirtualFile* out_verification, VirtualFile base_storage, s64 offset,
        const NcaAesCtrUpperIv& upper_iv, const NcaSparseInfo& sparse_info,
        const NcaMetaDataHashDataInfo& meta_data_hash_data_info);
    Result CreateSparseStorageWithVerification(
        VirtualFile* out, s64* out_fs_data_offset,
        std::shared_ptr<SparseStorage>* out_sparse_storage, VirtualFile* out_meta_storage,
        VirtualFile* out_verification, s32 index, const NcaAesCtrUpperIv& upper_iv,
        const NcaSparseInfo& sparse_info, const NcaMetaDataHashDataInfo& meta_data_hash_data_info,
        NcaFsHeader::MetaDataHashType meta_data_hash_type);

    Result CreateAesCtrExStorageMetaStorage(VirtualFile* out, VirtualFile base_storage, s64 offset,
                                            NcaFsHeader::EncryptionType encryption_type,
                                            const NcaAesCtrUpperIv& upper_iv,
                                            const NcaPatchInfo& patch_info);
    Result CreateAesCtrExStorage(VirtualFile* out,
                                 std::shared_ptr<AesCtrCounterExtendedStorage>* out_ext,
                                 VirtualFile base_storage, VirtualFile meta_storage,
                                 s64 counter_offset, const NcaAesCtrUpperIv& upper_iv,
                                 const NcaPatchInfo& patch_info);

    Result CreateIndirectStorageMetaStorage(VirtualFile* out, VirtualFile base_storage,
                                            const NcaPatchInfo& patch_info);
    Result CreateIndirectStorage(VirtualFile* out, std::shared_ptr<IndirectStorage>* out_ind,
                                 VirtualFile base_storage, VirtualFile original_data_storage,
                                 VirtualFile meta_storage, const NcaPatchInfo& patch_info);

    Result CreatePatchMetaStorage(VirtualFile* out_aes_ctr_ex_meta, VirtualFile* out_indirect_meta,
                                  VirtualFile* out_verification, VirtualFile base_storage,
                                  s64 offset, const NcaAesCtrUpperIv& upper_iv,
                                  const NcaPatchInfo& patch_info,
                                  const NcaMetaDataHashDataInfo& meta_data_hash_data_info);

    Result CreateSha256Storage(VirtualFile* out, VirtualFile base_storage,
                               const NcaFsHeader::HashData::HierarchicalSha256Data& sha256_data);

    Result CreateIntegrityVerificationStorage(
        VirtualFile* out, VirtualFile base_storage,
        const NcaFsHeader::HashData::IntegrityMetaInfo& meta_info);
    Result CreateIntegrityVerificationStorageForMeta(
        VirtualFile* out, VirtualFile* out_verification, VirtualFile base_storage, s64 offset,
        const NcaMetaDataHashDataInfo& meta_data_hash_data_info);
    Result CreateIntegrityVerificationStorageImpl(
        VirtualFile* out, VirtualFile base_storage,
        const NcaFsHeader::HashData::IntegrityMetaInfo& meta_info, s64 layer_info_offset,
        int max_data_cache_entries, int max_hash_cache_entries, s8 buffer_level);

    Result CreateRegionSwitchStorage(VirtualFile* out, const NcaFsHeaderReader* header_reader,
                                     VirtualFile inside_storage, VirtualFile outside_storage);

    Result CreateCompressedStorage(VirtualFile* out, std::shared_ptr<CompressedStorage>* out_cmp,
                                   VirtualFile* out_meta, VirtualFile base_storage,
                                   const NcaCompressionInfo& compression_info);

public:
    Result CreateCompressedStorage(VirtualFile* out, std::shared_ptr<CompressedStorage>* out_cmp,
                                   VirtualFile* out_meta, VirtualFile base_storage,
                                   const NcaCompressionInfo& compression_info,
                                   GetDecompressorFunction get_decompressor);

private:
    std::shared_ptr<NcaReader> m_original_reader;
    std::shared_ptr<NcaReader> m_reader;
};

} // namespace FileSys
