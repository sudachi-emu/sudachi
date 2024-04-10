// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/file_sys/fssystem/fssystem_sparse_storage.h"

namespace FileSys {

size_t SparseStorage::Read(u8* buffer, size_t size, size_t offset) const {
    // Validate preconditions.
    ASSERT(this->IsInitialized());
    ASSERT(buffer != nullptr);

    // Allow zero size.
    if (size == 0) {
        return size;
    }

    SparseStorage* self = const_cast<SparseStorage*>(this);

    if (self->GetEntryTable().IsEmpty()) {
        BucketTree::Offsets table_offsets;
        ASSERT(R_SUCCEEDED(self->GetEntryTable().GetOffsets(std::addressof(table_offsets))));
        ASSERT(table_offsets.IsInclude(offset, size));

        std::memset(buffer, 0, size);
    } else {
        self->OperatePerEntry<false, true>(
            offset, size,
            [=](VirtualFile storage, s64 data_offset, s64 cur_offset, s64 cur_size) -> Result {
                storage->Read(reinterpret_cast<u8*>(buffer) + (cur_offset - offset),
                              static_cast<size_t>(cur_size), data_offset);
                R_SUCCEED();
            });
    }

    return size;
}

} // namespace FileSys
