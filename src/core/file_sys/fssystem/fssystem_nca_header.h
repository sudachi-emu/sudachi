// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "common/common_funcs.h"
#include "common/common_types.h"
#include "common/literals.h"

#include "core/file_sys/errors.h"
#include "core/file_sys/fssystem/fs_types.h"

namespace FileSys {

using namespace Common::Literals;

struct Hash {
    static constexpr std::size_t Size = 256 / 8;
    std::array<u8, Size> value;
};
static_assert(sizeof(Hash) == Hash::Size);
static_assert(std::is_trivial_v<Hash>);

using NcaDigest = Hash;

struct NcaHeader {
    enum class ContentType : u8 {
        Program = 0,
        Meta = 1,
        Control = 2,
        Manual = 3,
        Data = 4,
        PublicData = 5,

        Start = Program,
        End = PublicData,
    };

    enum class DistributionType : u8 {
        Download = 0,
        GameCard = 1,

        Start = Download,
        End = GameCard,
    };

    enum class EncryptionType : u8 {
        Auto = 0,
        None = 1,
    };

    enum DecryptionKey {
        DecryptionKey_AesXts = 0,
        DecryptionKey_AesXts1 = DecryptionKey_AesXts,
        DecryptionKey_AesXts2 = 1,
        DecryptionKey_AesCtr = 2,
        DecryptionKey_AesCtrEx = 3,
        DecryptionKey_AesCtrHw = 4,
        DecryptionKey_Count,
    };

    struct FsInfo {
        u32 start_sector;
        u32 end_sector;
        u32 hash_sectors;
        u32 reserved;
    };
    static_assert(sizeof(FsInfo) == 0x10);
    static_assert(std::is_trivial_v<FsInfo>);

    static constexpr u32 Magic0 = Common::MakeMagic('N', 'C', 'A', '0');
    static constexpr u32 Magic1 = Common::MakeMagic('N', 'C', 'A', '1');
    static constexpr u32 Magic2 = Common::MakeMagic('N', 'C', 'A', '2');
    static constexpr u32 Magic3 = Common::MakeMagic('N', 'C', 'A', '3');

    static constexpr u32 Magic = Magic3;

    static constexpr std::size_t Size = 1_KiB;
    static constexpr s32 FsCountMax = 4;
    static constexpr std::size_t HeaderSignCount = 2;
    static constexpr std::size_t HeaderSignSize = 0x100;
    static constexpr std::size_t EncryptedKeyAreaSize = 0x100;
    static constexpr std::size_t SectorSize = 0x200;
    static constexpr std::size_t SectorShift = 9;
    static constexpr std::size_t RightsIdSize = 0x10;
    static constexpr std::size_t XtsBlockSize = 0x200;
    static constexpr std::size_t CtrBlockSize = 0x10;

    static_assert(SectorSize == (1 << SectorShift));

    // Data members.
    std::array<u8, HeaderSignSize> header_sign_1;
    std::array<u8, HeaderSignSize> header_sign_2;
    u32 magic;
    DistributionType distribution_type;
    ContentType content_type;
    u8 key_generation;
    u8 key_index;
    u64 content_size;
    u64 program_id;
    u32 content_index;
    u32 sdk_addon_version;
    u8 key_generation_2;
    u8 header1_signature_key_generation;
    std::array<u8, 2> reserved_222;
    std::array<u32, 3> reserved_224;
    std::array<u8, RightsIdSize> rights_id;
    std::array<FsInfo, FsCountMax> fs_info;
    std::array<Hash, FsCountMax> fs_header_hash;
    std::array<u8, EncryptedKeyAreaSize> encrypted_key_area;

    static constexpr u64 SectorToByte(u32 sector) {
        return static_cast<u64>(sector) << SectorShift;
    }

    static constexpr u32 ByteToSector(u64 byte) {
        return static_cast<u32>(byte >> SectorShift);
    }

    u8 GetProperKeyGeneration() const;
};
static_assert(sizeof(NcaHeader) == NcaHeader::Size);
static_assert(std::is_trivial_v<NcaHeader>);

struct NcaBucketInfo {
    static constexpr size_t HeaderSize = 0x10;
    Int64 offset;
    Int64 size;
    std::array<u8, HeaderSize> header;
};
static_assert(std::is_trivial_v<NcaBucketInfo>);

struct NcaPatchInfo {
    static constexpr size_t Size = 0x40;
    static constexpr size_t Offset = 0x100;

    Int64 indirect_offset;
    Int64 indirect_size;
    std::array<u8, NcaBucketInfo::HeaderSize> indirect_header;
    Int64 aes_ctr_ex_offset;
    Int64 aes_ctr_ex_size;
    std::array<u8, NcaBucketInfo::HeaderSize> aes_ctr_ex_header;

    bool HasIndirectTable() const;
    bool HasAesCtrExTable() const;
};
static_assert(std::is_trivial_v<NcaPatchInfo>);

union NcaAesCtrUpperIv {
    u64 value;
    struct {
        u32 generation;
        u32 secure_value;
    } part;
};
static_assert(std::is_trivial_v<NcaAesCtrUpperIv>);

struct NcaSparseInfo {
    NcaBucketInfo bucket;
    Int64 physical_offset;
    u16 generation;
    std::array<u8, 6> reserved;

    s64 GetPhysicalSize() const {
        return this->bucket.offset + this->bucket.size;
    }

    u32 GetGeneration() const {
        return static_cast<u32>(this->generation) << 16;
    }

