// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "core/file_sys/fssystem/fssystem_bucket_tree.h"

namespace FileSys::impl {

class SafeValue {
public:
    static s64 GetInt64(const void* ptr) {
        s64 value;
        std::memcpy(std::addressof(value), ptr, sizeof(s64));
        return value;
    }

    static s64 GetInt64(const s64* ptr) {
        return GetInt64(static_cast<const void*>(ptr));
    }

    static s64 GetInt64(const s64& v) {
        return GetInt64(std::addressof(v));
    }

    static void SetInt64(void* dst, const void* src) {
        std::memcpy(dst, src, sizeof(s64));
    }

    static void SetInt64(void* dst, const s64* src) {
        return SetInt64(dst, static_cast<const void*>(src));
    }

    static void SetInt64(void* dst, const s64& v) {
        return SetInt64(dst, std::addressof(v));
    }
};

template <typename IteratorType>
struct BucketTreeNode {
    using Header = BucketTree::NodeHeader;

    Header header;

    s32 GetCount() const {
        return this->header.count;
    }

    void* GetArray() {
        return std::addressof(this->header) + 1;
    }
    template <typename T>
    T* GetArray() {
        return reinterpret_cast<T*>(this->GetArray());
    }
    const void* GetArray() const {
        return std::addressof(this->header) + 1;
    }
    template <typename T>
    const T* GetArray() const {
        return reinterpret_cast<const T*>(this->GetArray());
    }

    s64 GetBeginOffset() const {
        return *this->GetArray<s64>();
    }
    s64 GetEndOffset() const {
        return this->header.offset;
    }

    IteratorType GetBegin() {
        return IteratorType(this->GetArray<s64>());
    }
    IteratorType GetEnd() {
        return IteratorType(this->GetArray<s64>()) + this->header.count;
    }
    IteratorType GetBegin() const {
        return IteratorType(this->GetArray<s64>());
    }
    IteratorType GetEnd() const {
        return IteratorType(this->GetArray<s64>()) + this->header.count;
    }

    IteratorType GetBegin(size_t entry_size) {
        return IteratorType(this->GetArray(), entry_size);
    }
    IteratorType GetEnd(size_t entry_size) {
        return IteratorType(this->GetArray(), entry_size) + this->header.count;
    }
    IteratorType GetBegin(size_t entry_size) const {
        return IteratorType(this->GetArray(), entry_size);
    }
    IteratorType GetEnd(size_t entry_size) const {
        return IteratorType(this->GetArray(), entry_size) + this->header.count;
    }
};

constexpr inline s64 GetBucketTreeEntryOffset(s64 entry_set_offset, size_t entry_size,
                                              s32 entry_index) {
    return entry_set_offset + sizeof(BucketTree::NodeHeader) +
           entry_index * static_cast<s64>(entry_size);
}

constexpr inline s64 GetBucketTreeEntryOffset(s32 entry_set_index, size_t node_size,
                                              size_t entry_size, s32 entry_index) {
    return GetBucketTreeEntryOffset(entry_set_index * static_cast<s64>(node_size), entry_size,
                                    entry_index);
}

} // namespace FileSys::impl
