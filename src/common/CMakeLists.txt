# SPDX-FileCopyrightText: 2018 sudachi Emulator Project
# SPDX-License-Identifier: GPL-2.0-or-later

if (DEFINED ENV{AZURECIREPO})
  set(BUILD_REPOSITORY $ENV{AZURECIREPO})
endif()
if (DEFINED ENV{TITLEBARFORMATIDLE})
  set(TITLE_BAR_FORMAT_IDLE $ENV{TITLEBARFORMATIDLE})
endif ()
if (DEFINED ENV{TITLEBARFORMATRUNNING})
  set(TITLE_BAR_FORMAT_RUNNING $ENV{TITLEBARFORMATRUNNING})
endif ()
if (DEFINED ENV{DISPLAYVERSION})
  set(DISPLAY_VERSION $ENV{DISPLAYVERSION})
endif ()

include(GenerateSCMRev)

add_library(common STATIC
    address_space.cpp
    address_space.h
    algorithm.h
    alignment.h
    announce_multiplayer_room.h
    assert.cpp
    assert.h
    atomic_helpers.h
    atomic_ops.h
    bit_cast.h
    bit_field.h
    bit_set.h
    bit_util.h
    bounded_threadsafe_queue.h
    cityhash.cpp
    cityhash.h
    common_funcs.h
    common_precompiled_headers.h
    common_types.h
    concepts.h
    container_hash.h
    demangle.cpp
    demangle.h
    detached_tasks.cpp
    detached_tasks.h
    div_ceil.h
    dynamic_library.cpp
    dynamic_library.h
    elf.h
    error.cpp
    error.h
    expected.h
    fiber.cpp
    fiber.h
    fixed_point.h
    free_region_manager.h
    fs/file.cpp
    fs/file.h
    fs/fs.cpp
    fs/fs.h
    fs/fs_paths.h
    fs/fs_types.h
    fs/fs_util.cpp
    fs/fs_util.h
    fs/path_util.cpp
    fs/path_util.h
    hash.h
    heap_tracker.cpp
    heap_tracker.h
    hex_util.cpp
    hex_util.h
    host_memory.cpp
    host_memory.h
    input.h
    intrusive_red_black_tree.h
    literals.h
    logging/backend.cpp
    logging/backend.h
    logging/filter.cpp
    logging/filter.h
    logging/formatter.h
    logging/log.h
    logging/log_entry.h
    logging/text_formatter.cpp
    logging/text_formatter.h
    logging/types.h
    lz4_compression.cpp
    lz4_compression.h
    make_unique_for_overwrite.h
    math_util.h
    memory_detect.cpp
    memory_detect.h
    multi_level_page_table.cpp
    multi_level_page_table.h
    nvidia_flags.cpp
    nvidia_flags.h
    overflow.h
    page_table.cpp
    page_table.h
    param_package.cpp
    param_package.h
    parent_of_member.h
    point.h
    precompiled_headers.h
    quaternion.h
    range_map.h
    range_mutex.h
    range_sets.h
    range_sets.inc
    reader_writer_queue.h
    ring_buffer.h
    ${CMAKE_CURRENT_BINARY_DIR}/scm_rev.cpp
    scm_rev.h
    scope_exit.h
    scratch_buffer.h
    settings.cpp
    settings.h
    settings_common.cpp
    settings_common.h
    settings_enums.h
    settings_input.cpp
    settings_input.h
    settings_setting.h
    slot_vector.h
    socket_types.h
    spin_lock.cpp
    spin_lock.h
    stb.cpp
    stb.h
    steady_clock.cpp
    steady_clock.h
    stream.cpp
    stream.h
    string_util.cpp
    string_util.h
    swap.h
    telemetry.cpp
    telemetry.h
    thread.cpp
    thread.h
    thread_queue_list.h
    thread_worker.h
    threadsafe_queue.h
    time_zone.cpp
    time_zone.h
    tiny_mt.h
    tree.h
    typed_address.h
    uint128.h
    unique_function.h
    uuid.cpp
    uuid.h
    vector_math.h
    virtual_buffer.cpp
    virtual_buffer.h
    wall_clock.cpp
    wall_clock.h
    zstd_compression.cpp
    zstd_compression.h
    profiling.h
)

