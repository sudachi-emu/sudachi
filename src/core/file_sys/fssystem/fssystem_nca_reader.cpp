// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/file_sys/fssystem/fssystem_aes_xts_storage.h"
#include "core/file_sys/fssystem/fssystem_nca_file_system_driver.h"
#include "core/file_sys/vfs/vfs_offset.h"

namespace FileSys {

namespace {

constexpr inline u32 SdkAddonVersionMin = 0x000B0000;
constexpr inline size_t Aes128KeySize = 0x10;
constexpr const std::array<u8, Aes128KeySize> ZeroKey{};

constexpr Result CheckNcaMagic(u32 magic) {
    // Verify the magic is not a deprecated one.
    R_UNLESS(magic != NcaHeader::Magic0, ResultUnsupportedSdkVersion);
    R_UNLESS(magic != NcaHeader::Magic1, ResultUnsupportedSdkVersion);
    R_UNLESS(magic != NcaHeader::Magic2, ResultUnsupportedSdkVersion);

    // Verify the magic is the current one.
    R_UNLESS(magic == NcaHeader::Magic3, ResultInvalidNcaSignature);

    R_SUCCEED();
}

} // namespace

NcaReader::NcaReader()
    : m_body_storage(), m_header_storage(), m_is_software_aes_prioritized(false),
      m_is_available_sw_key(false), m_header_encryption_type(NcaHeader::EncryptionType::Auto),
      m_get_decompressor() {
    std::memset(std::addressof(m_header), 0, sizeof(m_header));
    std::memset(std::addressof(m_decryption_keys), 0, sizeof(m_decryption_keys));
    std::memset(std::addressof(m_external_decryption_key), 0, sizeof(m_external_decryption_key));
}

NcaReader::~NcaReader() {}

Result NcaReader::Initialize(VirtualFile base_storage, const NcaCryptoConfiguration& crypto_cfg,
                             const NcaCompressionConfiguration& compression_cfg) {
    // Validate preconditions.
    ASSERT(base_storage != nullptr);
    ASSERT(m_body_storage == nullptr);

    // Create the work header storage storage.
    VirtualFile work_header_storage;

    // We need to be able to generate keys.
    R_UNLESS(crypto_cfg.generate_key != nullptr, ResultInvalidArgument);

    // Generate keys for header.
    using AesXtsStorageForNcaHeader = AesXtsStorage;

    constexpr std::array<s32, NcaCryptoConfiguration::HeaderEncryptionKeyCount>
        HeaderKeyTypeValues = {
            static_cast<s32>(KeyType::NcaHeaderKey1),
            static_cast<s32>(KeyType::NcaHeaderKey2),
        };

    std::array<std::array<u8, NcaCryptoConfiguration::Aes128KeySize>,
               NcaCryptoConfiguration::HeaderEncryptionKeyCount>
        header_decryption_keys;
    for (size_t i = 0; i < NcaCryptoConfiguration::HeaderEncryptionKeyCount; i++) {
        crypto_cfg.generate_key(header_decryption_keys[i].data(),
                                AesXtsStorageForNcaHeader::KeySize,
                                crypto_cfg.header_encrypted_encryption_keys[i].data(),
                                AesXtsStorageForNcaHeader::KeySize, HeaderKeyTypeValues[i]);
    }

    // Create the header storage.
    std::array<u8, AesXtsStorageForNcaHeader::IvSize> header_iv = {};
    work_header_storage = std::make_unique<AesXtsStorageForNcaHeader>(
        base_storage, header_decryption_keys[0].data(), header_decryption_keys[1].data(),
        AesXtsStorageForNcaHeader::KeySize, header_iv.data(), AesXtsStorageForNcaHeader::IvSize,
        NcaHeader::XtsBlockSize);

    // Check that we successfully created the storage.
    R_UNLESS(work_header_storage != nullptr, ResultAllocationMemoryFailedInNcaReaderA);

    // Read the header.
    work_header_storage->ReadObject(std::addressof(m_header), 0);

    // Validate the magic.
    if (const Result magic_result = CheckNcaMagic(m_header.magic); R_FAILED(magic_result)) {
        // Try to use a plaintext header.
        base_storage->ReadObject(std::addressof(m_header), 0);
        R_UNLESS(R_SUCCEEDED(CheckNcaMagic(m_header.magic)), magic_result);

        // Configure to use the plaintext header.
        auto base_storage_size = base_storage->GetSize();
        work_header_storage = std::make_shared<OffsetVfsFile>(base_storage, base_storage_size, 0);
        R_UNLESS(work_header_storage != nullptr, ResultAllocationMemoryFailedInNcaReaderA);

        // Set encryption type as plaintext.
        m_header_encryption_type = NcaHeader::EncryptionType::None;
    }

    // Verify the header sign1.
    if (crypto_cfg.verify_sign1 != nullptr) {
        const u8* sig = m_header.header_sign_1.data();
        const size_t sig_size = NcaHeader::HeaderSignSize;
        const u8* msg =
            static_cast<const u8*>(static_cast<const void*>(std::addressof(m_header.magic)));
        const size_t msg_size =
            NcaHeader::Size - NcaHeader::HeaderSignSize * NcaHeader::HeaderSignCount;

        m_is_header_sign1_signature_valid = crypto_cfg.verify_sign1(
            sig, sig_size, msg, msg_size, m_header.header1_signature_key_generation);

        if (!m_is_header_sign1_signature_valid) {
            LOG_WARNING(Common_Filesystem, "Invalid NCA header sign1");
        }
    }

    // Validate the sdk version.
    R_UNLESS(m_header.sdk_addon_version >= SdkAddonVersionMin, ResultUnsupportedSdkVersion);

    // Validate the key index.
    R_UNLESS(m_header.key_index < NcaCryptoConfiguration::KeyAreaEncryptionKeyIndexCount ||
                 m_header.key_index == NcaCryptoConfiguration::KeyAreaEncryptionKeyIndexZeroKey,
             ResultInvalidNcaKeyIndex);

    // Check if we have a rights id.
    constexpr const std::array<u8, NcaHeader::RightsIdSize> ZeroRightsId{};
    if (std::memcmp(ZeroRightsId.data(), m_header.rights_id.data(), NcaHeader::RightsIdSize) == 0) {
        // If we don't, then we don't have an external key, so we need to generate decryption keys.
        crypto_cfg.generate_key(
            m_decryption_keys[NcaHeader::DecryptionKey_AesCtr].data(), Aes128KeySize,
            m_header.encrypted_key_area.data() + NcaHeader::DecryptionKey_AesCtr * Aes128KeySize,
            Aes128KeySize, GetKeyTypeValue(m_header.key_index, m_header.GetProperKeyGeneration()));
        crypto_cfg.generate_key(
            m_decryption_keys[NcaHeader::DecryptionKey_AesXts1].data(), Aes128KeySize,
            m_header.encrypted_key_area.data() + NcaHeader::DecryptionKey_AesXts1 * Aes128KeySize,
            Aes128KeySize, GetKeyTypeValue(m_header.key_index, m_header.GetProperKeyGeneration()));
        crypto_cfg.generate_key(
            m_decryption_keys[NcaHeader::DecryptionKey_AesXts2].data(), Aes128KeySize,
            m_header.encrypted_key_area.data() + NcaHeader::DecryptionKey_AesXts2 * Aes128KeySize,
            Aes128KeySize, GetKeyTypeValue(m_header.key_index, m_header.GetProperKeyGeneration()));
        crypto_cfg.generate_key(
            m_decryption_keys[NcaHeader::DecryptionKey_AesCtrEx].data(), Aes128KeySize,
            m_header.encrypted_key_area.data() + NcaHeader::DecryptionKey_AesCtrEx * Aes128KeySize,
            Aes128KeySize, GetKeyTypeValue(m_header.key_index, m_header.GetProperKeyGeneration()));

        // Copy the hardware speed emulation key.
        std::memcpy(m_decryption_keys[NcaHeader::DecryptionKey_AesCtrHw].data(),
                    m_header.encrypted_key_area.data() +
                        NcaHeader::DecryptionKey_AesCtrHw * Aes128KeySize,
                    Aes128KeySize);
    }

    // Clear the external decryption key.
    std::memset(m_external_decryption_key.data(), 0, m_external_decryption_key.size());

    // Set software key availability.
    m_is_available_sw_key = crypto_cfg.is_available_sw_key;

    // Set our decompressor function getter.
    m_get_decompressor = compression_cfg.get_decompressor;

    // Set our storages.
    m_header_storage = std::move(work_header_storage);
    m_body_storage = std::move(base_storage);

    R_SUCCEED();
}

VirtualFile NcaReader::GetSharedBodyStorage() {
    ASSERT(m_body_storage != nullptr);
    return m_body_storage;
}

u32 NcaReader::GetMagic() const {
    ASSERT(m_body_storage != nullptr);
    return m_header.magic;
}

NcaHeader::DistributionType NcaReader::GetDistributionType() const {
    ASSERT(m_body_storage != nullptr);
    return m_header.distribution_type;
}

NcaHeader::ContentType NcaReader::GetContentType() const {
    ASSERT(m_body_storage != nullptr);
    return m_header.content_type;
}

u8 NcaReader::GetHeaderSign1KeyGeneration() const {
    ASSERT(m_body_storage != nullptr);
    return m_header.header1_signature_key_generation;
}

u8 NcaReader::GetKeyGeneration() const {
    ASSERT(m_body_storage != nullptr);
    return m_header.GetProperKeyGeneration();
}

u8 NcaReader::GetKeyIndex() const {
    ASSERT(m_body_storage != nullptr);
    return m_header.key_index;
}

u64 NcaReader::GetContentSize() const {
    ASSERT(m_body_storage != nullptr);
    return m_header.content_size;
}

u64 NcaReader::GetProgramId() const {
    ASSERT(m_body_storage != nullptr);
    return m_header.program_id;
}

u32 NcaReader::GetContentIndex() const {
    ASSERT(m_body_storage != nullptr);
    return m_header.content_index;
}

u32 NcaReader::GetSdkAddonVersion() const {
    ASSERT(m_body_storage != nullptr);
    return m_header.sdk_addon_version;
}

void NcaReader::GetRightsId(u8* dst, size_t dst_size) const {
    ASSERT(dst != nullptr);
    ASSERT(dst_size >= NcaHeader::RightsIdSize);

    std::memcpy(dst, m_header.rights_id.data(), NcaHeader::RightsIdSize);
}

bool NcaReader::HasFsInfo(s32 index) const {
    ASSERT(0 <= index && index < NcaHeader::FsCountMax);
    return m_header.fs_info[index].start_sector != 0 || m_header.fs_info[index].end_sector != 0;
}

s32 NcaReader::GetFsCount() const {
    ASSERT(m_body_storage != nullptr);
    for (s32 i = 0; i < NcaHeader::FsCountMax; i++) {
        if (!this->HasFsInfo(i)) {
            return i;
        }
    }
    return NcaHeader::FsCountMax;
}

const Hash& NcaReader::GetFsHeaderHash(s32 index) const {
    ASSERT(m_body_storage != nullptr);
    ASSERT(0 <= index && index < NcaHeader::FsCountMax);
    return m_header.fs_header_hash[index];
}

void NcaReader::GetFsHeaderHash(Hash* dst, s32 index) const {
    ASSERT(m_body_storage != nullptr);
    ASSERT(0 <= index && index < NcaHeader::FsCountMax);
    ASSERT(dst != nullptr);
    std::memcpy(dst, std::addressof(m_header.fs_header_hash[index]), sizeof(*dst));
}

void NcaReader::GetFsInfo(NcaHeader::FsInfo* dst, s32 index) const {
    ASSERT(m_body_storage != nullptr);
    ASSERT(0 <= index && index < NcaHeader::FsCountMax);
    ASSERT(dst != nullptr);
    std::memcpy(dst, std::addressof(m_header.fs_info[index]), sizeof(*dst));
}

u64 NcaReader::GetFsOffset(s32 index) const {
    ASSERT(m_body_storage != nullptr);
    ASSERT(0 <= index && index < NcaHeader::FsCountMax);
    return NcaHeader::SectorToByte(m_header.fs_info[index].start_sector);
}

u64 NcaReader::GetFsEndOffset(s32 index) const {
    ASSERT(m_body_storage != nullptr);
    ASSERT(0 <= index && index < NcaHeader::FsCountMax);
    return NcaHeader::SectorToByte(m_header.fs_info[index].end_sector);
}

u64 NcaReader::GetFsSize(s32 index) const {
    ASSERT(m_body_storage != nullptr);
    ASSERT(0 <= index && index < NcaHeader::FsCountMax);
    return NcaHeader::SectorToByte(m_header.fs_info[index].end_sector -
                                   m_header.fs_info[index].start_sector);
}

void NcaReader::GetEncryptedKey(void* dst, size_t size) const {
    ASSERT(m_body_storage != nullptr);
    ASSERT(dst != nullptr);
    ASSERT(size >= NcaHeader::EncryptedKeyAreaSize);

    std::memcpy(dst, m_header.encrypted_key_area.data(), NcaHeader::EncryptedKeyAreaSize);
}

const void* NcaReader::GetDecryptionKey(s32 index) const {
    ASSERT(m_body_storage != nullptr);
    ASSERT(0 <= index && index < NcaHeader::DecryptionKey_Count);
    return m_decryption_keys[index].data();
}

bool NcaReader::HasValidInternalKey() const {
    for (s32 i = 0; i < NcaHeader::DecryptionKey_Count; i++) {
        if (std::memcmp(ZeroKey.data(), m_header.encrypted_key_area.data() + i * Aes128KeySize,
                        Aes128KeySize) != 0) {
            return true;
        }
    }
    return false;
}

bool NcaReader::HasInternalDecryptionKeyForAesHw() const {
    return std::memcmp(ZeroKey.data(), this->GetDecryptionKey(NcaHeader::DecryptionKey_AesCtrHw),
                       Aes128KeySize) != 0;
}

bool NcaReader::IsSoftwareAesPrioritized() const {
    return m_is_software_aes_prioritized;
}

void NcaReader::PrioritizeSoftwareAes() {
    m_is_software_aes_prioritized = true;
}

bool NcaReader::IsAvailableSwKey() const {
    return m_is_available_sw_key;
}

bool NcaReader::HasExternalDecryptionKey() const {
    return std::memcmp(ZeroKey.data(), this->GetExternalDecryptionKey(), Aes128KeySize) != 0;
}

const void* NcaReader::GetExternalDecryptionKey() const {
    return m_external_decryption_key.data();
}

void NcaReader::SetExternalDecryptionKey(const void* src, size_t size) {
    ASSERT(src != nullptr);
    ASSERT(size == sizeof(m_external_decryption_key));

    std::memcpy(m_external_decryption_key.data(), src, sizeof(m_external_decryption_key));
}

void NcaReader::GetRawData(void* dst, size_t dst_size) const {
    ASSERT(m_body_storage != nullptr);
    ASSERT(dst != nullptr);
    ASSERT(dst_size >= sizeof(NcaHeader));

    std::memcpy(dst, std::addressof(m_header), sizeof(NcaHeader));
}

GetDecompressorFunction NcaReader::GetDecompressor() const {
    ASSERT(m_get_decompressor != nullptr);
    return m_get_decompressor;
}

NcaHeader::EncryptionType NcaReader::GetEncryptionType() const {
    return m_header_encryption_type;
}

Result NcaReader::ReadHeader(NcaFsHeader* dst, s32 index) const {
    ASSERT(dst != nullptr);
    ASSERT(0 <= index && index < NcaHeader::FsCountMax);

    const s64 offset = sizeof(NcaHeader) + sizeof(NcaFsHeader) * index;
    m_header_storage->ReadObject(dst, offset);

    R_SUCCEED();
}

bool NcaReader::GetHeaderSign1Valid() const {
    return m_is_header_sign1_signature_valid;
}

void NcaReader::GetHeaderSign2(void* dst, size_t size) const {
    ASSERT(dst != nullptr);
    ASSERT(size == NcaHeader::HeaderSignSize);

    std::memcpy(dst, m_header.header_sign_2.data(), size);
}

Result NcaFsHeaderReader::Initialize(const NcaReader& reader, s32 index) {
    // Reset ourselves to uninitialized.
    m_fs_index = -1;

    // Read the header.
    R_TRY(reader.ReadHeader(std::addressof(m_data), index));

    // Set our index.
    m_fs_index = index;
    R_SUCCEED();
}

void NcaFsHeaderReader::GetRawData(void* dst, size_t dst_size) const {
    ASSERT(this->IsInitialized());
    ASSERT(dst != nullptr);
    ASSERT(dst_size >= sizeof(NcaFsHeader));

    std::memcpy(dst, std::addressof(m_data), sizeof(NcaFsHeader));
}

NcaFsHeader::HashData& NcaFsHeaderReader::GetHashData() {
    ASSERT(this->IsInitialized());
    return m_data.hash_data;
}

const NcaFsHeader::HashData& NcaFsHeaderReader::GetHashData() const {
    ASSERT(this->IsInitialized());
    return m_data.hash_data;
}

u16 NcaFsHeaderReader::GetVersion() const {
    ASSERT(this->IsInitialized());
    return m_data.version;
}

s32 NcaFsHeaderReader::GetFsIndex() const {
    ASSERT(this->IsInitialized());
    return m_fs_index;
}

NcaFsHeader::FsType NcaFsHeaderReader::GetFsType() const {
    ASSERT(this->IsInitialized());
    return m_data.fs_type;
}

NcaFsHeader::HashType NcaFsHeaderReader::GetHashType() const {
    ASSERT(this->IsInitialized());
    return m_data.hash_type;
}

NcaFsHeader::EncryptionType NcaFsHeaderReader::GetEncryptionType() const {
    ASSERT(this->IsInitialized());
    return m_data.encryption_type;
}

NcaPatchInfo& NcaFsHeaderReader::GetPatchInfo() {
    ASSERT(this->IsInitialized());
    return m_data.patch_info;
}

const NcaPatchInfo& NcaFsHeaderReader::GetPatchInfo() const {
    ASSERT(this->IsInitialized());
    return m_data.patch_info;
}

const NcaAesCtrUpperIv NcaFsHeaderReader::GetAesCtrUpperIv() const {
    ASSERT(this->IsInitialized());
    return m_data.aes_ctr_upper_iv;
}

bool NcaFsHeaderReader::IsSkipLayerHashEncryption() const {
    ASSERT(this->IsInitialized());
    return m_data.IsSkipLayerHashEncryption();
}

Result NcaFsHeaderReader::GetHashTargetOffset(s64* out) const {
    ASSERT(out != nullptr);
    ASSERT(this->IsInitialized());

    R_RETURN(m_data.GetHashTargetOffset(out));
}

bool NcaFsHeaderReader::ExistsSparseLayer() const {
    ASSERT(this->IsInitialized());
    return m_data.sparse_info.generation != 0;
}

NcaSparseInfo& NcaFsHeaderReader::GetSparseInfo() {
    ASSERT(this->IsInitialized());
    return m_data.sparse_info;
}

const NcaSparseInfo& NcaFsHeaderReader::GetSparseInfo() const {
    ASSERT(this->IsInitialized());
    return m_data.sparse_info;
}

bool NcaFsHeaderReader::ExistsCompressionLayer() const {
    ASSERT(this->IsInitialized());
    return m_data.compression_info.bucket.offset != 0 && m_data.compression_info.bucket.size != 0;
}

NcaCompressionInfo& NcaFsHeaderReader::GetCompressionInfo() {
    ASSERT(this->IsInitialized());
    return m_data.compression_info;
}

const NcaCompressionInfo& NcaFsHeaderReader::GetCompressionInfo() const {
    ASSERT(this->IsInitialized());
    return m_data.compression_info;
}

bool NcaFsHeaderReader::ExistsPatchMetaHashLayer() const {
    ASSERT(this->IsInitialized());
    return m_data.meta_data_hash_data_info.size != 0 && this->GetPatchInfo().HasIndirectTable();
}

NcaMetaDataHashDataInfo& NcaFsHeaderReader::GetPatchMetaDataHashDataInfo() {
    ASSERT(this->IsInitialized());
    return m_data.meta_data_hash_data_info;
}

const NcaMetaDataHashDataInfo& NcaFsHeaderReader::GetPatchMetaDataHashDataInfo() const {
    ASSERT(this->IsInitialized());
    return m_data.meta_data_hash_data_info;
}

NcaFsHeader::MetaDataHashType NcaFsHeaderReader::GetPatchMetaHashType() const {
    ASSERT(this->IsInitialized());
    return m_data.meta_data_hash_type;
}

bool NcaFsHeaderReader::ExistsSparseMetaHashLayer() const {
    ASSERT(this->IsInitialized());
    return m_data.meta_data_hash_data_info.size != 0 && this->ExistsSparseLayer();
}

NcaMetaDataHashDataInfo& NcaFsHeaderReader::GetSparseMetaDataHashDataInfo() {
    ASSERT(this->IsInitialized());
    return m_data.meta_data_hash_data_info;
}

const NcaMetaDataHashDataInfo& NcaFsHeaderReader::GetSparseMetaDataHashDataInfo() const {
    ASSERT(this->IsInitialized());
    return m_data.meta_data_hash_data_info;
}

NcaFsHeader::MetaDataHashType NcaFsHeaderReader::GetSparseMetaHashType() const {
    ASSERT(this->IsInitialized());
    return m_data.meta_data_hash_type;
}

} // namespace FileSys
