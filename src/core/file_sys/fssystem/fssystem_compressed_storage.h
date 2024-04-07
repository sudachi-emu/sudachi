// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "common/literals.h"

#include "core/file_sys/errors.h"
#include "core/file_sys/fssystem/fs_i_storage.h"
#include "core/file_sys/fssystem/fssystem_bucket_tree.h"
#include "core/file_sys/fssystem/fssystem_compression_common.h"
#include "core/file_sys/fssystem/fssystem_pooled_buffer.h"
#include "core/file_sys/vfs/vfs.h"

namespace FileSys {

using namespace Common::Literals;

class CompressedStorage : public IReadOnlyStorage {
    SUDACHI_NON_COPYABLE(CompressedStorage);
    SUDACHI_NON_MOVEABLE(CompressedStorage);

public:
    static constexpr size_t NodeSize = 16_KiB;

    struct Entry {
        s64 virt_offset;
        s64 phys_offset;
        CompressionType compression_type;
        s32 phys_size;

        s64 GetPhysicalSize() const {
            return this->phys_size;
        }
    };
    static_assert(std::is_trivial_v<Entry>);
    static_assert(sizeof(Entry) == 0x18);

public:
    static constexpr s64 QueryNodeStorageSize(s32 entry_count) {
        return BucketTree::QueryNodeStorageSize(NodeSize, sizeof(Entry), entry_count);
    }

    static constexpr s64 QueryEntryStorageSize(s32 entry_count) {
        return BucketTree::QueryEntryStorageSize(NodeSize, sizeof(Entry), entry_count);
    }

private:
    class CompressedStorageCore {
        SUDACHI_NON_COPYABLE(CompressedStorageCore);
        SUDACHI_NON_MOVEABLE(CompressedStorageCore);

    public:
        CompressedStorageCore() : m_table(), m_data_storage() {}

        ~CompressedStorageCore() {
            this->Finalize();
        }

    public:
        Result Initialize(VirtualFile data_storage, VirtualFile node_storage,
                          VirtualFile entry_storage, s32 bktr_entry_count, size_t block_size_max,
                          size_t continuous_reading_size_max,
                          GetDecompressorFunction get_decompressor) {
            // Check pre-conditions.
            ASSERT(0 < block_size_max);
            ASSERT(block_size_max <= continuous_reading_size_max);
            ASSERT(get_decompressor != nullptr);

            // Initialize our entry table.
            R_TRY(m_table.Initialize(node_storage, entry_storage, NodeSize, sizeof(Entry),
                                     bktr_entry_count));

            // Set our other fields.
            m_block_size_max = block_size_max;
            m_continuous_reading_size_max = continuous_reading_size_max;
            m_data_storage = data_storage;
            m_get_decompressor_function = get_decompressor;

            R_SUCCEED();
        }

        void Finalize() {
            if (this->IsInitialized()) {
                m_table.Finalize();
                m_data_storage = VirtualFile();
            }
        }

        VirtualFile GetDataStorage() {
            return m_data_storage;
        }

        Result GetDataStorageSize(s64* out) {
            // Check pre-conditions.
            ASSERT(out != nullptr);

            // Get size.
            *out = m_data_storage->GetSize();

            R_SUCCEED();
        }

        BucketTree& GetEntryTable() {
            return m_table;
        }

