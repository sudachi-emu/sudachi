// SPDX-FileCopyrightText: Copyright 2021 sudachi Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <span>

#include "common/bit_field.h"
#include "common/common_types.h"

namespace Service::SPL {

constexpr size_t AES_128_KEY_SIZE = 0x10;

namespace Smc {

enum class FunctionId : u32 {
    SetConfig = 0xC3000401,
    GetConfig = 0xC3000002,
    GetResult = 0xC3000003,
    GetResultData = 0xC3000404,
    ModularExponentiate = 0xC3000E05,
    GenerateRandomBytes = 0xC3000006,
    GenerateAesKek = 0xC3000007,
    LoadAesKey = 0xC3000008,
    ComputeAes = 0xC3000009,
    GenerateSpecificAesKey = 0xC300000A,
    ComputeCmac = 0xC300040B,
    ReencryptDeviceUniqueData = 0xC300D60C,
    DecryptDeviceUniqueData = 0xC300100D,

    ModularExponentiateWithStorageKey = 0xC300060F,
    PrepareEsDeviceUniqueKey = 0xC3000610,
    LoadPreparedAesKey = 0xC3000011,
    PrepareCommonEsTitleKey = 0xC3000012,

    // Deprecated functions.
    LoadEsDeviceKey = 0xC300100C,
    DecryptAndStoreGcKey = 0xC300100E,

    // Atmosphere functions.
    AtmosphereIramCopy = 0xF0000201,
    AtmosphereReadWriteRegister = 0xF0000002,

    AtmosphereGetEmummcConfig = 0xF0000404,
};

enum class CipherMode {
    CbcEncrypt = 0,
    CbcDecrypt = 1,
    Ctr = 2,
};

enum class DeviceUniqueDataMode {
    DecryptDeviceUniqueData = 0,
    DecryptAndStoreGcKey = 1,
    DecryptAndStoreEsDeviceKey = 2,
    DecryptAndStoreSslKey = 3,
    DecryptAndStoreDrmDeviceCertKey = 4,
};

enum class ModularExponentiateWithStorageKeyMode {
    Gc = 0,
    Ssl = 1,
    DrmDeviceCert = 2,
};

enum class EsCommonKeyType {
    TitleKey = 0,
    ArchiveKey = 1,
    UnknownKey = 2, // TODO: (jarrodnorwell)
};

struct AsyncOperationKey {
    u64 value;
};

} // namespace Smc

enum class HardwareType {
    Icosa = 0,
    Copper = 1,
    Hoag = 2,
    Iowa = 3,
    Calcio = 4,
    Aula = 5,
};

enum class SocType {
    Erista = 0,
    Mariko = 1,
};

enum class HardwareState {
    Development = 0,
    Production = 1,
};

enum class MemoryArrangement {
    Standard = 0,
    StandardForAppletDev = 1,
    StandardForSystemDev = 2,
    Expanded = 3,
    ExpandedForAppletDev = 4,

    // Note: Dynamic is not official.
    // Atmosphere uses it to maintain compatibility with firmwares prior to 6.0.0,
    // which removed the explicit retrieval of memory arrangement from PM.
    Dynamic = 5,
    Count,
};

enum class BootReason {
    Unknown = 0,
    AcOk = 1,
    OnKey = 2,
    RtcAlarm1 = 3,
    RtcAlarm2 = 4,
};

struct BootReasonValue {
    union {
        u32 value{};

        BitField<0, 8, u32> power_intr;
        BitField<8, 8, u32> rtc_intr;
        BitField<16, 8, u32> nv_erc;
        BitField<24, 8, u32> boot_reason;
    };
};
static_assert(sizeof(BootReasonValue) == sizeof(u32), "BootReasonValue definition!");

struct AesKey {
    std::array<u64, AES_128_KEY_SIZE / sizeof(u64)> data64{};

    std::span<u8> AsBytes() {
        return std::span{reinterpret_cast<u8*>(data64.data()), AES_128_KEY_SIZE};
    }

    std::span<const u8> AsBytes() const {
        return std::span{reinterpret_cast<const u8*>(data64.data()), AES_128_KEY_SIZE};
    }
};
static_assert(sizeof(AesKey) == AES_128_KEY_SIZE, "AesKey definition!");

struct IvCtr {
    std::array<u64, AES_128_KEY_SIZE / sizeof(u64)> data64{};

    std::span<u8> AsBytes() {
        return std::span{reinterpret_cast<u8*>(data64.data()), AES_128_KEY_SIZE};
    }

    std::span<const u8> AsBytes() const {
        return std::span{reinterpret_cast<const u8*>(data64.data()), AES_128_KEY_SIZE};
    }
};
static_assert(sizeof(AesKey) == AES_128_KEY_SIZE, "IvCtr definition!");

struct Cmac {
    std::array<u64, AES_128_KEY_SIZE / sizeof(u64)> data64{};

    std::span<u8> AsBytes() {
        return std::span{reinterpret_cast<u8*>(data64.data()), AES_128_KEY_SIZE};
    }

    std::span<const u8> AsBytes() const {
        return std::span{reinterpret_cast<const u8*>(data64.data()), AES_128_KEY_SIZE};
    }
};
static_assert(sizeof(AesKey) == AES_128_KEY_SIZE, "Cmac definition!");

struct AccessKey {
    std::array<u64, AES_128_KEY_SIZE / sizeof(u64)> data64{};

    std::span<u8> AsBytes() {
        return std::span{reinterpret_cast<u8*>(data64.data()), AES_128_KEY_SIZE};
    }

    std::span<const u8> AsBytes() const {
        return std::span{reinterpret_cast<const u8*>(data64.data()), AES_128_KEY_SIZE};
    }
};
static_assert(sizeof(AesKey) == AES_128_KEY_SIZE, "AccessKey definition!");

struct KeySource {
    std::array<u64, AES_128_KEY_SIZE / sizeof(u64)> data64{};

    std::span<u8> AsBytes() {
        return std::span{reinterpret_cast<u8*>(data64.data()), AES_128_KEY_SIZE};
    }

    std::span<const u8> AsBytes() const {
        return std::span{reinterpret_cast<const u8*>(data64.data()), AES_128_KEY_SIZE};
    }
};
static_assert(sizeof(AesKey) == AES_128_KEY_SIZE, "KeySource definition!");

enum class ConfigItem : u32 {
    // Standard config items.
    DisableProgramVerification = 1,
    DramId = 2,
    SecurityEngineInterruptNumber = 3,
    FuseVersion = 4,
    HardwareType = 5,
    HardwareState = 6,
    IsRecoveryBoot = 7,
    DeviceId = 8,
    BootReason = 9,
    MemoryMode = 10,
    IsDevelopmentFunctionEnabled = 11,
    KernelConfiguration = 12,
    IsChargerHiZModeEnabled = 13,
    QuestState = 14,
    RegulatorType = 15,
    DeviceUniqueKeyGeneration = 16,
    Package2Hash = 17,

    // Extension config items for exosphere.
    ExosphereApiVersion = 65000,
    ExosphereNeedsReboot = 65001,
    ExosphereNeedsShutdown = 65002,
    ExosphereGitCommitHash = 65003,
    ExosphereHasRcmBugPatch = 65004,
    ExosphereBlankProdInfo = 65005,
    ExosphereAllowCalWrites = 65006,
    ExosphereEmummcType = 65007,
    ExospherePayloadAddress = 65008,
    ExosphereLogConfiguration = 65009,
    ExosphereForceEnableUsb30 = 65010,
};

} // namespace Service::SPL
