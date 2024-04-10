// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <list>
#include <string>

#include "audio_core/sink/sink.h"

namespace Core {
class System;
}

namespace AudioCore::Sink {
class SinkStream;

class OboeSink final : public Sink {
public:
    explicit OboeSink();
    ~OboeSink() override;

    /**
     * Create a new sink stream.
     *
     * @param system          - Core system.
     * @param system_channels - Number of channels the audio system expects.
     *                          May differ from the device's channel count.
     * @param name            - Name of this stream.
     * @param type            - Type of this stream, render/in/out.
     *
     * @return A pointer to the created SinkStream
     */
    SinkStream* AcquireSinkStream(Core::System& system, u32 system_channels,
                                  const std::string& name, StreamType type) override;

    /**
     * Close a given stream.
     *
     * @param stream - The stream to close.
     */
    void CloseStream(SinkStream* stream) override;

    /**
     * Close all streams.
     */
    void CloseStreams() override;

    /**
     * Get the device volume. Set from calls to the IAudioDevice service.
     *
     * @return Volume of the device.
     */
    f32 GetDeviceVolume() const override;

    /**
     * Set the device volume. Set from calls to the IAudioDevice service.
     *
     * @param volume - New volume of the device.
     */
    void SetDeviceVolume(f32 volume) override;

    /**
     * Set the system volume. Comes from the audio system using this stream.
     *
     * @param volume - New volume of the system.
     */
    void SetSystemVolume(f32 volume) override;

private:
    /// List of streams managed by this sink
    std::list<SinkStreamPtr> sink_streams{};
};

} // namespace AudioCore::Sink
