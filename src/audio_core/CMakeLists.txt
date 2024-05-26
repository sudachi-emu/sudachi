# SPDX-FileCopyrightText: 2018 sudachi Emulator Project
# SPDX-License-Identifier: GPL-2.0-or-later

add_library(audio_core STATIC
    adsp/adsp.cpp
    adsp/adsp.h
    adsp/mailbox.h
    adsp/apps/audio_renderer/audio_renderer.cpp
    adsp/apps/audio_renderer/audio_renderer.h
    adsp/apps/audio_renderer/command_buffer.h
    adsp/apps/audio_renderer/command_list_processor.cpp
    adsp/apps/audio_renderer/command_list_processor.h
    adsp/apps/opus/opus_decoder.cpp
    adsp/apps/opus/opus_decoder.h
    adsp/apps/opus/opus_decode_object.cpp
    adsp/apps/opus/opus_decode_object.h
    adsp/apps/opus/opus_multistream_decode_object.cpp
    adsp/apps/opus/opus_multistream_decode_object.h
    adsp/apps/opus/shared_memory.h
    audio_core.cpp
    audio_core.h
    audio_event.h
    audio_event.cpp
    audio_render_manager.cpp
    audio_render_manager.h
    audio_in_manager.cpp
    audio_in_manager.h
    audio_out_manager.cpp
    audio_out_manager.h
    audio_manager.cpp
    audio_manager.h
    common/audio_renderer_parameter.h
    common/common.h
    common/feature_support.h
    common/wave_buffer.h
    common/workbuffer_allocator.h
    device/audio_buffer.h
    device/audio_buffers.h
    device/device_session.cpp
    device/device_session.h
    in/audio_in.cpp
    in/audio_in.h
    in/audio_in_system.cpp
    in/audio_in_system.h
    opus/hardware_opus.cpp
    opus/hardware_opus.h
    opus/decoder_manager.cpp
    opus/decoder_manager.h
    opus/decoder.cpp
    opus/decoder.h
    opus/parameters.h
    out/audio_out.cpp
    out/audio_out.h
    out/audio_out_system.cpp
    out/audio_out_system.h
    precompiled_headers.h
    renderer/audio_device.cpp
    renderer/audio_device.h
    renderer/audio_renderer.h
    renderer/audio_renderer.cpp
    renderer/behavior/behavior_info.cpp
    renderer/behavior/behavior_info.h
    renderer/behavior/info_updater.cpp
    renderer/behavior/info_updater.h
    renderer/command/data_source/adpcm.cpp
    renderer/command/data_source/adpcm.h
    renderer/command/data_source/decode.cpp
    renderer/command/data_source/decode.h
    renderer/command/data_source/pcm_float.cpp
    renderer/command/data_source/pcm_float.h
    renderer/command/data_source/pcm_int16.cpp
    renderer/command/data_source/pcm_int16.h
    renderer/command/effect/aux_.cpp
    renderer/command/effect/aux_.h
    renderer/command/effect/biquad_filter.cpp
    renderer/command/effect/biquad_filter.h
    renderer/command/effect/capture.cpp
    renderer/command/effect/capture.h
    renderer/command/effect/compressor.cpp
    renderer/command/effect/compressor.h
    renderer/command/effect/delay.cpp
    renderer/command/effect/delay.h
    renderer/command/effect/i3dl2_reverb.cpp
    renderer/command/effect/i3dl2_reverb.h
    renderer/command/effect/light_limiter.cpp
    renderer/command/effect/light_limiter.h
    renderer/command/effect/multi_tap_biquad_filter.cpp
    renderer/command/effect/multi_tap_biquad_filter.h
    renderer/command/effect/reverb.cpp
    renderer/command/effect/reverb.h
    renderer/command/mix/clear_mix.cpp
    renderer/command/mix/clear_mix.h
    renderer/command/mix/copy_mix.cpp
    renderer/command/mix/copy_mix.h
    renderer/command/mix/depop_for_mix_buffers.cpp
    renderer/command/mix/depop_for_mix_buffers.h
    renderer/command/mix/depop_prepare.cpp
    renderer/command/mix/depop_prepare.h
    renderer/command/mix/mix.cpp
    renderer/command/mix/mix.h
    renderer/command/mix/mix_ramp.cpp
    renderer/command/mix/mix_ramp.h
    renderer/command/mix/mix_ramp_grouped.cpp
    renderer/command/mix/mix_ramp_grouped.h
    renderer/command/mix/volume.cpp
    renderer/command/mix/volume.h
    renderer/command/mix/volume_ramp.cpp
    renderer/command/mix/volume_ramp.h
    renderer/command/performance/performance.cpp
    renderer/command/performance/performance.h
    renderer/command/resample/downmix_6ch_to_2ch.cpp
    renderer/command/resample/downmix_6ch_to_2ch.h
    renderer/command/resample/resample.h
    renderer/command/resample/resample.cpp
    renderer/command/resample/upsample.cpp
    renderer/command/resample/upsample.h
    renderer/command/sink/device.cpp
    renderer/command/sink/device.h
    renderer/command/sink/circular_buffer.cpp
    renderer/command/sink/circular_buffer.h
    renderer/command/command_buffer.cpp
    renderer/command/command_buffer.h
    renderer/command/command_generator.cpp
    renderer/command/command_generator.h
    renderer/command/command_list_header.h
    renderer/command/command_processing_time_estimator.cpp
    renderer/command/command_processing_time_estimator.h
    renderer/command/commands.h
    renderer/command/icommand.h
    renderer/effect/aux_.cpp
    renderer/effect/aux_.h
    renderer/effect/biquad_filter.cpp
    renderer/effect/biquad_filter.h
    renderer/effect/buffer_mixer.cpp
    renderer/effect/buffer_mixer.h
    renderer/effect/capture.cpp
    renderer/effect/capture.h
    renderer/effect/compressor.cpp
    renderer/effect/compressor.h
    renderer/effect/delay.cpp
    renderer/effect/delay.h
    renderer/effect/effect_context.cpp
    renderer/effect/effect_context.h
    renderer/effect/effect_info_base.h
    renderer/effect/effect_reset.h
    renderer/effect/effect_result_state.h
    renderer/effect/i3dl2.cpp
    renderer/effect/i3dl2.h
    renderer/effect/light_limiter.cpp
    renderer/effect/light_limiter.h
    renderer/effect/reverb.h
    renderer/effect/reverb.cpp
    renderer/mix/mix_context.cpp
    renderer/mix/mix_context.h
    renderer/mix/mix_info.cpp
    renderer/mix/mix_info.h
    renderer/memory/address_info.h
    renderer/memory/memory_pool_info.cpp
    renderer/memory/memory_pool_info.h
    renderer/memory/pool_mapper.cpp
    renderer/memory/pool_mapper.h
    renderer/nodes/bit_array.h
    renderer/nodes/edge_matrix.cpp
    renderer/nodes/edge_matrix.h
    renderer/nodes/node_states.cpp
    renderer/nodes/node_states.h
    renderer/performance/detail_aspect.cpp
    renderer/performance/detail_aspect.h
    renderer/performance/entry_aspect.cpp
    renderer/performance/entry_aspect.h
    renderer/performance/performance_detail.h
    renderer/performance/performance_entry.h
    renderer/performance/performance_entry_addresses.h
    renderer/performance/performance_frame_header.h
    renderer/performance/performance_manager.cpp
    renderer/performance/performance_manager.h
    renderer/sink/circular_buffer_sink_info.cpp
    renderer/sink/circular_buffer_sink_info.h
    renderer/sink/device_sink_info.cpp
    renderer/sink/device_sink_info.h
    renderer/sink/sink_context.cpp
    renderer/sink/sink_context.h
    renderer/sink/sink_info_base.cpp
    renderer/sink/sink_info_base.h
    renderer/splitter/splitter_context.cpp
    renderer/splitter/splitter_context.h
    renderer/splitter/splitter_destinations_data.cpp
    renderer/splitter/splitter_destinations_data.h
    renderer/splitter/splitter_info.cpp
    renderer/splitter/splitter_info.h
    renderer/system.cpp
    renderer/system.h
    renderer/system_manager.cpp
    renderer/system_manager.h
    renderer/upsampler/upsampler_info.h
    renderer/upsampler/upsampler_manager.cpp
    renderer/upsampler/upsampler_manager.h
    renderer/upsampler/upsampler_state.h
    renderer/voice/voice_channel_resource.h
    renderer/voice/voice_context.cpp
    renderer/voice/voice_context.h
    renderer/voice/voice_info.cpp
    renderer/voice/voice_info.h
    renderer/voice/voice_state.h
    sink/null_sink.h
    sink/sink.h
    sink/sink_details.cpp
    sink/sink_details.h
    sink/sink_stream.cpp
    sink/sink_stream.h
)

