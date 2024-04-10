// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <mutex>

#include "common/alignment.h"
#include "common/common_funcs.h"
#include "common/common_types.h"
#include "common/literals.h"

#include "core/file_sys/vfs/vfs.h"
#include "core/hle/result.h"

namespace FileSys {

using namespace Common::Literals;

class BucketTree {
    SUDACHI_NON_COPYABLE(BucketTree);
    SUDACHI_NON_MOVEABLE(BucketTree);

public:
    static constexpr u32 Magic = Common::MakeMagic('B', 'K', 'T', 'R');
    static constexpr u32 Version = 1;

    static constexpr size_t NodeSizeMin = 1_KiB;
    static constexpr size_t NodeSizeMax = 512_KiB;

public:
    class Visitor;

    struct Header {
        u32 magic;
        u32 version;
        s32 entry_count;
        s32 reserved;

        void Format(s32 entry_count);
        Result Verify() const;
    };
    static_assert(std::is_trivial_v<Header>);
    static_assert(sizeof(Header) == 0x10);

    struct NodeHeader {
        s32 index;
        s32 count;
        s64 offset;

        Result Verify(s32 node_index, size_t node_size, size_t entry_size) const;
    };
    static_assert(std::is_trivial_v<NodeHeader>);
    static_assert(sizeof(NodeHeader) == 0x10);

    struct Offsets {
        s64 start_offset;
        s64 end_offset;

        constexpr bool IsInclude(s64 offset) const {
            return this->start_offset <= offset && offset < this->end_offset;
        }

        constexpr bool IsInclude(s64 offset, s64 size) const {
            return size > 0 && this->start_offset <= offset && size <= (this->end_offset - offset);
        }
    };
    static_assert(std::is_trivial_v<Offsets>);
    static_assert(sizeof(Offsets) == 0x10);

    struct OffsetCache {
        Offsets offsets;
        std::mutex mutex;
        bool is_initialized;

        OffsetCache() : offsets{-1, -1}, mutex(), is_initialized(false) {}
    };

    class ContinuousReadingInfo {
    public:
        constexpr ContinuousReadingInfo() : m_read_size(), m_skip_count(), m_done() {}

        constexpr void Reset() {
            m_read_size = 0;
            m_skip_count = 0;
            m_done = false;
        }

        constexpr void SetSkipCount(s32 count) {
            ASSERT(count >= 0);
            m_skip_count = count;
        }
        constexpr s32 GetSkipCount() const {
            return m_skip_count;
        }
        constexpr bool CheckNeedScan() {
            return (--m_skip_count) <= 0;
        }

        constexpr void Done() {
            m_read_size = 0;
            m_done = true;
        }
        constexpr bool IsDone() const {
            return m_done;
        }

        constexpr void SetReadSize(size_t size) {
            m_read_size = size;
        }
        constexpr size_t GetReadSize() const {
            return m_read_size;
        }
        constexpr bool CanDo() const {
            return m_read_size > 0;
        }

    private:
        size_t m_read_size;
        s32 m_skip_count;
        bool m_done;
    };

private:
    class NodeBuffer {
        SUDACHI_NON_COPYABLE(NodeBuffer);

    public:
        NodeBuffer() : m_header() {}

        ~NodeBuffer() {
            ASSERT(m_header == nullptr);
        }

        NodeBuffer(NodeBuffer&& rhs) : m_header(rhs.m_header) {
            rhs.m_header = nullptr;
        }

        NodeBuffer& operator=(NodeBuffer&& rhs) {
            if (this != std::addressof(rhs)) {
                ASSERT(m_header == nullptr);

                m_header = rhs.m_header;

                rhs.m_header = nullptr;
            }
            return *this;
        }

        bool Allocate(size_t node_size) {
            ASSERT(m_header == nullptr);

            m_header = ::operator new(node_size, std::align_val_t{sizeof(s64)});

            // ASSERT(Common::IsAligned(m_header, sizeof(s64)));

            return m_header != nullptr;
        }

        void Free(size_t node_size) {
            if (m_header) {
                ::operator delete(m_header, std::align_val_t{sizeof(s64)});
                m_header = nullptr;
            }
        }

