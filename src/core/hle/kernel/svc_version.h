// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "common/bit_field.h"
#include "common/common_types.h"
#include "common/literals.h"

namespace Kernel::Svc {

constexpr inline u32 ConvertToSvcMajorVersion(u32 sdk) {
    return sdk + 4;
}
constexpr inline u32 ConvertToSdkMajorVersion(u32 svc) {
    return svc - 4;
}

constexpr inline u32 ConvertToSvcMinorVersion(u32 sdk) {
    return sdk;
}
constexpr inline u32 ConvertToSdkMinorVersion(u32 svc) {
    return svc;
}

union KernelVersion {
    u32 value;
    BitField<0, 4, u32> minor_version;
    BitField<4, 13, u32> major_version;
};

constexpr inline u32 EncodeKernelVersion(u32 major, u32 minor) {
    return decltype(KernelVersion::minor_version)::FormatValue(minor) |
           decltype(KernelVersion::major_version)::FormatValue(major);
}

constexpr inline u32 GetKernelMajorVersion(u32 encoded) {
    return decltype(KernelVersion::major_version)::ExtractValue(encoded);
}

constexpr inline u32 GetKernelMinorVersion(u32 encoded) {
    return decltype(KernelVersion::minor_version)::ExtractValue(encoded);
}

// Nintendo doesn't support programs targeting SVC versions < 3.0.
constexpr inline u32 RequiredKernelMajorVersion = 3;
constexpr inline u32 RequiredKernelMinorVersion = 0;
constexpr inline u32 RequiredKernelVersion =
    EncodeKernelVersion(RequiredKernelMajorVersion, RequiredKernelMinorVersion);

// This is the highest SVC version supported, to be updated on new kernel releases.
// NOTE: Official kernel versions have SVC major = SDK major + 4, SVC minor = SDK minor.
constexpr inline u32 SupportedKernelMajorVersion = ConvertToSvcMajorVersion(15);
constexpr inline u32 SupportedKernelMinorVersion = ConvertToSvcMinorVersion(3);
constexpr inline u32 SupportedKernelVersion =
    EncodeKernelVersion(SupportedKernelMajorVersion, SupportedKernelMinorVersion);

} // namespace Kernel::Svc