if (MSVC)
    target_compile_options(audio_core PRIVATE
        /we4242 # 'identifier': conversion from 'type1' to 'type2', possible loss of data
        /we4244 # 'conversion': conversion from 'type1' to 'type2', possible loss of data
        /we4245 # 'conversion': conversion from 'type1' to 'type2', signed/unsigned mismatch
        /we4254 # 'operator': conversion from 'type1:field_bits' to 'type2:field_bits', possible loss of data
        /we4800 # Implicit conversion from 'type' to bool. Possible information loss
    )
else()
    target_compile_options(audio_core PRIVATE
        -Werror=conversion

        -Wno-sign-conversion
    )
endif()

target_link_libraries(audio_core PUBLIC common core Opus::opus)
if (ARCHITECTURE_x86_64 OR ARCHITECTURE_arm64)
    target_link_libraries(audio_core PRIVATE dynarmic::dynarmic)
endif()

if (ENABLE_CUBEB)
    target_sources(audio_core PRIVATE
        sink/cubeb_sink.cpp
        sink/cubeb_sink.h
    )

    target_link_libraries(audio_core PRIVATE cubeb::cubeb)
    target_compile_definitions(audio_core PRIVATE -DHAVE_CUBEB=1)
endif()

if (ENABLE_SDL3)
    target_sources(audio_core PRIVATE
        sink/sdl3_sink.cpp
        sink/sdl3_sink.h
    )

    target_link_libraries(audio_core PRIVATE SDL3::SDL3)
    target_compile_definitions(audio_core PRIVATE HAVE_SDL3)
endif()

if (ANDROID)
    target_sources(audio_core PRIVATE
        sink/oboe_sink.cpp
        sink/oboe_sink.h
    )

    # FIXME: this port seems broken, it cannot be imported with find_package(oboe REQUIRED)
    target_link_libraries(audio_core PRIVATE "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/lib/liboboe.a")
    target_compile_definitions(audio_core PRIVATE HAVE_OBOE)
endif()

if (SUDACHI_USE_PRECOMPILED_HEADERS)
    target_precompile_headers(audio_core PRIVATE precompiled_headers.h)
endif()

create_target_directory_groups(audio_core)
