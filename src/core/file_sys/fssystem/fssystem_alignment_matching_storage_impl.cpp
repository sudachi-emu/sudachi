// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/alignment.h"
#include "core/file_sys/fssystem/fssystem_alignment_matching_storage_impl.h"

namespace FileSys {

namespace {

template <typename T>
constexpr size_t GetRoundDownDifference(T x, size_t align) {
    return static_cast<size_t>(x - Common::AlignDown(x, align));
}

template <typename T>
constexpr size_t GetRoundUpDifference(T x, size_t align) {
    return static_cast<size_t>(Common::AlignUp(x, align) - x);
}

template <typename T>
size_t GetRoundUpDifference(T* x, size_t align) {
    return GetRoundUpDifference(reinterpret_cast<uintptr_t>(x), align);
}

} // namespace

size_t AlignmentMatchingStorageImpl::Read(VirtualFile base_storage, char* work_buf,
                                          size_t work_buf_size, size_t data_alignment,
                                          size_t buffer_alignment, s64 offset, u8* buffer,
                                          size_t size) {
    // Check preconditions.
    ASSERT(work_buf_size >= data_alignment);

    // Succeed if zero size.
    if (size == 0) {
        return size;
    }

    // Validate arguments.
    ASSERT(buffer != nullptr);

    // Determine extents.
    u8* aligned_core_buffer;
    s64 core_offset;
    size_t core_size;
    size_t buffer_gap;
    size_t offset_gap;
    s64 covered_offset;

    const size_t offset_round_up_difference = GetRoundUpDifference(offset, data_alignment);
    if (Common::IsAligned(reinterpret_cast<uintptr_t>(buffer) + offset_round_up_difference,
                          buffer_alignment)) {
        aligned_core_buffer = buffer + offset_round_up_difference;

        core_offset = Common::AlignUp(offset, data_alignment);
        core_size = (size < offset_round_up_difference)
                        ? 0
                        : Common::AlignDown(size - offset_round_up_difference, data_alignment);
        buffer_gap = 0;
        offset_gap = 0;

        covered_offset = core_size > 0 ? core_offset : offset;
    } else {
        const size_t buffer_round_up_difference = GetRoundUpDifference(buffer, buffer_alignment);

        aligned_core_buffer = buffer + buffer_round_up_difference;

        core_offset = Common::AlignDown(offset, data_alignment);
        core_size = (size < buffer_round_up_difference)
                        ? 0
                        : Common::AlignDown(size - buffer_round_up_difference, data_alignment);
        buffer_gap = buffer_round_up_difference;
        offset_gap = GetRoundDownDifference(offset, data_alignment);

        covered_offset = offset;
    }

    // Read the core portion.
    if (core_size > 0) {
        base_storage->Read(aligned_core_buffer, core_size, core_offset);

        if (offset_gap != 0 || buffer_gap != 0) {
            std::memmove(aligned_core_buffer - buffer_gap, aligned_core_buffer + offset_gap,
                         core_size - offset_gap);
            core_size -= offset_gap;
        }
    }

    // Handle the head portion.
    if (offset < covered_offset) {
        const s64 head_offset = Common::AlignDown(offset, data_alignment);
        const size_t head_size = static_cast<size_t>(covered_offset - offset);

        ASSERT(GetRoundDownDifference(offset, data_alignment) + head_size <= work_buf_size);

        base_storage->Read(reinterpret_cast<u8*>(work_buf), data_alignment, head_offset);
        std::memcpy(buffer, work_buf + GetRoundDownDifference(offset, data_alignment), head_size);
    }

    // Handle the tail portion.
    s64 tail_offset = covered_offset + core_size;
    size_t remaining_tail_size = static_cast<size_t>((offset + size) - tail_offset);
    while (remaining_tail_size > 0) {
        const auto aligned_tail_offset = Common::AlignDown(tail_offset, data_alignment);
        const auto cur_size =
            std::min(static_cast<size_t>(aligned_tail_offset + data_alignment - tail_offset),
                     remaining_tail_size);
        base_storage->Read(reinterpret_cast<u8*>(work_buf), data_alignment, aligned_tail_offset);

        ASSERT((tail_offset - offset) + cur_size <= size);
        ASSERT((tail_offset - aligned_tail_offset) + cur_size <= data_alignment);
        std::memcpy(reinterpret_cast<char*>(buffer) + (tail_offset - offset),
                    work_buf + (tail_offset - aligned_tail_offset), cur_size);

        remaining_tail_size -= cur_size;
        tail_offset += cur_size;
    }

    return size;
}

size_t AlignmentMatchingStorageImpl::Write(VirtualFile base_storage, char* work_buf,
                                           size_t work_buf_size, size_t data_alignment,
                                           size_t buffer_alignment, s64 offset, const u8* buffer,
                                           size_t size) {
    // Check preconditions.
    ASSERT(work_buf_size >= data_alignment);

    // Succeed if zero size.
    if (size == 0) {
        return size;
    }

    // Validate arguments.
    ASSERT(buffer != nullptr);

    // Determine extents.
    const u8* aligned_core_buffer;
    s64 core_offset;
    size_t core_size;
    s64 covered_offset;

    const size_t offset_round_up_difference = GetRoundUpDifference(offset, data_alignment);
    if (Common::IsAligned(reinterpret_cast<uintptr_t>(buffer) + offset_round_up_difference,
                          buffer_alignment)) {
        aligned_core_buffer = buffer + offset_round_up_difference;

        core_offset = Common::AlignUp(offset, data_alignment);
        core_size = (size < offset_round_up_difference)
                        ? 0
                        : Common::AlignDown(size - offset_round_up_difference, data_alignment);

        covered_offset = core_size > 0 ? core_offset : offset;
    } else {
        aligned_core_buffer = nullptr;

        core_offset = Common::AlignDown(offset, data_alignment);
        core_size = 0;

        covered_offset = offset;
    }

    // Write the core portion.
    if (core_size > 0) {
        base_storage->Write(aligned_core_buffer, core_size, core_offset);
    }

    // Handle the head portion.
    if (offset < covered_offset) {
        const s64 head_offset = Common::AlignDown(offset, data_alignment);
        const size_t head_size = static_cast<size_t>(covered_offset - offset);

        ASSERT((offset - head_offset) + head_size <= data_alignment);

        base_storage->Read(reinterpret_cast<u8*>(work_buf), data_alignment, head_offset);
        std::memcpy(work_buf + (offset - head_offset), buffer, head_size);
        base_storage->Write(reinterpret_cast<u8*>(work_buf), data_alignment, head_offset);
    }

    // Handle the tail portion.
    s64 tail_offset = covered_offset + core_size;
    size_t remaining_tail_size = static_cast<size_t>((offset + size) - tail_offset);
    while (remaining_tail_size > 0) {
        ASSERT(static_cast<size_t>(tail_offset - offset) < size);

        const auto aligned_tail_offset = Common::AlignDown(tail_offset, data_alignment);
        const auto cur_size =
            std::min(static_cast<size_t>(aligned_tail_offset + data_alignment - tail_offset),
                     remaining_tail_size);

        base_storage->Read(reinterpret_cast<u8*>(work_buf), data_alignment, aligned_tail_offset);
        std::memcpy(work_buf + GetRoundDownDifference(tail_offset, data_alignment),
                    buffer + (tail_offset - offset), cur_size);
        base_storage->Write(reinterpret_cast<u8*>(work_buf), data_alignment, aligned_tail_offset);

        remaining_tail_size -= cur_size;
        tail_offset += cur_size;
    }

    return size;
}

} // namespace FileSys
