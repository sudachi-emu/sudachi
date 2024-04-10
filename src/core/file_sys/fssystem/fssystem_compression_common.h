// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "core/hle/result.h"

namespace FileSys {

enum class CompressionType : u8 {
    None = 0,
    Zeros = 1,
    Two = 2,
    Lz4 = 3,
    Unknown = 4,
};

using DecompressorFunction = Result (*)(void*, size_t, const void*, size_t);
using GetDecompressorFunction = DecompressorFunction (*)(CompressionType);

constexpr s64 CompressionBlockAlignment = 0x10;

namespace CompressionTypeUtility {

constexpr bool IsBlockAlignmentRequired(CompressionType type) {
    return type != CompressionType::None && type != CompressionType::Zeros;
}

constexpr bool IsDataStorageAccessRequired(CompressionType type) {
    return type != CompressionType::Zeros;
}

constexpr bool IsRandomAccessible(CompressionType type) {
    return type == CompressionType::None;
}

constexpr bool IsUnknownType(CompressionType type) {
    return type >= CompressionType::Unknown;
}

} // namespace CompressionTypeUtility

} // namespace FileSys