        void FillZero(size_t node_size) const {
            if (m_header) {
                std::memset(m_header, 0, node_size);
            }
        }

        NodeHeader* Get() const {
            return reinterpret_cast<NodeHeader*>(m_header);
        }

        NodeHeader* operator->() const {
            return this->Get();
        }

        template <typename T>
        T* Get() const {
            static_assert(std::is_trivial_v<T>);
            static_assert(sizeof(T) == sizeof(NodeHeader));
            return reinterpret_cast<T*>(m_header);
        }

    private:
        void* m_header;
    };

private:
    static constexpr s32 GetEntryCount(size_t node_size, size_t entry_size) {
        return static_cast<s32>((node_size - sizeof(NodeHeader)) / entry_size);
    }

    static constexpr s32 GetOffsetCount(size_t node_size) {
        return static_cast<s32>((node_size - sizeof(NodeHeader)) / sizeof(s64));
    }

    static constexpr s32 GetEntrySetCount(size_t node_size, size_t entry_size, s32 entry_count) {
        const s32 entry_count_per_node = GetEntryCount(node_size, entry_size);
        return Common::DivideUp(entry_count, entry_count_per_node);
    }

    static constexpr s32 GetNodeL2Count(size_t node_size, size_t entry_size, s32 entry_count) {
        const s32 offset_count_per_node = GetOffsetCount(node_size);
        const s32 entry_set_count = GetEntrySetCount(node_size, entry_size, entry_count);

        if (entry_set_count <= offset_count_per_node) {
            return 0;
        }

        const s32 node_l2_count = Common::DivideUp(entry_set_count, offset_count_per_node);
        ASSERT(node_l2_count <= offset_count_per_node);

        return Common::DivideUp(entry_set_count - (offset_count_per_node - (node_l2_count - 1)),
                                offset_count_per_node);
    }

public:
    BucketTree()
        : m_node_storage(), m_entry_storage(), m_node_l1(), m_node_size(), m_entry_size(),
          m_entry_count(), m_offset_count(), m_entry_set_count(), m_offset_cache() {}
    ~BucketTree() {
        this->Finalize();
    }

    Result Initialize(VirtualFile node_storage, VirtualFile entry_storage, size_t node_size,
                      size_t entry_size, s32 entry_count);
    void Initialize(size_t node_size, s64 end_offset);
    void Finalize();

    bool IsInitialized() const {
        return m_node_size > 0;
    }
    bool IsEmpty() const {
        return m_entry_size == 0;
    }

    Result Find(Visitor* visitor, s64 virtual_address);
    Result InvalidateCache();

    s32 GetEntryCount() const {
        return m_entry_count;
    }

    Result GetOffsets(Offsets* out) {
        // Ensure we have an offset cache.
        R_TRY(this->EnsureOffsetCache());

        // Set the output.
        *out = m_offset_cache.offsets;
        R_SUCCEED();
    }

public:
    static constexpr s64 QueryHeaderStorageSize() {
        return sizeof(Header);
    }

    static constexpr s64 QueryNodeStorageSize(size_t node_size, size_t entry_size,
                                              s32 entry_count) {
        ASSERT(entry_size >= sizeof(s64));
        ASSERT(node_size >= entry_size + sizeof(NodeHeader));
        ASSERT(NodeSizeMin <= node_size && node_size <= NodeSizeMax);
        ASSERT(Common::IsPowerOfTwo(node_size));
        ASSERT(entry_count >= 0);

        if (entry_count <= 0) {
            return 0;
        }
        return (1 + GetNodeL2Count(node_size, entry_size, entry_count)) *
               static_cast<s64>(node_size);
    }

