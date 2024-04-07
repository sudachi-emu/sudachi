// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <array>

#include "common/common_funcs.h"
#include "common/common_types.h"

namespace Service::NFC {
enum class BackendType : u32 {
    None,
    Nfc,
    Nfp,
    Mifare,
};

// This is nn::nfc::DeviceState
enum class DeviceState : u32 {
    Initialized,
    SearchingForTag,
    TagFound,
    TagRemoved,
    TagMounted,
    Unavailable,
    Finalized,
};

// This is nn::nfc::State
enum class State : u32 {
    NonInitialized,
    Initialized,
};

// This is nn::nfc::TagType
enum class TagType : u32 {
    None = 0,
    Type1 = 1U << 0,  // ISO14443A RW. Topaz
    Type2 = 1U << 1,  // ISO14443A RW. Ultralight, NTAGX, ST25TN
    Type3 = 1U << 2,  // ISO14443A RW/RO. Sony FeliCa
    Type4A = 1U << 3, // ISO14443A RW/RO. DESFire
    Type4B = 1U << 4, // ISO14443B RW/RO. DESFire
    Type5 = 1U << 5,  // ISO15693 RW/RO. SLI, SLIX, ST25TV
    Mifare = 1U << 6, // Mifare classic. Skylanders
    All = 0xFFFFFFFF,
};

enum class PackedTagType : u8 {
    None = 0,
    Type1 = 1U << 0,  // ISO14443A RW. Topaz
    Type2 = 1U << 1,  // ISO14443A RW. Ultralight, NTAGX, ST25TN
    Type3 = 1U << 2,  // ISO14443A RW/RO. Sony FeliCa
    Type4A = 1U << 3, // ISO14443A RW/RO. DESFire
    Type4B = 1U << 4, // ISO14443B RW/RO. DESFire
    Type5 = 1U << 5,  // ISO15693 RW/RO. SLI, SLIX, ST25TV
    Mifare = 1U << 6, // Mifare classic. Skylanders
    All = 0xFF,
};

// This is nn::nfc::NfcProtocol
enum class NfcProtocol : u32 {
    None,
    TypeA = 1U << 0, // ISO14443A
    TypeB = 1U << 1, // ISO14443B
    TypeF = 1U << 2, // Sony FeliCa
    All = 0xFFFFFFFFU,
};

// this is nn::nfc::TestWaveType
enum class TestWaveType : u32 {
    Unknown,
};

using UniqueSerialNumber = std::array<u8, 10>;

// This is nn::nfc::DeviceHandle
using DeviceHandle = u64;

// This is nn::nfc::TagInfo
struct TagInfo {
    UniqueSerialNumber uuid;
    u8 uuid_length;
    INSERT_PADDING_BYTES(0x15);
    NfcProtocol protocol;
    TagType tag_type;
    INSERT_PADDING_BYTES(0x30);
};
static_assert(sizeof(TagInfo) == 0x58, "TagInfo is an invalid size");

} // namespace Service::NFC