        Result GetEntryList(Entry* out_entries, s32* out_read_count, s32 max_entry_count,
                            s64 offset, s64 size) {
            // Check pre-conditions.
            ASSERT(offset >= 0);
            ASSERT(size >= 0);
            ASSERT(this->IsInitialized());

            // Check that we can output the count.
            R_UNLESS(out_read_count != nullptr, ResultNullptrArgument);

            // Check that we have anything to read at all.
            R_SUCCEED_IF(size == 0);

            // Check that either we have a buffer, or this is to determine how many we need.
            if (max_entry_count != 0) {
                R_UNLESS(out_entries != nullptr, ResultNullptrArgument);
            }

            // Get the table offsets.
            BucketTree::Offsets table_offsets;
            R_TRY(m_table.GetOffsets(std::addressof(table_offsets)));

            // Validate arguments.
            R_UNLESS(table_offsets.IsInclude(offset, size), ResultOutOfRange);

            // Find the offset in our tree.
            BucketTree::Visitor visitor;
            R_TRY(m_table.Find(std::addressof(visitor), offset));
            {
                const auto entry_offset = visitor.Get<Entry>()->virt_offset;
                R_UNLESS(0 <= entry_offset && table_offsets.IsInclude(entry_offset),
                         ResultUnexpectedInCompressedStorageA);
            }

            // Get the entries.
            const auto end_offset = offset + size;
            s32 read_count = 0;
            while (visitor.Get<Entry>()->virt_offset < end_offset) {
                // If we should be setting the output, do so.
                if (max_entry_count != 0) {
                    // Ensure we only read as many entries as we can.
                    if (read_count >= max_entry_count) {
                        break;
                    }

                    // Set the current output entry.
                    out_entries[read_count] = *visitor.Get<Entry>();
                }

                // Increase the read count.
                ++read_count;

                // If we're at the end, we're done.
                if (!visitor.CanMoveNext()) {
                    break;
                }

                // Move to the next entry.
                R_TRY(visitor.MoveNext());
            }

            // Set the output read count.
            *out_read_count = read_count;
            R_SUCCEED();
        }

        Result GetSize(s64* out) {
            // Check pre-conditions.
            ASSERT(out != nullptr);

            // Get our table offsets.
            BucketTree::Offsets offsets;
            R_TRY(m_table.GetOffsets(std::addressof(offsets)));

            // Set the output.
            *out = offsets.end_offset;
            R_SUCCEED();
        }

        Result OperatePerEntry(s64 offset, s64 size, auto f) {
            // Check pre-conditions.
            ASSERT(offset >= 0);
            ASSERT(size >= 0);
            ASSERT(this->IsInitialized());

            // Succeed if there's nothing to operate on.
            R_SUCCEED_IF(size == 0);

            // Get the table offsets.
            BucketTree::Offsets table_offsets;
            R_TRY(m_table.GetOffsets(std::addressof(table_offsets)));

            // Validate arguments.
            R_UNLESS(table_offsets.IsInclude(offset, size), ResultOutOfRange);

            // Find the offset in our tree.
            BucketTree::Visitor visitor;
            R_TRY(m_table.Find(std::addressof(visitor), offset));
            {
                const auto entry_offset = visitor.Get<Entry>()->virt_offset;
                R_UNLESS(0 <= entry_offset && table_offsets.IsInclude(entry_offset),
                         ResultUnexpectedInCompressedStorageA);
            }

            // Prepare to operate in chunks.
            auto cur_offset = offset;
            const auto end_offset = offset + static_cast<s64>(size);

            while (cur_offset < end_offset) {
                // Get the current entry.
                const auto cur_entry = *visitor.Get<Entry>();

                // Get and validate the entry's offset.
                const auto cur_entry_offset = cur_entry.virt_offset;
                R_UNLESS(cur_entry_offset <= cur_offset, ResultUnexpectedInCompressedStorageA);

                // Get and validate the next entry offset.
                s64 next_entry_offset;
                if (visitor.CanMoveNext()) {
                    R_TRY(visitor.MoveNext());
                    next_entry_offset = visitor.Get<Entry>()->virt_offset;
                    R_UNLESS(table_offsets.IsInclude(next_entry_offset),
                             ResultUnexpectedInCompressedStorageA);
                } else {
                    next_entry_offset = table_offsets.end_offset;
                }
                R_UNLESS(cur_offset < next_entry_offset, ResultUnexpectedInCompressedStorageA);

                // Get the offset of the entry in the data we read.
                const auto data_offset = cur_offset - cur_entry_offset;
                const auto data_size = (next_entry_offset - cur_entry_offset);
                ASSERT(data_size > 0);

                // Determine how much is left.
                const auto remaining_size = end_offset - cur_offset;
                const auto cur_size = std::min<s64>(remaining_size, data_size - data_offset);
                ASSERT(cur_size <= size);

                // Get the data storage size.
                s64 storage_size = m_data_storage->GetSize();

                // Check that our read remains naively physically in bounds.
                R_UNLESS(0 <= cur_entry.phys_offset && cur_entry.phys_offset <= storage_size,
                         ResultUnexpectedInCompressedStorageC);

                // If we have any compression, verify that we remain physically in bounds.
                if (cur_entry.compression_type != CompressionType::None) {
                    R_UNLESS(cur_entry.phys_offset + cur_entry.GetPhysicalSize() <= storage_size,
                             ResultUnexpectedInCompressedStorageC);
                }

                // Check that block alignment requirements are met.
                if (CompressionTypeUtility::IsBlockAlignmentRequired(cur_entry.compression_type)) {
                    R_UNLESS(Common::IsAligned(cur_entry.phys_offset, CompressionBlockAlignment),
                             ResultUnexpectedInCompressedStorageA);
                }

                // Invoke the operator.
                bool is_continuous = true;
                R_TRY(
                    f(std::addressof(is_continuous), cur_entry, data_size, data_offset, cur_size));

                // If not continuous, we're done.
                if (!is_continuous) {
                    break;
                }

                // Advance.
                cur_offset += cur_size;
            }

            R_SUCCEED();
        }

