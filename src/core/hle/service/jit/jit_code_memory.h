// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <random>

#include "core/hle/kernel/k_code_memory.h"

namespace Service::JIT {

class CodeMemory {
public:
    SUDACHI_NON_COPYABLE(CodeMemory);

    explicit CodeMemory() = default;

    CodeMemory(CodeMemory&& rhs) {
        std::swap(m_code_memory, rhs.m_code_memory);
        std::swap(m_size, rhs.m_size);
        std::swap(m_address, rhs.m_address);
        std::swap(m_perm, rhs.m_perm);
    }

    ~CodeMemory() {
        this->Finalize();
    }

public:
    Result Initialize(Kernel::KProcess& process, Kernel::KCodeMemory& code_memory, size_t size,
                      Kernel::Svc::MemoryPermission perm, std::mt19937_64& generate_random);
    void Finalize();

    size_t GetSize() const {
        return m_size;
    }

    u64 GetAddress() const {
        return m_address;
    }

private:
    Kernel::KCodeMemory* m_code_memory{};
    size_t m_size{};
    u64 m_address{};
    Kernel::Svc::MemoryPermission m_perm{};
};

} // namespace Service::JIT
