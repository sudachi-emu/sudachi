// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <iterator>
#include <memory>
#include <optional>
#include <span>
#include <vector>

#include "common/scratch_buffer.h"
#include "core/guest_memory.h"
#include "video_core/memory_manager.h"

namespace Tegra::Memory {

using GuestMemoryFlags = Core::Memory::GuestMemoryFlags;

template <typename T, GuestMemoryFlags FLAGS>
using DeviceGuestMemory = Core::Memory::GuestMemory<Tegra::MaxwellDeviceMemoryManager, T, FLAGS>;
template <typename T, GuestMemoryFlags FLAGS>
using DeviceGuestMemoryScoped =
    Core::Memory::GuestMemoryScoped<Tegra::MaxwellDeviceMemoryManager, T, FLAGS>;
template <typename T, GuestMemoryFlags FLAGS>
using GpuGuestMemory = Core::Memory::GuestMemory<Tegra::MemoryManager, T, FLAGS>;
template <typename T, GuestMemoryFlags FLAGS>
using GpuGuestMemoryScoped = Core::Memory::GuestMemoryScoped<Tegra::MemoryManager, T, FLAGS>;

} // namespace Tegra::Memory