    public:
        using ReadImplFunction = std::function<Result(void*, size_t)>;
        using ReadFunction = std::function<Result(size_t, const ReadImplFunction&)>;

    public:
        Result Read(s64 offset, s64 size, const ReadFunction& read_func) {
            // Check pre-conditions.
            ASSERT(offset >= 0);
            ASSERT(this->IsInitialized());

            // Succeed immediately, if we have nothing to read.
            R_SUCCEED_IF(size == 0);

            // Declare read lambda.
            constexpr int EntriesCountMax = 0x80;
            struct Entries {
                CompressionType compression_type;
                u32 gap_from_prev;
                u32 physical_size;
                u32 virtual_size;
            };
            std::array<Entries, EntriesCountMax> entries;
            s32 entry_count = 0;
            Entry prev_entry = {
                .virt_offset = -1,
                .phys_offset{},
                .compression_type{},
                .phys_size{},
            };
            bool will_allocate_pooled_buffer = false;
            s64 required_access_physical_offset = 0;
            s64 required_access_physical_size = 0;

            auto PerformRequiredRead = [&]() -> Result {
                // If there are no entries, we have nothing to do.
                R_SUCCEED_IF(entry_count == 0);

                // Get the remaining size in a convenient form.
                const size_t total_required_size =
                    static_cast<size_t>(required_access_physical_size);

                // Perform the read based on whether we need to allocate a buffer.
                if (will_allocate_pooled_buffer) {
                    // Allocate a pooled buffer.
                    PooledBuffer pooled_buffer;
                    if (pooled_buffer.GetAllocatableSizeMax() >= total_required_size) {
                        pooled_buffer.Allocate(total_required_size, m_block_size_max);
                    } else {
                        pooled_buffer.AllocateParticularlyLarge(
                            std::min<size_t>(
                                total_required_size,
                                PooledBuffer::GetAllocatableParticularlyLargeSizeMax()),
                            m_block_size_max);
                    }

                    // Read each of the entries.
                    for (s32 entry_idx = 0; entry_idx < entry_count; ++entry_idx) {
                        // Determine the current read size.
                        bool will_use_pooled_buffer = false;
                        const size_t cur_read_size = [&]() -> size_t {
                            if (const size_t target_entry_size =
                                    static_cast<size_t>(entries[entry_idx].physical_size) +
                                    static_cast<size_t>(entries[entry_idx].gap_from_prev);
                                target_entry_size <= pooled_buffer.GetSize()) {
                                // We'll be using the pooled buffer.
                                will_use_pooled_buffer = true;

                                // Determine how much we can read.
                                const size_t max_size = std::min<size_t>(
                                    required_access_physical_size, pooled_buffer.GetSize());

                                size_t read_size = 0;
                                for (auto n = entry_idx; n < entry_count; ++n) {
                                    const size_t cur_entry_size =
                                        static_cast<size_t>(entries[n].physical_size) +
                                        static_cast<size_t>(entries[n].gap_from_prev);
                                    if (read_size + cur_entry_size > max_size) {
                                        break;
                                    }

                                    read_size += cur_entry_size;
                                }

                                return read_size;
                            } else {
                                // If we don't fit, we must be uncompressed.
                                ASSERT(entries[entry_idx].compression_type ==
                                       CompressionType::None);

                                // We can perform the whole of an uncompressed read directly.
                                return entries[entry_idx].virtual_size;
                            }
                        }();

                        // Perform the read based on whether or not we'll use the pooled buffer.
                        if (will_use_pooled_buffer) {
                            // Read the compressed data into the pooled buffer.
                            auto* const buffer = pooled_buffer.GetBuffer();
                            m_data_storage->Read(reinterpret_cast<u8*>(buffer), cur_read_size,
                                                 required_access_physical_offset);

                            // Decompress the data.
                            size_t buffer_offset;
                            for (buffer_offset = 0;
                                 entry_idx < entry_count &&
                                 ((static_cast<size_t>(entries[entry_idx].physical_size) +
                                   static_cast<size_t>(entries[entry_idx].gap_from_prev)) == 0 ||
                                  buffer_offset < cur_read_size);
                                 buffer_offset += entries[entry_idx++].physical_size) {
                                // Advance by the relevant gap.
                                buffer_offset += entries[entry_idx].gap_from_prev;

                                const auto compression_type = entries[entry_idx].compression_type;
                                switch (compression_type) {
                                case CompressionType::None: {
                                    // Check that we can remain within bounds.
                                    ASSERT(buffer_offset + entries[entry_idx].virtual_size <=
                                           cur_read_size);

                                    // Perform no decompression.
                                    R_TRY(read_func(
                                        entries[entry_idx].virtual_size,
                                        [&](void* dst, size_t dst_size) -> Result {
                                            // Check that the size is valid.
                                            ASSERT(dst_size == entries[entry_idx].virtual_size);

                                            // We have no compression, so just copy the data
                                            // out.
                                            std::memcpy(dst, buffer + buffer_offset,
                                                        entries[entry_idx].virtual_size);
                                            R_SUCCEED();
                                        }));

                                    break;
                                }
                                case CompressionType::Zeros: {
                                    // Check that we can remain within bounds.
                                    ASSERT(buffer_offset <= cur_read_size);

                                    // Zero the memory.
                                    R_TRY(read_func(
                                        entries[entry_idx].virtual_size,
                                        [&](void* dst, size_t dst_size) -> Result {
                                            // Check that the size is valid.
                                            ASSERT(dst_size == entries[entry_idx].virtual_size);

                                            // The data is zeroes, so zero the buffer.
                                            std::memset(dst, 0, entries[entry_idx].virtual_size);
                                            R_SUCCEED();
                                        }));

                                    break;
                                }
                                default: {
                                    // Check that we can remain within bounds.
                                    ASSERT(buffer_offset + entries[entry_idx].physical_size <=
                                           cur_read_size);

                                    // Get the decompressor.
                                    const auto decompressor =
                                        this->GetDecompressor(compression_type);
                                    R_UNLESS(decompressor != nullptr,
                                             ResultUnexpectedInCompressedStorageB);

                                    // Decompress the data.
                                    R_TRY(read_func(entries[entry_idx].virtual_size,
                                                    [&](void* dst, size_t dst_size) -> Result {
                                                        // Check that the size is valid.
                                                        ASSERT(dst_size ==
                                                               entries[entry_idx].virtual_size);

                                                        // Perform the decompression.
                                                        R_RETURN(decompressor(
                                                            dst, entries[entry_idx].virtual_size,
                                                            buffer + buffer_offset,
                                                            entries[entry_idx].physical_size));
                                                    }));

                                    break;
                                }
                                }
                            }

                            // Check that we processed the correct amount of data.
                            ASSERT(buffer_offset == cur_read_size);
                        } else {
                            // Account for the gap from the previous entry.
                            required_access_physical_offset += entries[entry_idx].gap_from_prev;
                            required_access_physical_size -= entries[entry_idx].gap_from_prev;

                            // We don't need the buffer (as the data is uncompressed), so just
                            // execute the read.
                            R_TRY(
                                read_func(cur_read_size, [&](void* dst, size_t dst_size) -> Result {
                                    // Check that the size is valid.
                                    ASSERT(dst_size == cur_read_size);

                                    // Perform the read.
                                    m_data_storage->Read(reinterpret_cast<u8*>(dst), cur_read_size,
                                                         required_access_physical_offset);

                                    R_SUCCEED();
                                }));
                        }

                        // Advance on.
                        required_access_physical_offset += cur_read_size;
                        required_access_physical_size -= cur_read_size;
                    }

                    // Verify that we have nothing remaining to read.
                    ASSERT(required_access_physical_size == 0);

                    R_SUCCEED();
                } else {
                    // We don't need a buffer, so just execute the read.
                    R_TRY(read_func(total_required_size, [&](void* dst, size_t dst_size) -> Result {
                        // Check that the size is valid.
                        ASSERT(dst_size == total_required_size);

                        // Perform the read.
                        m_data_storage->Read(reinterpret_cast<u8*>(dst), total_required_size,
                                             required_access_physical_offset);

                        R_SUCCEED();
                    }));
                }

                R_SUCCEED();
            };

            R_TRY(this->OperatePerEntry(
                offset, size,
                [&](bool* out_continuous, const Entry& entry, s64 virtual_data_size,
                    s64 data_offset, s64 read_size) -> Result {
                    // Determine the physical extents.
                    s64 physical_offset, physical_size;
                    if (CompressionTypeUtility::IsRandomAccessible(entry.compression_type)) {
                        physical_offset = entry.phys_offset + data_offset;
                        physical_size = read_size;
                    } else {
                        physical_offset = entry.phys_offset;
                        physical_size = entry.GetPhysicalSize();
                    }

                    // If we have a pending data storage operation, perform it if we have to.
                    const s64 required_access_physical_end =
                        required_access_physical_offset + required_access_physical_size;
                    if (required_access_physical_size > 0) {
                        const bool required_by_gap =
                            !(required_access_physical_end <= physical_offset &&
                              physical_offset <= Common::AlignUp(required_access_physical_end,
                                                                 CompressionBlockAlignment));
                        const bool required_by_continuous_size =
                            ((physical_size + physical_offset) - required_access_physical_end) +
                                required_access_physical_size >
                            static_cast<s64>(m_continuous_reading_size_max);
                        const bool required_by_entry_count = entry_count == EntriesCountMax;
                        if (required_by_gap || required_by_continuous_size ||
                            required_by_entry_count) {
                            // Check that our planned access is sane.
                            ASSERT(!will_allocate_pooled_buffer ||
                                   required_access_physical_size <=
                                       static_cast<s64>(m_continuous_reading_size_max));

                            // Perform the required read.
                            const Result rc = PerformRequiredRead();
                            if (R_FAILED(rc)) {
                                R_THROW(rc);
                            }

                            // Reset our requirements.
                            prev_entry.virt_offset = -1;
                            required_access_physical_size = 0;
                            entry_count = 0;
                            will_allocate_pooled_buffer = false;
                        }
                    }

                    // Sanity check that we're within bounds on entries.
                    ASSERT(entry_count < EntriesCountMax);

                    // Determine if a buffer allocation is needed.
                    if (entry.compression_type != CompressionType::None ||
                        (prev_entry.virt_offset >= 0 &&
                         entry.virt_offset - prev_entry.virt_offset !=
                             entry.phys_offset - prev_entry.phys_offset)) {
                        will_allocate_pooled_buffer = true;
                    }

                    // If we need to access the data storage, update our required access parameters.
                    if (CompressionTypeUtility::IsDataStorageAccessRequired(
                            entry.compression_type)) {
                        // If the data is compressed, ensure the access is sane.
                        if (entry.compression_type != CompressionType::None) {
                            R_UNLESS(data_offset == 0, ResultInvalidOffset);
                            R_UNLESS(virtual_data_size == read_size, ResultInvalidSize);
                            R_UNLESS(entry.GetPhysicalSize() <= static_cast<s64>(m_block_size_max),
                                     ResultUnexpectedInCompressedStorageD);
                        }

                        // Update the required access parameters.
                        s64 gap_from_prev;
                        if (required_access_physical_size > 0) {
                            gap_from_prev = physical_offset - required_access_physical_end;
                        } else {
                            gap_from_prev = 0;
                            required_access_physical_offset = physical_offset;
                        }
                        required_access_physical_size += physical_size + gap_from_prev;

                        // Create an entry to access the data storage.
                        entries[entry_count++] = {
                            .compression_type = entry.compression_type,
                            .gap_from_prev = static_cast<u32>(gap_from_prev),
                            .physical_size = static_cast<u32>(physical_size),
                            .virtual_size = static_cast<u32>(read_size),
                        };
                    } else {
                        // Verify that we're allowed to be operating on the non-data-storage-access
                        // type.
                        R_UNLESS(entry.compression_type == CompressionType::Zeros,
                                 ResultUnexpectedInCompressedStorageB);

                        // If we have entries, create a fake entry for the zero region.
                        if (entry_count != 0) {
                            // We need to have a physical size.
                            R_UNLESS(entry.GetPhysicalSize() != 0,
                                     ResultUnexpectedInCompressedStorageD);

                            // Create a fake entry.
                            entries[entry_count++] = {
                                .compression_type = CompressionType::Zeros,
                                .gap_from_prev = 0,
                                .physical_size = 0,
                                .virtual_size = static_cast<u32>(read_size),
                            };
                        } else {
                            // We have no entries, so we can just perform the read.
                            const Result rc =
                                read_func(static_cast<size_t>(read_size),
                                          [&](void* dst, size_t dst_size) -> Result {
                                              // Check the space we should zero is correct.
                                              ASSERT(dst_size == static_cast<size_t>(read_size));

                                              // Zero the memory.
                                              std::memset(dst, 0, read_size);
                                              R_SUCCEED();
                                          });
                            if (R_FAILED(rc)) {
                                R_THROW(rc);
                            }
                        }
                    }

                    // Set the previous entry.
                    prev_entry = entry;

                    // We're continuous.
                    *out_continuous = true;
                    R_SUCCEED();
                }));

            // If we still have a pending access, perform it.
            if (required_access_physical_size != 0) {
                R_TRY(PerformRequiredRead());
            }

            R_SUCCEED();
        }

