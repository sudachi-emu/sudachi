// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "common/alignment.h"
#include "core/file_sys/fssystem/fs_i_storage.h"
#include "core/file_sys/fssystem/fs_types.h"
#include "core/file_sys/fssystem/fssystem_alignment_matching_storage.h"
#include "core/file_sys/fssystem/fssystem_integrity_verification_storage.h"
#include "core/file_sys/vfs/vfs_offset.h"

namespace FileSys {

struct HierarchicalIntegrityVerificationLevelInformation {
    Int64 offset;
    Int64 size;
    s32 block_order;
    std::array<u8, 4> reserved;
};
static_assert(std::is_trivial_v<HierarchicalIntegrityVerificationLevelInformation>);
static_assert(sizeof(HierarchicalIntegrityVerificationLevelInformation) == 0x18);
static_assert(alignof(HierarchicalIntegrityVerificationLevelInformation) == 0x4);

struct HierarchicalIntegrityVerificationInformation {
    u32 max_layers;
    std::array<HierarchicalIntegrityVerificationLevelInformation, IntegrityMaxLayerCount - 1> info;
    HashSalt seed;

    s64 GetLayeredHashSize() const {
        return this->info[this->max_layers - 2].offset;
    }

    s64 GetDataOffset() const {
        return this->info[this->max_layers - 2].offset;
    }

    s64 GetDataSize() const {
        return this->info[this->max_layers - 2].size;
    }
};
static_assert(std::is_trivial_v<HierarchicalIntegrityVerificationInformation>);

struct HierarchicalIntegrityVerificationMetaInformation {
    u32 magic;
    u32 version;
    u32 master_hash_size;
    HierarchicalIntegrityVerificationInformation level_hash_info;
};
static_assert(std::is_trivial_v<HierarchicalIntegrityVerificationMetaInformation>);

struct HierarchicalIntegrityVerificationSizeSet {
    s64 control_size;
    s64 master_hash_size;
    std::array<s64, IntegrityMaxLayerCount - 2> layered_hash_sizes;
};
static_assert(std::is_trivial_v<HierarchicalIntegrityVerificationSizeSet>);

class HierarchicalIntegrityVerificationStorage : public IReadOnlyStorage {
    SUDACHI_NON_COPYABLE(HierarchicalIntegrityVerificationStorage);
    SUDACHI_NON_MOVEABLE(HierarchicalIntegrityVerificationStorage);

public:
    using GenerateRandomFunction = void (*)(void* dst, size_t size);

    class HierarchicalStorageInformation {
    public:
        enum {
            MasterStorage = 0,
            Layer1Storage = 1,
            Layer2Storage = 2,
            Layer3Storage = 3,
            Layer4Storage = 4,
            Layer5Storage = 5,
            DataStorage = 6,
        };

    private:
        std::array<VirtualFile, DataStorage + 1> m_storages;

    public:
        void SetMasterHashStorage(VirtualFile s) {
            m_storages[MasterStorage] = s;
        }
        void SetLayer1HashStorage(VirtualFile s) {
            m_storages[Layer1Storage] = s;
        }
        void SetLayer2HashStorage(VirtualFile s) {
            m_storages[Layer2Storage] = s;
        }
        void SetLayer3HashStorage(VirtualFile s) {
            m_storages[Layer3Storage] = s;
        }
        void SetLayer4HashStorage(VirtualFile s) {
            m_storages[Layer4Storage] = s;
        }
        void SetLayer5HashStorage(VirtualFile s) {
            m_storages[Layer5Storage] = s;
        }
        void SetDataStorage(VirtualFile s) {
            m_storages[DataStorage] = s;
        }

        VirtualFile& operator[](s32 index) {
            ASSERT(MasterStorage <= index && index <= DataStorage);
            return m_storages[index];
        }
    };

public:
    HierarchicalIntegrityVerificationStorage();
    virtual ~HierarchicalIntegrityVerificationStorage() override {
        this->Finalize();
    }

    Result Initialize(const HierarchicalIntegrityVerificationInformation& info,
                      HierarchicalStorageInformation storage, int max_data_cache_entries,
                      int max_hash_cache_entries, s8 buffer_level);
    void Finalize();

    virtual size_t Read(u8* buffer, size_t size, size_t offset) const override;
    virtual size_t GetSize() const override;

    bool IsInitialized() const {
        return m_data_size >= 0;
    }

    s64 GetL1HashVerificationBlockSize() const {
        return m_verify_storages[m_max_layers - 2]->GetBlockSize();
    }

    VirtualFile GetL1HashStorage() {
        return std::make_shared<OffsetVfsFile>(
            m_buffer_storages[m_max_layers - 3],
            Common::DivideUp(m_data_size, this->GetL1HashVerificationBlockSize()), 0);
    }

public:
    static constexpr s8 GetDefaultDataCacheBufferLevel(u32 max_layers) {
        return static_cast<s8>(16 + max_layers - 2);
    }

protected:
    static constexpr s64 HashSize = 256 / 8;
    static constexpr size_t MaxLayers = IntegrityMaxLayerCount;

private:
    static GenerateRandomFunction s_generate_random;

    static void SetGenerateRandomFunction(GenerateRandomFunction func) {
        s_generate_random = func;
    }

private:
    friend struct HierarchicalIntegrityVerificationMetaInformation;

private:
    std::array<std::shared_ptr<IntegrityVerificationStorage>, MaxLayers - 1> m_verify_storages;
    std::array<VirtualFile, MaxLayers - 1> m_buffer_storages;
    s64 m_data_size;
    s32 m_max_layers;
};

} // namespace FileSys
