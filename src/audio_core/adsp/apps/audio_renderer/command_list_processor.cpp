// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <string>

#include "audio_core/adsp/apps/audio_renderer/command_list_processor.h"
#include "audio_core/renderer/command/command_list_header.h"
#include "audio_core/renderer/command/commands.h"
#include "common/settings.h"
#include "core/core.h"
#include "core/core_timing.h"
#include "core/hle/kernel/k_process.h"
#include "core/memory.h"

namespace AudioCore::ADSP::AudioRenderer {

void CommandListProcessor::Initialize(Core::System& system_, Kernel::KProcess& process,
                                      CpuAddr buffer, u64 size, Sink::SinkStream* stream_) {
    system = &system_;
    memory = &process.GetMemory();
    stream = stream_;
    header = reinterpret_cast<Renderer::CommandListHeader*>(buffer);
    commands = reinterpret_cast<u8*>(buffer + sizeof(Renderer::CommandListHeader));
    commands_buffer_size = size;
    command_count = header->command_count;
    sample_count = header->sample_count;
    target_sample_rate = header->sample_rate;
    mix_buffers = header->samples_buffer;
    buffer_count = header->buffer_count;
    processed_command_count = 0;
}

void CommandListProcessor::SetProcessTimeMax(const u64 time) {
    max_process_time = time;
}

u32 CommandListProcessor::GetRemainingCommandCount() const {
    return command_count - processed_command_count;
}

Sink::SinkStream* CommandListProcessor::GetOutputSinkStream() const {
    return stream;
}

u64 CommandListProcessor::Process(u32 session_id) {
    const auto start_time_{system->CoreTiming().GetGlobalTimeUs().count()};
    const auto command_base{CpuAddr(commands)};

    if (processed_command_count > 0) {
        current_processing_time += start_time_ - end_time;
    } else {
        start_time = start_time_;
        current_processing_time = 0;
    }

    std::string dump{fmt::format("\nSession {}\n", session_id)};

    for (u32 index = 0; index < command_count; index++) {
        auto& command{*reinterpret_cast<Renderer::ICommand*>(commands)};

        if (command.magic != 0xCAFEBABE) {
            LOG_ERROR(Service_Audio, "Command has invalid magic! Expected 0xCAFEBABE, got {:08X}",
                      command.magic);
            return system->CoreTiming().GetGlobalTimeUs().count() - start_time_;
        }

        auto current_offset{CpuAddr(commands) - command_base};

        if (current_offset + command.size > commands_buffer_size) {
            LOG_ERROR(Service_Audio,
                      "Command exceeded command buffer, buffer size {:08X}, command ends at {:08X}",
                      commands_buffer_size,
                      CpuAddr(commands) + command.size - sizeof(Renderer::CommandListHeader));
            return system->CoreTiming().GetGlobalTimeUs().count() - start_time_;
        }

        if (Settings::values.dump_audio_commands) {
            command.Dump(*this, dump);
        }

        if (!command.Verify(*this)) {
            break;
        }

        if (command.enabled) {
            command.Process(*this);
        } else {
            dump += fmt::format("\tDisabled!\n");
        }

        processed_command_count++;
        commands += command.size;
    }

    if (Settings::values.dump_audio_commands && dump != last_dump) {
        LOG_WARNING(Service_Audio, "{}", dump);
        last_dump = dump;
    }

    end_time = system->CoreTiming().GetGlobalTimeUs().count();
    return end_time - start_time_;
}

} // namespace AudioCore::ADSP::AudioRenderer