    private:
        DecompressorFunction GetDecompressor(CompressionType type) const {
            // Check that we can get a decompressor for the type.
            if (CompressionTypeUtility::IsUnknownType(type)) {
                return nullptr;
            }

            // Get the decompressor.
            return m_get_decompressor_function(type);
        }

        bool IsInitialized() const {
            return m_table.IsInitialized();
        }

    private:
        size_t m_block_size_max;
        size_t m_continuous_reading_size_max;
        BucketTree m_table;
        VirtualFile m_data_storage;
        GetDecompressorFunction m_get_decompressor_function;
    };

    class CacheManager {
        SUDACHI_NON_COPYABLE(CacheManager);
        SUDACHI_NON_MOVEABLE(CacheManager);

    private:
        struct AccessRange {
            s64 virtual_offset;
            s64 virtual_size;
            u32 physical_size;
            bool is_block_alignment_required;

            s64 GetEndVirtualOffset() const {
                return this->virtual_offset + this->virtual_size;
            }
        };
        static_assert(std::is_trivial_v<AccessRange>);

    public:
        CacheManager() = default;

    public:
        Result Initialize(s64 storage_size, size_t cache_size_0, size_t cache_size_1,
                          size_t max_cache_entries) {
            // Set our fields.
            m_storage_size = storage_size;

            R_SUCCEED();
        }

