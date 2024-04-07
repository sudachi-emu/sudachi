// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/alignment.h"
#include "core/file_sys/fssystem/fssystem_pooled_buffer.h"

namespace FileSys {

namespace {

constexpr size_t HeapBlockSize = BufferPoolAlignment;
static_assert(HeapBlockSize == 4_KiB);

// A heap block is 4KiB. An order is a power of two.
// This gives blocks of the order 32KiB, 512KiB, 4MiB.
constexpr s32 HeapOrderMax = 7;
constexpr s32 HeapOrderMaxForLarge = HeapOrderMax + 3;

constexpr size_t HeapAllocatableSizeMax = HeapBlockSize * (static_cast<size_t>(1) << HeapOrderMax);
constexpr size_t HeapAllocatableSizeMaxForLarge =
    HeapBlockSize * (static_cast<size_t>(1) << HeapOrderMaxForLarge);

} // namespace

size_t PooledBuffer::GetAllocatableSizeMaxCore(bool large) {
    return large ? HeapAllocatableSizeMaxForLarge : HeapAllocatableSizeMax;
}

void PooledBuffer::AllocateCore(size_t ideal_size, size_t required_size, bool large) {
    // Ensure preconditions.
    ASSERT(m_buffer == nullptr);

    // Check that we can allocate this size.
    ASSERT(required_size <= GetAllocatableSizeMaxCore(large));

    const size_t target_size =
        std::min(std::max(ideal_size, required_size), GetAllocatableSizeMaxCore(large));

    // Dummy implementation for allocate.
    if (target_size > 0) {
        m_buffer =
            reinterpret_cast<char*>(::operator new(target_size, std::align_val_t{HeapBlockSize}));
        m_size = target_size;

        // Ensure postconditions.
        ASSERT(m_buffer != nullptr);
    }
}

void PooledBuffer::Shrink(size_t ideal_size) {
    ASSERT(ideal_size <= GetAllocatableSizeMaxCore(true));

    // Shrinking to zero means that we have no buffer.
    if (ideal_size == 0) {
        ::operator delete(m_buffer, std::align_val_t{HeapBlockSize});
        m_buffer = nullptr;
        m_size = ideal_size;
    }
}

} // namespace FileSys