    static constexpr s64 QueryEntryStorageSize(size_t node_size, size_t entry_size,
                                               s32 entry_count) {
        ASSERT(entry_size >= sizeof(s64));
        ASSERT(node_size >= entry_size + sizeof(NodeHeader));
        ASSERT(NodeSizeMin <= node_size && node_size <= NodeSizeMax);
        ASSERT(Common::IsPowerOfTwo(node_size));
        ASSERT(entry_count >= 0);

        if (entry_count <= 0) {
            return 0;
        }
        return GetEntrySetCount(node_size, entry_size, entry_count) * static_cast<s64>(node_size);
    }

private:
    template <typename EntryType>
    struct ContinuousReadingParam {
        s64 offset;
        size_t size;
        NodeHeader entry_set;
        s32 entry_index;
        Offsets offsets;
        EntryType entry;
    };

private:
    template <typename EntryType>
    Result ScanContinuousReading(ContinuousReadingInfo* out_info,
                                 const ContinuousReadingParam<EntryType>& param) const;

    bool IsExistL2() const {
        return m_offset_count < m_entry_set_count;
    }
    bool IsExistOffsetL2OnL1() const {
        return this->IsExistL2() && m_node_l1->count < m_offset_count;
    }

    s64 GetEntrySetIndex(s32 node_index, s32 offset_index) const {
        return (m_offset_count - m_node_l1->count) + (m_offset_count * node_index) + offset_index;
    }

    Result EnsureOffsetCache();

private:
    mutable VirtualFile m_node_storage;
    mutable VirtualFile m_entry_storage;
    NodeBuffer m_node_l1;
    size_t m_node_size;
    size_t m_entry_size;
    s32 m_entry_count;
    s32 m_offset_count;
    s32 m_entry_set_count;
    OffsetCache m_offset_cache;
};

class BucketTree::Visitor {
    SUDACHI_NON_COPYABLE(Visitor);
    SUDACHI_NON_MOVEABLE(Visitor);

public:
    constexpr Visitor()
        : m_tree(), m_entry(), m_entry_index(-1), m_entry_set_count(), m_entry_set{} {}
    ~Visitor() {
        if (m_entry != nullptr) {
            ::operator delete(m_entry, m_tree->m_entry_size);
            m_tree = nullptr;
            m_entry = nullptr;
        }
    }

    bool IsValid() const {
        return m_entry_index >= 0;
    }
    bool CanMoveNext() const {
        return this->IsValid() && (m_entry_index + 1 < m_entry_set.info.count ||
                                   m_entry_set.info.index + 1 < m_entry_set_count);
    }
    bool CanMovePrevious() const {
        return this->IsValid() && (m_entry_index > 0 || m_entry_set.info.index > 0);
    }

    Result MoveNext();
    Result MovePrevious();

    template <typename EntryType>
    Result ScanContinuousReading(ContinuousReadingInfo* out_info, s64 offset, size_t size) const;

    const void* Get() const {
        ASSERT(this->IsValid());
        return m_entry;
    }

    template <typename T>
    const T* Get() const {
        ASSERT(this->IsValid());
        return reinterpret_cast<const T*>(m_entry);
    }

    const BucketTree* GetTree() const {
        return m_tree;
    }

private:
    Result Initialize(const BucketTree* tree, const BucketTree::Offsets& offsets);

    Result Find(s64 virtual_address);

    Result FindEntrySet(s32* out_index, s64 virtual_address, s32 node_index);
    Result FindEntrySetWithBuffer(s32* out_index, s64 virtual_address, s32 node_index,
                                  char* buffer);
    Result FindEntrySetWithoutBuffer(s32* out_index, s64 virtual_address, s32 node_index);

    Result FindEntry(s64 virtual_address, s32 entry_set_index);
    Result FindEntryWithBuffer(s64 virtual_address, s32 entry_set_index, char* buffer);
    Result FindEntryWithoutBuffer(s64 virtual_address, s32 entry_set_index);

private:
    friend class BucketTree;

    union EntrySetHeader {
        NodeHeader header;
        struct Info {
            s32 index;
            s32 count;
            s64 end;
            s64 start;
        } info;
        static_assert(std::is_trivial_v<Info>);
    };
    static_assert(std::is_trivial_v<EntrySetHeader>);

    const BucketTree* m_tree;
    BucketTree::Offsets m_offsets;
    void* m_entry;
    s32 m_entry_index;
    s32 m_entry_set_count;
    EntrySetHeader m_entry_set;
};

} // namespace FileSys
