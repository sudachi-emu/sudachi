// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <span>
#include <vector>

#include <oboe/Oboe.h>

#include "audio_core/common/common.h"
#include "audio_core/sink/oboe_sink.h"
#include "audio_core/sink/sink_stream.h"
#include "common/logging/log.h"
#include "common/scope_exit.h"
#include "core/core.h"

namespace AudioCore::Sink {

class OboeSinkStream final : public SinkStream,
                             public oboe::AudioStreamDataCallback,
                             public oboe::AudioStreamErrorCallback {
public:
    explicit OboeSinkStream(Core::System& system_, StreamType type_, const std::string& name_,
                            u32 system_channels_)
        : SinkStream(system_, type_) {
        name = name_;
        system_channels = system_channels_;

        this->OpenStream();
    }

    ~OboeSinkStream() override {
        LOG_INFO(Audio_Sink, "Destroyed Oboe stream");
    }

    void Finalize() override {
        this->Stop();
        m_stream.reset();
    }

    void Start(bool resume = false) override {
        if (!m_stream || !paused) {
            return;
        }

        paused = false;

        if (m_stream->start() != oboe::Result::OK) {
            LOG_CRITICAL(Audio_Sink, "Error starting Oboe stream");
        }
    }

    void Stop() override {
        if (!m_stream || paused) {
            return;
        }

        this->SignalPause();

        if (m_stream->stop() != oboe::Result::OK) {
            LOG_CRITICAL(Audio_Sink, "Error stopping Oboe stream");
        }
    }

public:
    static s32 QueryChannelCount(oboe::Direction direction) {
        std::shared_ptr<oboe::AudioStream> temp_stream;
        oboe::AudioStreamBuilder builder;

        const auto result = ConfigureBuilder(builder, direction)->openStream(temp_stream);
        if (result == oboe::Result::OK) {
            return temp_stream->getChannelCount() >= 6 ? 6 : 2;
        }

        LOG_ERROR(Audio_Sink, "Failed to open {} stream. Using default channel count 2",
                  direction == oboe::Direction::Output ? "output" : "input");
        return 2;
    }

protected:
    oboe::DataCallbackResult onAudioReady(oboe::AudioStream*, void* audio_data,
                                          s32 num_buffer_frames) override {
        const size_t num_channels = this->GetDeviceChannels();
        const size_t frame_size = num_channels;
        const size_t num_frames = static_cast<size_t>(num_buffer_frames);

        if (type == StreamType::In) {
            std::span<const s16> input_buffer{reinterpret_cast<const s16*>(audio_data),
                                              num_frames * frame_size};
            this->ProcessAudioIn(input_buffer, num_frames);
        } else {
            std::span<s16> output_buffer{reinterpret_cast<s16*>(audio_data),
                                         num_frames * frame_size};
            this->ProcessAudioOutAndRender(output_buffer, num_frames);
        }

        return oboe::DataCallbackResult::Continue;
    }

    void onErrorAfterClose(oboe::AudioStream*, oboe::Result) override {
        LOG_INFO(Audio_Sink, "Audio stream closed, reinitializing");

        if (this->OpenStream()) {
            m_stream->start();
        }
    }

private:
    static oboe::AudioStreamBuilder* ConfigureBuilder(oboe::AudioStreamBuilder& builder,
                                                      oboe::Direction direction) {
        // TODO: investigate callback delay issues when using AAudio
        return builder.setPerformanceMode(oboe::PerformanceMode::LowLatency)
            ->setAudioApi(oboe::AudioApi::OpenSLES)
            ->setDirection(direction)
            ->setSampleRate(TargetSampleRate)
            ->setSampleRateConversionQuality(oboe::SampleRateConversionQuality::High)
            ->setFormat(oboe::AudioFormat::I16)
            ->setFormatConversionAllowed(true)
            ->setUsage(oboe::Usage::Game)
            ->setBufferCapacityInFrames(TargetSampleCount * 2);
    }

    bool OpenStream() {
        const auto direction = [&]() {
            switch (type) {
            case StreamType::In:
                return oboe::Direction::Input;
            case StreamType::Out:
            case StreamType::Render:
                return oboe::Direction::Output;
            default:
                ASSERT(false);
                return oboe::Direction::Output;
            }
        }();

        const auto expected_channels = QueryChannelCount(direction);
        const auto expected_mask = [&]() {
            switch (expected_channels) {
            case 1:
                return oboe::ChannelMask::Mono;
            case 2:
                return oboe::ChannelMask::Stereo;
            case 6:
                return oboe::ChannelMask::CM5Point1;
            default:
                ASSERT(false);
                return oboe::ChannelMask::Unspecified;
            }
        }();

        oboe::AudioStreamBuilder builder;
        const auto result = ConfigureBuilder(builder, direction)
                                ->setChannelCount(expected_channels)
                                ->setChannelMask(expected_mask)
                                ->setChannelConversionAllowed(true)
                                ->setDataCallback(this)
                                ->setErrorCallback(this)
                                ->openStream(m_stream);
        ASSERT(result == oboe::Result::OK);
        return result == oboe::Result::OK && this->SetStreamProperties();
    }

    bool SetStreamProperties() {
        ASSERT(m_stream);

        m_stream->setBufferSizeInFrames(TargetSampleCount * 2);
        device_channels = m_stream->getChannelCount();

        const auto sample_rate = m_stream->getSampleRate();
        const auto buffer_capacity = m_stream->getBufferCapacityInFrames();
        const auto stream_backend =
            m_stream->getAudioApi() == oboe::AudioApi::AAudio ? "AAudio" : "OpenSLES";

        LOG_INFO(Audio_Sink, "Opened Oboe {} stream with {} channels sample rate {} capacity {}",
                 stream_backend, device_channels, sample_rate, buffer_capacity);

        return true;
    }

    std::shared_ptr<oboe::AudioStream> m_stream{};
};

OboeSink::OboeSink() {
    // TODO: This is not generally knowable
    // The channel count is distinct based on direction and can change
    device_channels = OboeSinkStream::QueryChannelCount(oboe::Direction::Output);
}

OboeSink::~OboeSink() = default;

SinkStream* OboeSink::AcquireSinkStream(Core::System& system, u32 system_channels,
                                        const std::string& name, StreamType type) {
    SinkStreamPtr& stream = sink_streams.emplace_back(
        std::make_unique<OboeSinkStream>(system, type, name, system_channels));

    return stream.get();
}

void OboeSink::CloseStream(SinkStream* to_remove) {
    sink_streams.remove_if([&](auto& stream) { return stream.get() == to_remove; });
}

void OboeSink::CloseStreams() {
    sink_streams.clear();
}

f32 OboeSink::GetDeviceVolume() const {
    if (sink_streams.empty()) {
        return 1.0f;
    }

    return sink_streams.front()->GetDeviceVolume();
}

void OboeSink::SetDeviceVolume(f32 volume) {
    for (auto& stream : sink_streams) {
        stream->SetDeviceVolume(volume);
    }
}

void OboeSink::SetSystemVolume(f32 volume) {
    for (auto& stream : sink_streams) {
        stream->SetSystemVolume(volume);
    }
}

} // namespace AudioCore::Sink
