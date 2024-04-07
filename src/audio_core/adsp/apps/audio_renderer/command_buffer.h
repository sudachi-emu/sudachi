// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "audio_core/common/common.h"
#include "common/common_types.h"

namespace Kernel {
class KProcess;
}

namespace AudioCore::ADSP::AudioRenderer {

struct CommandBuffer {
    // Set by the host
    CpuAddr buffer{};
    u64 size{};
    u64 time_limit{};
    u64 applet_resource_user_id{};
    Kernel::KProcess* process{};
    bool reset_buffer{};
    // Set by the DSP
    u32 remaining_command_count{};
    u64 render_time_taken_us{};
};

} // namespace AudioCore::ADSP::AudioRenderer
