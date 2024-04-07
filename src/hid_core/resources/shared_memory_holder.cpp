// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/core.h"
#include "core/hle/kernel/k_shared_memory.h"
#include "hid_core/hid_result.h"
#include "hid_core/resources/applet_resource.h"
#include "hid_core/resources/shared_memory_format.h"
#include "hid_core/resources/shared_memory_holder.h"

namespace Service::HID {
SharedMemoryHolder::SharedMemoryHolder() {}

SharedMemoryHolder::~SharedMemoryHolder() {
    Finalize();
}

Result SharedMemoryHolder::Initialize(Core::System& system) {
    shared_memory = Kernel::KSharedMemory::Create(system.Kernel());
    const Result result = shared_memory->Initialize(
        system.DeviceMemory(), nullptr, Kernel::Svc::MemoryPermission::None,
        Kernel::Svc::MemoryPermission::Read, sizeof(SharedMemoryFormat));
    if (result.IsError()) {
        return result;
    }
    Kernel::KSharedMemory::Register(system.Kernel(), shared_memory);

    is_created = true;
    is_mapped = true;
    address = std::construct_at(reinterpret_cast<SharedMemoryFormat*>(shared_memory->GetPointer()));
    return ResultSuccess;
}

void SharedMemoryHolder::Finalize() {
    if (address != nullptr) {
        shared_memory->Close();
    }
    is_created = false;
    is_mapped = false;
    address = nullptr;
}

bool SharedMemoryHolder::IsMapped() {
    return is_mapped;
}

SharedMemoryFormat* SharedMemoryHolder::GetAddress() {
    return address;
}

Kernel::KSharedMemory* SharedMemoryHolder::GetHandle() {
    return shared_memory;
}
} // namespace Service::HID
