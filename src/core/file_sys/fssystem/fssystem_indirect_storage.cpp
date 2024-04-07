// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/file_sys/errors.h"
#include "core/file_sys/fssystem/fssystem_indirect_storage.h"

namespace FileSys {

Result IndirectStorage::Initialize(VirtualFile table_storage) {
    // Read and verify the bucket tree header.
    BucketTree::Header header;
    table_storage->ReadObject(std::addressof(header));
    R_TRY(header.Verify());

    // Determine extents.
    const auto node_storage_size = QueryNodeStorageSize(header.entry_count);
    const auto entry_storage_size = QueryEntryStorageSize(header.entry_count);
    const auto node_storage_offset = QueryHeaderStorageSize();
    const auto entry_storage_offset = node_storage_offset + node_storage_size;

    // Initialize.
    R_RETURN(this->Initialize(
        std::make_shared<OffsetVfsFile>(table_storage, node_storage_size, node_storage_offset),
        std::make_shared<OffsetVfsFile>(table_storage, entry_storage_size, entry_storage_offset),
        header.entry_count));
}

void IndirectStorage::Finalize() {
    if (this->IsInitialized()) {
        m_table.Finalize();
        for (auto i = 0; i < StorageCount; i++) {
            m_data_storage[i] = VirtualFile();
        }
    }
}

Result IndirectStorage::GetEntryList(Entry* out_entries, s32* out_entry_count, s32 entry_count,
                                     s64 offset, s64 size) {
    // Validate pre-conditions.
    ASSERT(offset >= 0);
    ASSERT(size >= 0);
    ASSERT(this->IsInitialized());

    // Clear the out count.
    R_UNLESS(out_entry_count != nullptr, ResultNullptrArgument);
    *out_entry_count = 0;

    // Succeed if there's no range.
    R_SUCCEED_IF(size == 0);

    // If we have an output array, we need it to be non-null.
    R_UNLESS(out_entries != nullptr || entry_count == 0, ResultNullptrArgument);

    // Check that our range is valid.
    BucketTree::Offsets table_offsets;
    R_TRY(m_table.GetOffsets(std::addressof(table_offsets)));

    R_UNLESS(table_offsets.IsInclude(offset, size), ResultOutOfRange);

    // Find the offset in our tree.
    BucketTree::Visitor visitor;
    R_TRY(m_table.Find(std::addressof(visitor), offset));
    {
        const auto entry_offset = visitor.Get<Entry>()->GetVirtualOffset();
        R_UNLESS(0 <= entry_offset && table_offsets.IsInclude(entry_offset),
                 ResultInvalidIndirectEntryOffset);
    }

    // Prepare to loop over entries.
    const auto end_offset = offset + static_cast<s64>(size);
    s32 count = 0;

    auto cur_entry = *visitor.Get<Entry>();
    while (cur_entry.GetVirtualOffset() < end_offset) {
        // Try to write the entry to the out list.
        if (entry_count != 0) {
            if (count >= entry_count) {
                break;
            }
            std::memcpy(out_entries + count, std::addressof(cur_entry), sizeof(Entry));
        }

        count++;

        // Advance.
        if (visitor.CanMoveNext()) {
            R_TRY(visitor.MoveNext());
            cur_entry = *visitor.Get<Entry>();
        } else {
            break;
        }
    }

    // Write the output count.
    *out_entry_count = count;
    R_SUCCEED();
}

size_t IndirectStorage::Read(u8* buffer, size_t size, size_t offset) const {
    // Validate pre-conditions.
    ASSERT(this->IsInitialized());
    ASSERT(buffer != nullptr);

    // Succeed if there's nothing to read.
    if (size == 0) {
        return 0;
    }

    const_cast<IndirectStorage*>(this)->OperatePerEntry<true, true>(
        offset, size,
        [=](VirtualFile storage, s64 data_offset, s64 cur_offset, s64 cur_size) -> Result {
            storage->Read(reinterpret_cast<u8*>(buffer) + (cur_offset - offset),
                          static_cast<size_t>(cur_size), data_offset);
            R_SUCCEED();
        });

    return size;
}
} // namespace FileSys
