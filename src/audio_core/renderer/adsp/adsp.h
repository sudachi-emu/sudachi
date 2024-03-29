// SPDX-FileCopyrightText: Copyright 2022 sudachi Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>
#include <mutex>

#include "audio_core/renderer/adsp/audio_renderer.h"
#include "common/common_types.h"

namespace Core {
namespace Memory {
class Memory;
}
class System;
} // namespace Core

namespace AudioCore {
namespace Sink {
class Sink;
}

namespace AudioRenderer::ADSP {
struct CommandBuffer;

enum class State {
    Started,
    Stopped,
};

/**
 * Represents the ADSP embedded within the audio sysmodule.
 * This is a 32-bit Linux4Tegra kernel from nVidia, which is launched with the sysmodule on boot.
 *
 * The kernel will run apps you program for it, Nintendo have the following:
 *
 * Gmix - Responsible for mixing final audio and sending it out to hardware. This is last place all
 *        audio samples end up, and we skip it entirely, since we have very different backends and
 *        mixing is implicitly handled by the OS (but also due to lack of research/simplicity).
 *
 * AudioRenderer - Receives command lists generated by the audio render
 *                 system, processes them, and sends the samples to Gmix.
 *
 * OpusDecoder - Contains libopus, and controls processing Opus audio and sends it to Gmix.
 *               Not much research done here, TODO if needed.
 *
 * We only implement the AudioRenderer for now.
 *
 * Communication for the apps is done through mailboxes, and some shared memory.
 */
class ADSP {
public:
    explicit ADSP(Core::System& system, Sink::Sink& sink);
    ~ADSP();

    /**
     * Start the ADSP.
     *
     * @return True if started or already running, otherwise false.
     */
    bool Start();

    /**
     * Stop the ADSP.
     */
    void Stop();

    /**
     * Get the ADSP's state.
     *
     * @return Started or Stopped.
     */
    State GetState() const;

    /**
     * Get the AudioRenderer mailbox to communicate with it.
     *
     * @return The AudioRenderer mailbox.
     */
    AudioRenderer_Mailbox* GetRenderMailbox();

    /**
     * Get the tick the ADSP was signalled.
     *
     * @return The tick the ADSP was signalled.
     */
    u64 GetSignalledTick() const;

    /**
     * Get the total time it took for the ADSP to run the last command lists (both command lists).
     *
     * @return The tick the ADSP was signalled.
     */
    u64 GetTimeTaken() const;

    /**
     * Get the last time a given command list took to run.
     *
     * @param session_id - The session id to check (0 or 1).
     * @return The time it took.
     */
    u64 GetRenderTimeTaken(u32 session_id);

    /**
     * Clear the remaining command count for a given session.
     *
     * @param session_id - The session id to check (0 or 1).
     */
    void ClearRemainCount(u32 session_id);

    /**
     * Get the remaining number of commands left to process for a command list.
     *
     * @param session_id - The session id to check (0 or 1).
     * @return The number of commands remaining.
     */
    u32 GetRemainCommandCount(u32 session_id) const;

    /**
     * Get the last tick a command list started processing.
     *
     * @param session_id - The session id to check (0 or 1).
     * @return The last tick the given command list started.
     */
    u64 GetRenderingStartTick(u32 session_id);

    /**
     * Set a command buffer to be processed.
     *
     * @param session_id     - The session id to check (0 or 1).
     * @param command_buffer - The command buffer to process.
     */
    void SendCommandBuffer(u32 session_id, const CommandBuffer& command_buffer);

    /**
     * Clear the command buffers (does not clear the time taken or the remaining command count)
     */
    void ClearCommandBuffers();

    /**
     * Signal the AudioRenderer to begin processing.
     */
    void Signal();

    /**
     * Wait for the AudioRenderer to finish processing.
     */
    void Wait();

private:
    /// Core system
    Core::System& system;
    /// Core memory
    Core::Memory::Memory& memory;
    /// Number of systems active, used to prevent accidental shutdowns
    u8 systems_active{0};
    /// ADSP running state
    std::atomic<bool> running{false};
    /// Output sink used by the ADSP
    Sink::Sink& sink;
    /// AudioRenderer app
    std::unique_ptr<AudioRenderer> audio_renderer{};
    /// Communication for the AudioRenderer
    AudioRenderer_Mailbox render_mailbox{};
    /// Mailbox lock ffor the render mailbox
    std::mutex mailbox_lock;
};

} // namespace AudioRenderer::ADSP
} // namespace AudioCore
