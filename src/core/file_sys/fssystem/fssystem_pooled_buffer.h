// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "common/common_funcs.h"
#include "common/common_types.h"
#include "common/literals.h"
#include "core/hle/result.h"

namespace FileSys {

using namespace Common::Literals;

constexpr inline size_t BufferPoolAlignment = 4_KiB;
constexpr inline size_t BufferPoolWorkSize = 320;

class PooledBuffer {
    SUDACHI_NON_COPYABLE(PooledBuffer);

public:
    // Constructor/Destructor.
    constexpr PooledBuffer() : m_buffer(), m_size() {}

    PooledBuffer(size_t ideal_size, size_t required_size) : m_buffer(), m_size() {
        this->Allocate(ideal_size, required_size);
    }

    ~PooledBuffer() {
        this->Deallocate();
    }

    // Move and assignment.
    explicit PooledBuffer(PooledBuffer&& rhs) : m_buffer(rhs.m_buffer), m_size(rhs.m_size) {
        rhs.m_buffer = nullptr;
        rhs.m_size = 0;
    }

    PooledBuffer& operator=(PooledBuffer&& rhs) {
        PooledBuffer(std::move(rhs)).Swap(*this);
        return *this;
    }

    // Allocation API.
    void Allocate(size_t ideal_size, size_t required_size) {
        return this->AllocateCore(ideal_size, required_size, false);
    }

    void AllocateParticularlyLarge(size_t ideal_size, size_t required_size) {
        return this->AllocateCore(ideal_size, required_size, true);
    }

    void Shrink(size_t ideal_size);

    void Deallocate() {
        // Shrink the buffer to empty.
        this->Shrink(0);
        ASSERT(m_buffer == nullptr);
    }

    char* GetBuffer() const {
        ASSERT(m_buffer != nullptr);
        return m_buffer;
    }

    size_t GetSize() const {
        ASSERT(m_buffer != nullptr);
        return m_size;
    }

public:
    static size_t GetAllocatableSizeMax() {
        return GetAllocatableSizeMaxCore(false);
    }
    static size_t GetAllocatableParticularlyLargeSizeMax() {
        return GetAllocatableSizeMaxCore(true);
    }

private:
    static size_t GetAllocatableSizeMaxCore(bool large);

private:
    void Swap(PooledBuffer& rhs) {
        std::swap(m_buffer, rhs.m_buffer);
        std::swap(m_size, rhs.m_size);
    }

    void AllocateCore(size_t ideal_size, size_t required_size, bool large);

private:
    char* m_buffer;
    size_t m_size;
};

} // namespace FileSys