    const NcaAesCtrUpperIv MakeAesCtrUpperIv(NcaAesCtrUpperIv upper_iv) const {
        NcaAesCtrUpperIv sparse_upper_iv = upper_iv;
        sparse_upper_iv.part.generation = this->GetGeneration();
        return sparse_upper_iv;
    }
};
static_assert(std::is_trivial_v<NcaSparseInfo>);

struct NcaCompressionInfo {
    NcaBucketInfo bucket;
    std::array<u8, 8> resreved;
};
static_assert(std::is_trivial_v<NcaCompressionInfo>);

struct NcaMetaDataHashDataInfo {
    Int64 offset;
    Int64 size;
    Hash hash;
};
static_assert(std::is_trivial_v<NcaMetaDataHashDataInfo>);

struct NcaFsHeader {
    static constexpr size_t Size = 0x200;
    static constexpr size_t HashDataOffset = 0x8;

    struct Region {
        Int64 offset;
        Int64 size;
    };
    static_assert(std::is_trivial_v<Region>);

    enum class FsType : u8 {
        RomFs = 0,
        PartitionFs = 1,
    };

    enum class EncryptionType : u8 {
        Auto = 0,
        None = 1,
        AesXts = 2,
        AesCtr = 3,
        AesCtrEx = 4,
        AesCtrSkipLayerHash = 5,
        AesCtrExSkipLayerHash = 6,
    };

    enum class HashType : u8 {
        Auto = 0,
        None = 1,
        HierarchicalSha256Hash = 2,
        HierarchicalIntegrityHash = 3,
        AutoSha3 = 4,
        HierarchicalSha3256Hash = 5,
        HierarchicalIntegritySha3Hash = 6,
    };

    enum class MetaDataHashType : u8 {
        None = 0,
        HierarchicalIntegrity = 1,
    };

    union HashData {
        struct HierarchicalSha256Data {
            static constexpr size_t HashLayerCountMax = 5;
            static const size_t MasterHashOffset;

            Hash fs_data_master_hash;
            s32 hash_block_size;
            s32 hash_layer_count;
            std::array<Region, HashLayerCountMax> hash_layer_region;
        } hierarchical_sha256_data;
        static_assert(std::is_trivial_v<HierarchicalSha256Data>);

        struct IntegrityMetaInfo {
            static const size_t MasterHashOffset;

            u32 magic;
            u32 version;
            u32 master_hash_size;

            struct LevelHashInfo {
                u32 max_layers;

                struct HierarchicalIntegrityVerificationLevelInformation {
                    static constexpr size_t IntegrityMaxLayerCount = 7;
                    Int64 offset;
                    Int64 size;
                    s32 block_order;
                    std::array<u8, 4> reserved;
                };
                std::array<
                    HierarchicalIntegrityVerificationLevelInformation,
                    HierarchicalIntegrityVerificationLevelInformation::IntegrityMaxLayerCount - 1>
                    info;

                struct SignatureSalt {
                    static constexpr size_t Size = 0x20;
                    std::array<u8, Size> value;
                };
                SignatureSalt seed;
            } level_hash_info;

            Hash master_hash;
        } integrity_meta_info;
        static_assert(std::is_trivial_v<IntegrityMetaInfo>);

        std::array<u8, NcaPatchInfo::Offset - HashDataOffset> padding;
    };

    u16 version;
    FsType fs_type;
    HashType hash_type;
    EncryptionType encryption_type;
    MetaDataHashType meta_data_hash_type;
    std::array<u8, 2> reserved;
    HashData hash_data;
    NcaPatchInfo patch_info;
    NcaAesCtrUpperIv aes_ctr_upper_iv;
    NcaSparseInfo sparse_info;
    NcaCompressionInfo compression_info;
    NcaMetaDataHashDataInfo meta_data_hash_data_info;
    std::array<u8, 0x30> pad;

    bool IsSkipLayerHashEncryption() const {
        return this->encryption_type == EncryptionType::AesCtrSkipLayerHash ||
               this->encryption_type == EncryptionType::AesCtrExSkipLayerHash;
    }

    Result GetHashTargetOffset(s64* out) const {
        switch (this->hash_type) {
        case HashType::HierarchicalIntegrityHash:
        case HashType::HierarchicalIntegritySha3Hash:
            *out = this->hash_data.integrity_meta_info.level_hash_info
                       .info[this->hash_data.integrity_meta_info.level_hash_info.max_layers - 2]
                       .offset;
            R_SUCCEED();
        case HashType::HierarchicalSha256Hash:
        case HashType::HierarchicalSha3256Hash:
            *out =
                this->hash_data.hierarchical_sha256_data
                    .hash_layer_region[this->hash_data.hierarchical_sha256_data.hash_layer_count -
                                       1]
                    .offset;
            R_SUCCEED();
        default:
            R_THROW(ResultInvalidNcaFsHeader);
        }
    }
};
static_assert(sizeof(NcaFsHeader) == NcaFsHeader::Size);
static_assert(std::is_trivial_v<NcaFsHeader>);
static_assert(offsetof(NcaFsHeader, patch_info) == NcaPatchInfo::Offset);

inline constexpr const size_t NcaFsHeader::HashData::HierarchicalSha256Data::MasterHashOffset =
    offsetof(NcaFsHeader, hash_data.hierarchical_sha256_data.fs_data_master_hash);
inline constexpr const size_t NcaFsHeader::HashData::IntegrityMetaInfo::MasterHashOffset =
    offsetof(NcaFsHeader, hash_data.integrity_meta_info.master_hash);

struct NcaMetaDataHashData {
    s64 layer_info_offset;
    NcaFsHeader::HashData::IntegrityMetaInfo integrity_meta_info;
};
static_assert(sizeof(NcaMetaDataHashData) ==
              sizeof(NcaFsHeader::HashData::IntegrityMetaInfo) + sizeof(s64));
static_assert(std::is_trivial_v<NcaMetaDataHashData>);

} // namespace FileSys
