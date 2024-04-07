// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "core/file_sys/errors.h"
#include "core/file_sys/fssystem/fssystem_bucket_tree.h"
#include "core/file_sys/fssystem/fssystem_bucket_tree_utils.h"
#include "core/file_sys/fssystem/fssystem_pooled_buffer.h"

namespace FileSys {

template <typename EntryType>
Result BucketTree::ScanContinuousReading(ContinuousReadingInfo* out_info,
                                         const ContinuousReadingParam<EntryType>& param) const {
    static_assert(std::is_trivial_v<ContinuousReadingParam<EntryType>>);

    // Validate our preconditions.
    ASSERT(this->IsInitialized());
    ASSERT(out_info != nullptr);
    ASSERT(m_entry_size == sizeof(EntryType));

    // Reset the output.
    out_info->Reset();

    // If there's nothing to read, we're done.
    R_SUCCEED_IF(param.size == 0);

    // If we're reading a fragment, we're done.
    R_SUCCEED_IF(param.entry.IsFragment());

    // Validate the first entry.
    auto entry = param.entry;
    auto cur_offset = param.offset;
    R_UNLESS(entry.GetVirtualOffset() <= cur_offset, ResultOutOfRange);

    // Create a pooled buffer for our scan.
    PooledBuffer pool(m_node_size, 1);
    char* buffer = nullptr;

    s64 entry_storage_size = m_entry_storage->GetSize();

    // Read the node.
    if (m_node_size <= pool.GetSize()) {
        buffer = pool.GetBuffer();
        const auto ofs = param.entry_set.index * static_cast<s64>(m_node_size);
        R_UNLESS(m_node_size + ofs <= static_cast<size_t>(entry_storage_size),
                 ResultInvalidBucketTreeNodeEntryCount);

        m_entry_storage->Read(reinterpret_cast<u8*>(buffer), m_node_size, ofs);
    }

    // Calculate extents.
    const auto end_offset = cur_offset + static_cast<s64>(param.size);
    s64 phys_offset = entry.GetPhysicalOffset();

    // Start merge tracking.
    s64 merge_size = 0;
    s64 readable_size = 0;
    bool merged = false;

    // Iterate.
    auto entry_index = param.entry_index;
    for (const auto entry_count = param.entry_set.count; entry_index < entry_count; ++entry_index) {
        // If we're past the end, we're done.
        if (end_offset <= cur_offset) {
            break;
        }

        // Validate the entry offset.
        const auto entry_offset = entry.GetVirtualOffset();
        R_UNLESS(entry_offset <= cur_offset, ResultInvalidIndirectEntryOffset);

        // Get the next entry.
        EntryType next_entry = {};
        s64 next_entry_offset;

        if (entry_index + 1 < entry_count) {
            if (buffer != nullptr) {
                const auto ofs = impl::GetBucketTreeEntryOffset(0, m_entry_size, entry_index + 1);
                std::memcpy(std::addressof(next_entry), buffer + ofs, m_entry_size);
            } else {
                const auto ofs = impl::GetBucketTreeEntryOffset(param.entry_set.index, m_node_size,
                                                                m_entry_size, entry_index + 1);
                m_entry_storage->ReadObject(std::addressof(next_entry), ofs);
            }

            next_entry_offset = next_entry.GetVirtualOffset();
            R_UNLESS(param.offsets.IsInclude(next_entry_offset), ResultInvalidIndirectEntryOffset);
        } else {
            next_entry_offset = param.entry_set.offset;
        }

        // Validate the next entry offset.
        R_UNLESS(cur_offset < next_entry_offset, ResultInvalidIndirectEntryOffset);

        // Determine the much data there is.
        const auto data_size = next_entry_offset - cur_offset;
        ASSERT(data_size > 0);

        // Determine how much data we should read.
        const auto remaining_size = end_offset - cur_offset;
        const size_t read_size = static_cast<size_t>(std::min(data_size, remaining_size));
        ASSERT(read_size <= param.size);

        // Update our merge tracking.
        if (entry.IsFragment()) {
            // If we can't merge, stop looping.
            if (EntryType::FragmentSizeMax <= read_size || remaining_size <= data_size) {
                break;
            }

            // Otherwise, add the current size to the merge size.
            merge_size += read_size;
        } else {
            //  If we can't merge, stop looping.
            if (phys_offset != entry.GetPhysicalOffset()) {
                break;
            }

            // Add the size to the readable amount.
            readable_size += merge_size + read_size;
            ASSERT(readable_size <= static_cast<s64>(param.size));

            // Update whether we've merged.
            merged |= merge_size > 0;
            merge_size = 0;
        }

        // Advance.
        cur_offset += read_size;
        ASSERT(cur_offset <= end_offset);

        phys_offset += next_entry_offset - entry_offset;
        entry = next_entry;
    }

    // If we merged, set our readable size.
    if (merged) {
        out_info->SetReadSize(static_cast<size_t>(readable_size));
    }
    out_info->SetSkipCount(entry_index - param.entry_index);

    R_SUCCEED();
}

template <typename EntryType>
Result BucketTree::Visitor::ScanContinuousReading(ContinuousReadingInfo* out_info, s64 offset,
                                                  size_t size) const {
    static_assert(std::is_trivial_v<EntryType>);
    ASSERT(this->IsValid());

    // Create our parameters.
    ContinuousReadingParam<EntryType> param = {
        .offset = offset,
        .size = size,
        .entry_set = m_entry_set.header,
        .entry_index = m_entry_index,
        .offsets{},
        .entry{},
    };
    std::memcpy(std::addressof(param.offsets), std::addressof(m_offsets),
                sizeof(BucketTree::Offsets));
    std::memcpy(std::addressof(param.entry), m_entry, sizeof(EntryType));

    // Scan.
    R_RETURN(m_tree->ScanContinuousReading<EntryType>(out_info, param));
}

} // namespace FileSys
