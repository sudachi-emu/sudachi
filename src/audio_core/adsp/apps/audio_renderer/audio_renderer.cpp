// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <array>
#include <chrono>

#include "audio_core/adsp/apps/audio_renderer/audio_renderer.h"
#include "audio_core/audio_core.h"
#include "audio_core/common/common.h"
#include "audio_core/sink/sink.h"
#include "common/logging/log.h"
#include "common/microprofile.h"
#include "common/thread.h"
#include "core/core.h"
#include "core/core_timing.h"

MICROPROFILE_DEFINE(Audio_Renderer, "Audio", "DSP_AudioRenderer", MP_RGB(60, 19, 97));

namespace AudioCore::ADSP::AudioRenderer {

AudioRenderer::AudioRenderer(Core::System& system_, Sink::Sink& sink_)
    : system{system_}, sink{sink_} {}

AudioRenderer::~AudioRenderer() {
    Stop();
}

void AudioRenderer::Start() {
    CreateSinkStreams();

    mailbox.Initialize(AppMailboxId::AudioRenderer);

    main_thread = std::jthread([this](std::stop_token stop_token) { Main(stop_token); });

    mailbox.Send(Direction::DSP, Message::InitializeOK);
    if (mailbox.Receive(Direction::Host) != Message::InitializeOK) {
        LOG_ERROR(Service_Audio, "Host Audio Renderer -- Failed to receive shutdown "
                                 "message response from ADSP!");
        return;
    }
    running = true;
}

void AudioRenderer::Stop() {
    if (!running) {
        return;
    }

    mailbox.Send(Direction::DSP, Message::Shutdown);
    if (mailbox.Receive(Direction::Host) != Message::Shutdown) {
        LOG_ERROR(Service_Audio, "Host Audio Renderer -- Failed to receive shutdown "
                                 "message response from ADSP!");
    }
    main_thread.request_stop();
    main_thread.join();

    for (auto& stream : streams) {
        if (stream) {
            stream->Stop();
            sink.CloseStream(stream);
            stream = nullptr;
        }
    }
    running = false;
}

void AudioRenderer::Signal() {
    signalled_tick = system.CoreTiming().GetGlobalTimeNs().count();
    Send(Direction::DSP, Message::Render);
}

void AudioRenderer::Wait() {
    auto msg = Receive(Direction::Host);
    if (msg != Message::RenderResponse) {
        LOG_ERROR(Service_Audio,
                  "Did not receive the expected render response from the AudioRenderer! Expected "
                  "{}, got {}",
                  Message::RenderResponse, msg);
    }
    PostDSPClearCommandBuffer();
}

void AudioRenderer::Send(Direction dir, u32 message) {
    mailbox.Send(dir, std::move(message));
}

u32 AudioRenderer::Receive(Direction dir) {
    return mailbox.Receive(dir);
}

void AudioRenderer::SetCommandBuffer(s32 session_id, CpuAddr buffer, u64 size, u64 time_limit,
                                     u64 applet_resource_user_id, Kernel::KProcess* process,
                                     bool reset) noexcept {
    command_buffers[session_id].buffer = buffer;
    command_buffers[session_id].size = size;
    command_buffers[session_id].time_limit = time_limit;
    command_buffers[session_id].applet_resource_user_id = applet_resource_user_id;
    command_buffers[session_id].process = process;
    command_buffers[session_id].reset_buffer = reset;
}

void AudioRenderer::PostDSPClearCommandBuffer() noexcept {
    for (auto& buffer : command_buffers) {
        buffer.buffer = 0;
        buffer.size = 0;
        buffer.reset_buffer = false;
    }
}

u32 AudioRenderer::GetRemainCommandCount(s32 session_id) const noexcept {
    return command_buffers[session_id].remaining_command_count;
}

void AudioRenderer::ClearRemainCommandCount(s32 session_id) noexcept {
    command_buffers[session_id].remaining_command_count = 0;
}

u64 AudioRenderer::GetRenderingStartTick(s32 session_id) const noexcept {
    return (1000 * command_buffers[session_id].render_time_taken_us) + signalled_tick;
}

void AudioRenderer::CreateSinkStreams() {
    u32 channels{sink.GetDeviceChannels()};
    for (u32 i = 0; i < MaxRendererSessions; i++) {
        std::string name{fmt::format("ADSP_RenderStream-{}", i)};
        streams[i] =
            sink.AcquireSinkStream(system, channels, name, ::AudioCore::Sink::StreamType::Render);
        streams[i]->SetRingSize(4);
    }
}

void AudioRenderer::Main(std::stop_token stop_token) {
    static constexpr char name[]{"DSP_AudioRenderer_Main"};
    MicroProfileOnThreadCreate(name);
    Common::SetCurrentThreadName(name);
    Common::SetCurrentThreadPriority(Common::ThreadPriority::High);

    // TODO: Create buffer map/unmap thread + mailbox
    // TODO: Create gMix devices, initialize them here

    if (mailbox.Receive(Direction::DSP) != Message::InitializeOK) {
        LOG_ERROR(Service_Audio,
                  "ADSP Audio Renderer -- Failed to receive initialize message from host!");
        return;
    }

    mailbox.Send(Direction::Host, Message::InitializeOK);

    // 0.12 seconds (2,304,000 / 19,200,000)
    constexpr u64 max_process_time{2'304'000ULL};

    while (!stop_token.stop_requested()) {
        auto msg{mailbox.Receive(Direction::DSP)};
        switch (msg) {
        case Message::Shutdown:
            mailbox.Send(Direction::Host, Message::Shutdown);
            return;

        case Message::Render: {
            if (system.IsShuttingDown()) [[unlikely]] {
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
                mailbox.Send(Direction::Host, Message::RenderResponse);
                continue;
            }
            std::array<bool, MaxRendererSessions> buffers_reset{};
            std::array<u64, MaxRendererSessions> render_times_taken{};
            const auto start_time{system.CoreTiming().GetGlobalTimeUs().count()};

            for (u32 index = 0; index < MaxRendererSessions; index++) {
                auto& command_buffer{command_buffers[index]};
                auto& command_list_processor{command_list_processors[index]};

                // Check this buffer is valid, as it may not be used.
                if (command_buffer.buffer != 0) {
                    // If there are no remaining commands (from the previous list),
                    // this is a new command list, initialize it.
                    if (command_buffer.remaining_command_count == 0) {
                        command_list_processor.Initialize(system, *command_buffer.process,
                                                          command_buffer.buffer,
                                                          command_buffer.size, streams[index]);
                    }

                    if (command_buffer.reset_buffer && !buffers_reset[index]) {
                        streams[index]->ClearQueue();
                        buffers_reset[index] = true;
                    }

                    u64 max_time{max_process_time};
                    if (index == 1 && command_buffer.applet_resource_user_id ==
                                          command_buffers[0].applet_resource_user_id) {
                        max_time = max_process_time - render_times_taken[0];
                        if (render_times_taken[0] > max_process_time) {
                            max_time = 0;
                        }
                    }

                    max_time = std::min(command_buffer.time_limit, max_time);
                    command_list_processor.SetProcessTimeMax(max_time);

                    if (index == 0) {
                        streams[index]->WaitFreeSpace(stop_token);
                    }

                    // Process the command list
                    {
                        MICROPROFILE_SCOPE(Audio_Renderer);
                        render_times_taken[index] =
                            command_list_processor.Process(index) - start_time;
                    }

                    const auto end_time{system.CoreTiming().GetGlobalTimeUs().count()};

                    command_buffer.remaining_command_count =
                        command_list_processor.GetRemainingCommandCount();
                    command_buffer.render_time_taken_us = end_time - start_time;
                }
            }

            mailbox.Send(Direction::Host, Message::RenderResponse);
        } break;

        default:
            LOG_WARNING(Service_Audio,
                        "ADSP AudioRenderer received an invalid message, msg={:02X}!", msg);
            break;
        }
    }
}

} // namespace AudioCore::ADSP::AudioRenderer