if (SUDACHI_ENABLE_PORTABLE)
    add_compile_definitions(SUDACHI_ENABLE_PORTABLE)
endif()

if (WIN32)
  target_sources(common PRIVATE
    windows/timer_resolution.cpp
    windows/timer_resolution.h
  )
  target_link_libraries(common PRIVATE ntdll)
endif()

if (NOT WIN32)
  target_sources(common PRIVATE
    signal_chain.cpp
    signal_chain.h
  )
endif()

if(ANDROID)
    target_sources(common
        PUBLIC
            fs/fs_android.cpp
            fs/fs_android.h
            android/android_common.cpp
            android/android_common.h
            android/id_cache.cpp
            android/id_cache.h
            android/applets/software_keyboard.cpp
            android/applets/software_keyboard.h
    )
endif()

if (UNIX AND NOT APPLE)
  target_sources(common PRIVATE
    linux/gamemode.cpp
    linux/gamemode.h
  )

  target_link_libraries(common PRIVATE gamemode::headers)
endif()

if(ARCHITECTURE_x86_64)
    target_sources(common
        PRIVATE
            x64/cpu_detect.cpp
            x64/cpu_detect.h
            x64/cpu_wait.cpp
            x64/cpu_wait.h
            x64/native_clock.cpp
            x64/native_clock.h
            x64/rdtsc.cpp
            x64/rdtsc.h
            x64/xbyak_abi.h
            x64/xbyak_util.h
    )
    target_link_libraries(common PRIVATE xbyak::xbyak)
endif()

if (HAS_NCE)
    target_sources(common
        PRIVATE
            arm64/native_clock.cpp
            arm64/native_clock.h
    )
endif()

if (MSVC)
  target_compile_definitions(common PRIVATE
    # The standard library doesn't provide any replacement for codecvt yet
    # so we can disable this deprecation warning for the time being.
    _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING
  )
  target_compile_options(common PRIVATE
    /we4242 # 'identifier': conversion from 'type1' to 'type2', possible loss of data
    /we4254 # 'operator': conversion from 'type1:field_bits' to 'type2:field_bits', possible loss of data
    /we4800 # Implicit conversion from 'type' to bool. Possible information loss
  )
else()
  set_source_files_properties(stb.cpp PROPERTIES COMPILE_OPTIONS "-Wno-implicit-fallthrough;-Wno-missing-declarations;-Wno-missing-field-initializers")
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  target_compile_options(common PRIVATE
    -fsized-deallocation
    -Werror=unreachable-code-aggressive
  )
  target_compile_definitions(common PRIVATE
    # Clang 14 and earlier have errors when explicitly instantiating Settings::Setting
    $<$<VERSION_LESS:$<CXX_COMPILER_VERSION>,15>:CANNOT_EXPLICITLY_INSTANTIATE>
  )
endif()

target_link_libraries(common PUBLIC Boost::context Boost::headers fmt::fmt stb::headers Threads::Threads)
target_link_libraries(common PRIVATE lz4::lz4 zstd::zstd LLVM::Demangle)

if (ENABLE_PROFILING)
    target_link_libraries(common PUBLIC tracy)
    target_compile_definitions(common PUBLIC -DENABLE_PROFILING)
endif()

if (ANDROID)
    # For ASharedMemory_create
    target_link_libraries(common PRIVATE android)
endif()

if (SUDACHI_USE_PRECOMPILED_HEADERS)
    target_precompile_headers(common PRIVATE precompiled_headers.h)
endif()

create_target_directory_groups(common)
