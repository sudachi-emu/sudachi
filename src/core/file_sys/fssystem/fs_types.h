// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "common/common_funcs.h"

namespace FileSys {

struct Int64 {
    u32 low;
    u32 high;

    constexpr void Set(s64 v) {
        this->low = static_cast<u32>((v & static_cast<u64>(0x00000000FFFFFFFFULL)) >> 0);
        this->high = static_cast<u32>((v & static_cast<u64>(0xFFFFFFFF00000000ULL)) >> 32);
    }

    constexpr s64 Get() const {
        return (static_cast<s64>(this->high) << 32) | (static_cast<s64>(this->low));
    }

    constexpr Int64& operator=(s64 v) {
        this->Set(v);
        return *this;
    }

    constexpr operator s64() const {
        return this->Get();
    }
};

struct HashSalt {
    static constexpr size_t Size = 32;

    std::array<u8, Size> value;
};
static_assert(std::is_trivial_v<HashSalt>);
static_assert(sizeof(HashSalt) == HashSalt::Size);

constexpr inline size_t IntegrityMinLayerCount = 2;
constexpr inline size_t IntegrityMaxLayerCount = 7;
constexpr inline size_t IntegrityLayerCountSave = 5;
constexpr inline size_t IntegrityLayerCountSaveDataMeta = 4;

} // namespace FileSys
