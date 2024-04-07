// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <memory>
#include <thread>

#include "audio_core/adsp/apps/audio_renderer/command_buffer.h"
#include "audio_core/adsp/apps/audio_renderer/command_list_processor.h"
#include "audio_core/adsp/mailbox.h"
#include "common/common_types.h"
#include "common/polyfill_thread.h"
#include "common/reader_writer_queue.h"
#include "common/thread.h"

namespace Core {
class System;
} // namespace Core

namespace Kernel {
class KProcess;
}

namespace AudioCore {
namespace Sink {
class Sink;
}

namespace ADSP::AudioRenderer {

enum Message : u32 {
    Invalid = 0,
    MapUnmap_Map = 1,
    MapUnmap_MapResponse = 2,
    MapUnmap_Unmap = 3,
    MapUnmap_UnmapResponse = 4,
    MapUnmap_InvalidateCache = 5,
    MapUnmap_InvalidateCacheResponse = 6,
    MapUnmap_Shutdown = 7,
    MapUnmap_ShutdownResponse = 8,
    InitializeOK = 22,
    RenderResponse = 32,
    Render = 42,
    Shutdown = 52,
};

/**
 * The AudioRenderer application running on the ADSP.
 */
class AudioRenderer {
public:
    explicit AudioRenderer(Core::System& system, Sink::Sink& sink);
    ~AudioRenderer();

    /**
     * Start the AudioRenderer.
     *
     * @param mailbox The mailbox to use for this session.
     */
    void Start();

    /**
     * Stop the AudioRenderer.
     */
    void Stop();

    void Signal();
    void Wait();

    void Send(Direction dir, u32 message);
    u32 Receive(Direction dir);

    void SetCommandBuffer(s32 session_id, CpuAddr buffer, u64 size, u64 time_limit,
                          u64 applet_resource_user_id, Kernel::KProcess* process,
                          bool reset) noexcept;
    u32 GetRemainCommandCount(s32 session_id) const noexcept;
    void ClearRemainCommandCount(s32 session_id) noexcept;
    u64 GetRenderingStartTick(s32 session_id) const noexcept;

private:
    /**
     * Main AudioRenderer thread, responsible for processing the command lists.
     */
    void Main(std::stop_token stop_token);

    /**
     * Creates the streams which will receive the processed samples.
     */
    void CreateSinkStreams();

    void PostDSPClearCommandBuffer() noexcept;

    /// Core system
    Core::System& system;
    /// The output sink the AudioRenderer will send samples to
    Sink::Sink& sink;
    /// The active mailbox
    Mailbox mailbox;
    /// Main thread
    std::jthread main_thread{};
    /// The current state
    std::atomic<bool> running{};
    /// Shared memory of input command buffers, set by host, read by DSP
    std::array<CommandBuffer, MaxRendererSessions> command_buffers{};
    /// The command lists to process
    std::array<CommandListProcessor, MaxRendererSessions> command_list_processors{};
    /// The streams which will receive the processed samples
    std::array<Sink::SinkStream*, MaxRendererSessions> streams{};
    /// CPU Tick when the DSP was signalled to process, uses time rather than tick
    u64 signalled_tick{0};
};

} // namespace ADSP::AudioRenderer
} // namespace AudioCore