        Result Read(CompressedStorageCore& core, s64 offset, void* buffer, size_t size) {
            // If we have nothing to read, succeed.
            R_SUCCEED_IF(size == 0);

            // Check that we have a buffer to read into.
            R_UNLESS(buffer != nullptr, ResultNullptrArgument);

            // Check that the read is in bounds.
            R_UNLESS(offset <= m_storage_size, ResultInvalidOffset);

            // Determine how much we can read.
            const size_t read_size = std::min<size_t>(size, m_storage_size - offset);

            // Create head/tail ranges.
            AccessRange head_range = {};
            AccessRange tail_range = {};
            bool is_tail_set = false;

            // Operate to determine the head range.
            R_TRY(core.OperatePerEntry(
                offset, 1,
                [&](bool* out_continuous, const Entry& entry, s64 virtual_data_size,
                    s64 data_offset, s64 data_read_size) -> Result {
                    // Set the head range.
                    head_range = {
                        .virtual_offset = entry.virt_offset,
                        .virtual_size = virtual_data_size,
                        .physical_size = static_cast<u32>(entry.phys_size),
                        .is_block_alignment_required =
                            CompressionTypeUtility::IsBlockAlignmentRequired(
                                entry.compression_type),
                    };

                    // If required, set the tail range.
                    if (static_cast<s64>(offset + read_size) <=
                        entry.virt_offset + virtual_data_size) {
                        tail_range = {
                            .virtual_offset = entry.virt_offset,
                            .virtual_size = virtual_data_size,
                            .physical_size = static_cast<u32>(entry.phys_size),
                            .is_block_alignment_required =
                                CompressionTypeUtility::IsBlockAlignmentRequired(
                                    entry.compression_type),
                        };
                        is_tail_set = true;
                    }

                    // We only want to determine the head range, so we're not continuous.
                    *out_continuous = false;
                    R_SUCCEED();
                }));

            // If necessary, determine the tail range.
            if (!is_tail_set) {
                R_TRY(core.OperatePerEntry(
                    offset + read_size - 1, 1,
                    [&](bool* out_continuous, const Entry& entry, s64 virtual_data_size,
                        s64 data_offset, s64 data_read_size) -> Result {
                        // Set the tail range.
                        tail_range = {
                            .virtual_offset = entry.virt_offset,
                            .virtual_size = virtual_data_size,
                            .physical_size = static_cast<u32>(entry.phys_size),
                            .is_block_alignment_required =
                                CompressionTypeUtility::IsBlockAlignmentRequired(
                                    entry.compression_type),
                        };

                        // We only want to determine the tail range, so we're not continuous.
                        *out_continuous = false;
                        R_SUCCEED();
                    }));
            }

            // Begin performing the accesses.
            s64 cur_offset = offset;
            size_t cur_size = read_size;
            char* cur_dst = static_cast<char*>(buffer);

            // Determine our alignment.
            const bool head_unaligned = head_range.is_block_alignment_required &&
                                        (cur_offset != head_range.virtual_offset ||
                                         static_cast<s64>(cur_size) < head_range.virtual_size);
            const bool tail_unaligned = [&]() -> bool {
                if (tail_range.is_block_alignment_required) {
                    if (static_cast<s64>(cur_size + cur_offset) ==
                        tail_range.GetEndVirtualOffset()) {
                        return false;
                    } else if (!head_unaligned) {
                        return true;
                    } else {
                        return head_range.GetEndVirtualOffset() <
                               static_cast<s64>(cur_size + cur_offset);
                    }
                } else {
                    return false;
                }
            }();

            // Determine start/end offsets.
            const s64 start_offset =
                head_range.is_block_alignment_required ? head_range.virtual_offset : cur_offset;
            const s64 end_offset = tail_range.is_block_alignment_required
                                       ? tail_range.GetEndVirtualOffset()
                                       : cur_offset + cur_size;

            // Perform the read.
            bool is_burst_reading = false;
            R_TRY(core.Read(
                start_offset, end_offset - start_offset,
                [&](size_t size_buffer_required,
                    const CompressedStorageCore::ReadImplFunction& read_impl) -> Result {
                    // Determine whether we're burst reading.
                    const AccessRange* unaligned_range = nullptr;
                    if (!is_burst_reading) {
                        // Check whether we're using head, tail, or none as unaligned.
                        if (head_unaligned && head_range.virtual_offset <= cur_offset &&
                            cur_offset < head_range.GetEndVirtualOffset()) {
                            unaligned_range = std::addressof(head_range);
                        } else if (tail_unaligned && tail_range.virtual_offset <= cur_offset &&
                                   cur_offset < tail_range.GetEndVirtualOffset()) {
                            unaligned_range = std::addressof(tail_range);
                        } else {
                            is_burst_reading = true;
                        }
                    }
                    ASSERT((is_burst_reading ^ (unaligned_range != nullptr)));

                    // Perform reading by burst, or not.
                    if (is_burst_reading) {
                        // Check that the access is valid for burst reading.
                        ASSERT(size_buffer_required <= cur_size);

                        // Perform the read.
                        Result rc = read_impl(cur_dst, size_buffer_required);
                        if (R_FAILED(rc)) {
                            R_THROW(rc);
                        }

                        // Advance.
                        cur_dst += size_buffer_required;
                        cur_offset += size_buffer_required;
                        cur_size -= size_buffer_required;

                        // Determine whether we're going to continue burst reading.
                        const s64 offset_aligned =
                            tail_unaligned ? tail_range.virtual_offset : end_offset;
                        ASSERT(cur_offset <= offset_aligned);

                        if (offset_aligned <= cur_offset) {
                            is_burst_reading = false;
                        }
                    } else {
                        // We're not burst reading, so we have some unaligned range.
                        ASSERT(unaligned_range != nullptr);

                        // Check that the size is correct.
                        ASSERT(size_buffer_required ==
                               static_cast<size_t>(unaligned_range->virtual_size));

                        // Get a pooled buffer for our read.
                        PooledBuffer pooled_buffer;
                        pooled_buffer.Allocate(size_buffer_required, size_buffer_required);

                        // Perform read.
                        Result rc = read_impl(pooled_buffer.GetBuffer(), size_buffer_required);
                        if (R_FAILED(rc)) {
                            R_THROW(rc);
                        }

                        // Copy the data we read to the destination.
                        const size_t skip_size = cur_offset - unaligned_range->virtual_offset;
                        const size_t copy_size = std::min<size_t>(
                            cur_size, unaligned_range->GetEndVirtualOffset() - cur_offset);

                        std::memcpy(cur_dst, pooled_buffer.GetBuffer() + skip_size, copy_size);

                        // Advance.
                        cur_dst += copy_size;
                        cur_offset += copy_size;
                        cur_size -= copy_size;
                    }

                    R_SUCCEED();
                }));

            R_SUCCEED();
        }

