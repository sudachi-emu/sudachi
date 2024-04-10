// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "core/file_sys/fssystem/fssystem_indirect_storage.h"

namespace FileSys {

class SparseStorage : public IndirectStorage {
    SUDACHI_NON_COPYABLE(SparseStorage);
    SUDACHI_NON_MOVEABLE(SparseStorage);

private:
    class ZeroStorage : public IReadOnlyStorage {
    public:
        ZeroStorage() {}
        virtual ~ZeroStorage() {}

        virtual size_t GetSize() const override {
            return std::numeric_limits<size_t>::max();
        }

        virtual size_t Read(u8* buffer, size_t size, size_t offset) const override {
            ASSERT(buffer != nullptr || size == 0);

            if (size > 0) {
                std::memset(buffer, 0, size);
            }

            return size;
        }
    };

public:
    SparseStorage() : IndirectStorage(), m_zero_storage(std::make_shared<ZeroStorage>()) {}
    virtual ~SparseStorage() {}

    using IndirectStorage::Initialize;

    void Initialize(s64 end_offset) {
        this->GetEntryTable().Initialize(NodeSize, end_offset);
        this->SetZeroStorage();
    }

    void SetDataStorage(VirtualFile storage) {
        ASSERT(this->IsInitialized());

        this->SetStorage(0, storage);
        this->SetZeroStorage();
    }

    template <typename T>
    void SetDataStorage(T storage, s64 offset, s64 size) {
        ASSERT(this->IsInitialized());

        this->SetStorage(0, storage, offset, size);
        this->SetZeroStorage();
    }

    virtual size_t Read(u8* buffer, size_t size, size_t offset) const override;

private:
    void SetZeroStorage() {
        return this->SetStorage(1, m_zero_storage, 0, std::numeric_limits<s64>::max());
    }

private:
    VirtualFile m_zero_storage;
};

} // namespace FileSys
