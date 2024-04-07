// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "core/hle/result.h"

namespace Core {
class System;
}

namespace Kernel {
class KSharedMemory;
}

namespace Service::HID {
struct SharedMemoryFormat;

// This is nn::hid::detail::SharedMemoryHolder
class SharedMemoryHolder {
public:
    SharedMemoryHolder();
    ~SharedMemoryHolder();

    Result Initialize(Core::System& system);
    void Finalize();

    bool IsMapped();
    SharedMemoryFormat* GetAddress();
    Kernel::KSharedMemory* GetHandle();

private:
    bool is_owner{};
    bool is_created{};
    bool is_mapped{};
    INSERT_PADDING_BYTES(0x5);
    Kernel::KSharedMemory* shared_memory;
    INSERT_PADDING_BYTES(0x38);
    SharedMemoryFormat* address = nullptr;
};
// Correct size is 0x50 bytes
static_assert(sizeof(SharedMemoryHolder) == 0x50, "SharedMemoryHolder is an invalid size");

} // namespace Service::HID
