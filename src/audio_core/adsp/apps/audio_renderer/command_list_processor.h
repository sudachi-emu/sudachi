// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <span>

#include "audio_core/common/common.h"
#include "audio_core/renderer/command/command_list_header.h"
#include "common/common_types.h"

namespace Core {
namespace Memory {
class Memory;
}
class System;
} // namespace Core

namespace Kernel {
class KProcess;
}

namespace AudioCore {
namespace Sink {
class SinkStream;
}

namespace Renderer {
struct CommandListHeader;
}

namespace ADSP::AudioRenderer {

/**
 * A processor for command lists given to the AudioRenderer.
 */
class CommandListProcessor {
public:
    /**
     * Initialize the processor.
     *
     * @param system - The core system.
     * @param buffer - The command buffer to process.
     * @param size   - The size of the buffer.
     * @param stream - The stream to be used for sending the samples.
     */
    void Initialize(Core::System& system, Kernel::KProcess& process, CpuAddr buffer, u64 size,
                    Sink::SinkStream* stream);

    /**
     * Set the maximum processing time for this command list.
     *
     * @param time - The maximum process time.
     */
    void SetProcessTimeMax(u64 time);

    /**
     * Get the remaining command count for this list.
     *
     * @return The remaining command count.
     */
    u32 GetRemainingCommandCount() const;

    /**
     * Get the stream for this command list.
     *
     * @return The stream associated with this command list.
     */
    Sink::SinkStream* GetOutputSinkStream() const;

    /**
     * Process the command list.
     *
     * @param session_id - Session ID for the commands being processed.
     *
     * @return The time taken to process.
     */
    u64 Process(u32 session_id);

    /// Core system
    Core::System* system{};
    /// Core memory
    Core::Memory::Memory* memory{};
    /// Stream for the processed samples
    Sink::SinkStream* stream{};
    /// Header info for this command list
    Renderer::CommandListHeader* header{};
    /// The command buffer
    u8* commands{};
    /// The command buffer size
    u64 commands_buffer_size{};
    /// The maximum processing time allotted
    u64 max_process_time{};
    /// The number of commands in the buffer
    u32 command_count{};
    /// The target sample count for output
    u32 sample_count{};
    /// The target sample rate for output
    u32 target_sample_rate{};
    /// The mixing buffers used by the commands
    std::span<s32> mix_buffers{};
    /// The number of mix buffers
    u32 buffer_count{};
    /// The number of processed commands so far
    u32 processed_command_count{};
    /// The processing start time of this list
    u64 start_time{};
    /// The current processing time for this list
    u64 current_processing_time{};
    /// The end processing time for this list
    u64 end_time{};
    /// Last command list string generated, used for dumping audio commands to console
    std::string last_dump{};
};

} // namespace ADSP::AudioRenderer
} // namespace AudioCore