    private:
        s64 m_storage_size = 0;
    };

public:
    CompressedStorage() = default;
    virtual ~CompressedStorage() {
        this->Finalize();
    }

    Result Initialize(VirtualFile data_storage, VirtualFile node_storage, VirtualFile entry_storage,
                      s32 bktr_entry_count, size_t block_size_max,
                      size_t continuous_reading_size_max, GetDecompressorFunction get_decompressor,
                      size_t cache_size_0, size_t cache_size_1, s32 max_cache_entries) {
        // Initialize our core.
        R_TRY(m_core.Initialize(data_storage, node_storage, entry_storage, bktr_entry_count,
                                block_size_max, continuous_reading_size_max, get_decompressor));

        // Get our core size.
        s64 core_size = 0;
        R_TRY(m_core.GetSize(std::addressof(core_size)));

        // Initialize our cache manager.
        R_TRY(m_cache_manager.Initialize(core_size, cache_size_0, cache_size_1, max_cache_entries));

        R_SUCCEED();
    }

    void Finalize() {
        m_core.Finalize();
    }

    VirtualFile GetDataStorage() {
        return m_core.GetDataStorage();
    }

    Result GetDataStorageSize(s64* out) {
        R_RETURN(m_core.GetDataStorageSize(out));
    }

    Result GetEntryList(Entry* out_entries, s32* out_read_count, s32 max_entry_count, s64 offset,
                        s64 size) {
        R_RETURN(m_core.GetEntryList(out_entries, out_read_count, max_entry_count, offset, size));
    }

    BucketTree& GetEntryTable() {
        return m_core.GetEntryTable();
    }

public:
    virtual size_t GetSize() const override {
        s64 ret{};
        m_core.GetSize(&ret);
        return ret;
    }

    virtual size_t Read(u8* buffer, size_t size, size_t offset) const override {
        if (R_SUCCEEDED(m_cache_manager.Read(m_core, offset, buffer, size))) {
            return size;
        } else {
            return 0;
        }
    }

private:
    mutable CompressedStorageCore m_core;
    mutable CacheManager m_cache_manager;
};

} // namespace FileSys
