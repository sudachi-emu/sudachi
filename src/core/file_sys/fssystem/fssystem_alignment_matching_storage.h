// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "common/alignment.h"
#include "core/file_sys/errors.h"
#include "core/file_sys/fssystem/fs_i_storage.h"
#include "core/file_sys/fssystem/fssystem_alignment_matching_storage_impl.h"
#include "core/file_sys/fssystem/fssystem_pooled_buffer.h"

namespace FileSys {

template <size_t DataAlign_, size_t BufferAlign_>
class AlignmentMatchingStorage : public IStorage {
    SUDACHI_NON_COPYABLE(AlignmentMatchingStorage);
    SUDACHI_NON_MOVEABLE(AlignmentMatchingStorage);

public:
    static constexpr size_t DataAlign = DataAlign_;
    static constexpr size_t BufferAlign = BufferAlign_;

    static constexpr size_t DataAlignMax = 0x200;
    static_assert(DataAlign <= DataAlignMax);
    static_assert(Common::IsPowerOfTwo(DataAlign));
    static_assert(Common::IsPowerOfTwo(BufferAlign));

private:
    VirtualFile m_base_storage;
    s64 m_base_storage_size;

public:
    explicit AlignmentMatchingStorage(VirtualFile bs) : m_base_storage(std::move(bs)) {}

    virtual size_t Read(u8* buffer, size_t size, size_t offset) const override {
        // Allocate a work buffer on stack.
        alignas(DataAlignMax) std::array<char, DataAlign> work_buf;

        // Succeed if zero size.
        if (size == 0) {
            return size;
        }

        // Validate arguments.
        ASSERT(buffer != nullptr);

        s64 bs_size = this->GetSize();
        ASSERT(R_SUCCEEDED(IStorage::CheckAccessRange(offset, size, bs_size)));

        return AlignmentMatchingStorageImpl::Read(m_base_storage, work_buf.data(), work_buf.size(),
                                                  DataAlign, BufferAlign, offset, buffer, size);
    }

    virtual size_t Write(const u8* buffer, size_t size, size_t offset) override {
        // Allocate a work buffer on stack.
        alignas(DataAlignMax) std::array<char, DataAlign> work_buf;

        // Succeed if zero size.
        if (size == 0) {
            return size;
        }

        // Validate arguments.
        ASSERT(buffer != nullptr);

        s64 bs_size = this->GetSize();
        ASSERT(R_SUCCEEDED(IStorage::CheckAccessRange(offset, size, bs_size)));

        return AlignmentMatchingStorageImpl::Write(m_base_storage, work_buf.data(), work_buf.size(),
                                                   DataAlign, BufferAlign, offset, buffer, size);
    }

    virtual size_t GetSize() const override {
        return m_base_storage->GetSize();
    }
};

template <size_t BufferAlign_>
class AlignmentMatchingStoragePooledBuffer : public IStorage {
    SUDACHI_NON_COPYABLE(AlignmentMatchingStoragePooledBuffer);
    SUDACHI_NON_MOVEABLE(AlignmentMatchingStoragePooledBuffer);

public:
    static constexpr size_t BufferAlign = BufferAlign_;

    static_assert(Common::IsPowerOfTwo(BufferAlign));

private:
    VirtualFile m_base_storage;
    s64 m_base_storage_size;
    size_t m_data_align;

public:
    explicit AlignmentMatchingStoragePooledBuffer(VirtualFile bs, size_t da)
        : m_base_storage(std::move(bs)), m_data_align(da) {
        ASSERT(Common::IsPowerOfTwo(da));
    }

    virtual size_t Read(u8* buffer, size_t size, size_t offset) const override {
        // Succeed if zero size.
        if (size == 0) {
            return size;
        }

        // Validate arguments.
        ASSERT(buffer != nullptr);

        s64 bs_size = this->GetSize();
        ASSERT(R_SUCCEEDED(IStorage::CheckAccessRange(offset, size, bs_size)));

        // Allocate a pooled buffer.
        PooledBuffer pooled_buffer;
        pooled_buffer.AllocateParticularlyLarge(m_data_align, m_data_align);

        return AlignmentMatchingStorageImpl::Read(m_base_storage, pooled_buffer.GetBuffer(),
                                                  pooled_buffer.GetSize(), m_data_align,
                                                  BufferAlign, offset, buffer, size);
    }

    virtual size_t Write(const u8* buffer, size_t size, size_t offset) override {
        // Succeed if zero size.
        if (size == 0) {
            return size;
        }

        // Validate arguments.
        ASSERT(buffer != nullptr);

        s64 bs_size = this->GetSize();
        ASSERT(R_SUCCEEDED(IStorage::CheckAccessRange(offset, size, bs_size)));

        // Allocate a pooled buffer.
        PooledBuffer pooled_buffer;
        pooled_buffer.AllocateParticularlyLarge(m_data_align, m_data_align);

        return AlignmentMatchingStorageImpl::Write(m_base_storage, pooled_buffer.GetBuffer(),
                                                   pooled_buffer.GetSize(), m_data_align,
                                                   BufferAlign, offset, buffer, size);
    }

    virtual size_t GetSize() const override {
        return m_base_storage->GetSize();
    }
};

} // namespace FileSys
