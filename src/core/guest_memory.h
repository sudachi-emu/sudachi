// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <iterator>
#include <memory>
#include <optional>
#include <span>
#include <vector>

#include "common/assert.h"
#include "common/scratch_buffer.h"

namespace Core::Memory {

enum GuestMemoryFlags : u32 {
    Read = 1 << 0,
    Write = 1 << 1,
    Safe = 1 << 2,
    Cached = 1 << 3,

    SafeRead = Read | Safe,
    SafeWrite = Write | Safe,
    SafeReadWrite = SafeRead | SafeWrite,
    SafeReadCachedWrite = SafeReadWrite | Cached,

    UnsafeRead = Read,
    UnsafeWrite = Write,
    UnsafeReadWrite = UnsafeRead | UnsafeWrite,
    UnsafeReadCachedWrite = UnsafeReadWrite | Cached,
};

namespace {
template <typename M, typename T, GuestMemoryFlags FLAGS>
class GuestMemory {
    using iterator = T*;
    using const_iterator = const T*;
    using value_type = T;
    using element_type = T;
    using iterator_category = std::contiguous_iterator_tag;

public:
    GuestMemory() = delete;
    explicit GuestMemory(M& memory, u64 addr, std::size_t size,
                         Common::ScratchBuffer<T>* backup = nullptr)
        : m_memory{&memory}, m_addr{addr}, m_size{size} {
        static_assert(FLAGS & GuestMemoryFlags::Read || FLAGS & GuestMemoryFlags::Write);
        if constexpr (!(FLAGS & GuestMemoryFlags::Read)) {
            if (!this->TrySetSpan()) {
                if (backup) {
                    backup->resize_destructive(this->size());
                    m_data_span = *backup;
                    m_span_valid = true;
                    m_is_data_copy = true;
                } else {
                    m_data_copy.resize(this->size());
                    m_data_span = std::span(m_data_copy);
                    m_span_valid = true;
                    m_is_data_copy = true;
                }
            }
        } else if constexpr (FLAGS & GuestMemoryFlags::Read) {
            Read(addr, size, backup);
        }
    }

    ~GuestMemory() = default;

    GuestMemory(GuestMemory&& rhs) = default;
    GuestMemory& operator=(GuestMemory&& rhs) = default;

    T* data() noexcept {
        return m_data_span.data();
    }

    const T* data() const noexcept {
        return m_data_span.data();
    }

    size_t size() const noexcept {
        return m_size;
    }

    size_t size_bytes() const noexcept {
        return this->size() * sizeof(T);
    }

    [[nodiscard]] T* begin() noexcept {
        return this->data();
    }

    [[nodiscard]] const T* begin() const noexcept {
        return this->data();
    }

    [[nodiscard]] T* end() noexcept {
        return this->data() + this->size();
    }

    [[nodiscard]] const T* end() const noexcept {
        return this->data() + this->size();
    }

    T& operator[](size_t index) noexcept {
        return m_data_span[index];
    }

    const T& operator[](size_t index) const noexcept {
        return m_data_span[index];
    }

    void SetAddressAndSize(u64 addr, std::size_t size) noexcept {
        m_addr = addr;
        m_size = size;
        m_addr_changed = true;
    }

    std::span<T> Read(u64 addr, std::size_t size,
                      Common::ScratchBuffer<T>* backup = nullptr) noexcept {
        m_addr = addr;
        m_size = size;
        if (m_size == 0) {
            m_is_data_copy = true;
            return {};
        }

        if (this->TrySetSpan()) {
            if constexpr (FLAGS & GuestMemoryFlags::Safe && M::HAS_FLUSH_INVALIDATION) {
                m_memory->FlushRegion(m_addr, this->size_bytes());
            }
        } else {
            if (backup) {
                backup->resize_destructive(this->size());
                m_data_span = *backup;
            } else {
                m_data_copy.resize(this->size());
                m_data_span = std::span(m_data_copy);
            }
            m_is_data_copy = true;
            m_span_valid = true;
            if constexpr (FLAGS & GuestMemoryFlags::Safe) {
                m_memory->ReadBlock(m_addr, this->data(), this->size_bytes());
            } else {
                m_memory->ReadBlockUnsafe(m_addr, this->data(), this->size_bytes());
            }
        }
        return m_data_span;
    }

    void Write(std::span<T> write_data) noexcept {
        if constexpr (FLAGS & GuestMemoryFlags::Cached) {
            m_memory->WriteBlockCached(m_addr, write_data.data(), this->size_bytes());
        } else if constexpr (FLAGS & GuestMemoryFlags::Safe) {
            m_memory->WriteBlock(m_addr, write_data.data(), this->size_bytes());
        } else {
            m_memory->WriteBlockUnsafe(m_addr, write_data.data(), this->size_bytes());
        }
    }

    bool TrySetSpan() noexcept {
        if (u8* ptr = m_memory->GetSpan(m_addr, this->size_bytes()); ptr) {
            m_data_span = {reinterpret_cast<T*>(ptr), this->size()};
            m_span_valid = true;
            m_is_data_copy = false;
            return true;
        }
        return false;
    }

protected:
    bool IsDataCopy() const noexcept {
        return m_is_data_copy;
    }

    bool AddressChanged() const noexcept {
        return m_addr_changed;
    }

    M* m_memory;
    u64 m_addr{};
    size_t m_size{};
    std::span<T> m_data_span{};
    std::vector<T> m_data_copy{};
    bool m_span_valid{false};
    bool m_is_data_copy{false};
    bool m_addr_changed{false};
};

template <typename M, typename T, GuestMemoryFlags FLAGS>
class GuestMemoryScoped : public GuestMemory<M, T, FLAGS> {
public:
    GuestMemoryScoped() = delete;
    explicit GuestMemoryScoped(M& memory, u64 addr, std::size_t size,
                               Common::ScratchBuffer<T>* backup = nullptr)
        : GuestMemory<M, T, FLAGS>(memory, addr, size, backup) {}

    ~GuestMemoryScoped() {
        if constexpr (FLAGS & GuestMemoryFlags::Write) {
            if (this->size() == 0) [[unlikely]] {
                return;
            }

            if (this->AddressChanged() || this->IsDataCopy()) {
                ASSERT(this->m_span_valid);
                if constexpr (FLAGS & GuestMemoryFlags::Cached) {
                    this->m_memory->WriteBlockCached(this->m_addr, this->data(),
                                                     this->size_bytes());
                } else if constexpr (FLAGS & GuestMemoryFlags::Safe) {
                    this->m_memory->WriteBlock(this->m_addr, this->data(), this->size_bytes());
                } else {
                    this->m_memory->WriteBlockUnsafe(this->m_addr, this->data(),
                                                     this->size_bytes());
                }
            } else if constexpr ((FLAGS & GuestMemoryFlags::Safe) ||
                                 (FLAGS & GuestMemoryFlags::Cached)) {
                this->m_memory->InvalidateRegion(this->m_addr, this->size_bytes());
            }
        }
    }
};
} // namespace

} // namespace Core::Memory
