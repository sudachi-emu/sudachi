# SPDX-FileCopyrightText: 2016 Citra Emulator Project
# SPDX-License-Identifier: GPL-2.0-or-later

# Dynarmic has cmake_minimum_required(3.12) and we may want to override
# some of its variables, which is only possible in 3.13+
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)

# Disable tests in all externals supporting the standard option name
set(BUILD_TESTING OFF)

# Build only static externals
set(BUILD_SHARED_LIBS OFF)

# Skip install rules for all externals
set_directory_properties(PROPERTIES EXCLUDE_FROM_ALL ON)

# Xbyak (also used by Dynarmic, so needs to be added first)
if ((ARCHITECTURE_x86 OR ARCHITECTURE_x86_64) AND NOT TARGET xbyak::xbyak)
    add_subdirectory(xbyak)
endif()

# Oaknut (also used by Dynarmic, so needs to be added first)
if (ARCHITECTURE_arm64 AND NOT TARGET merry::oaknut)
    add_subdirectory(oaknut)
endif()

# Dynarmic
if ((ARCHITECTURE_x86_64 OR ARCHITECTURE_arm64) AND NOT TARGET dynarmic::dynarmic)
    set(DYNARMIC_IGNORE_ASSERTS ON)
    add_subdirectory(dynarmic)
    add_library(dynarmic::dynarmic ALIAS dynarmic)
endif()

# getopt
if (MSVC)
    add_subdirectory(getopt)
endif()

# Glad
add_subdirectory(glad)

# mbedtls
add_subdirectory(mbedtls)
target_include_directories(mbedtls PUBLIC ./mbedtls/include)
if (NOT MSVC)
    target_compile_options(mbedcrypto PRIVATE
        -Wno-unused-but-set-variable
        -Wno-string-concatenation)
endif()

# MicroProfile
add_library(microprofile INTERFACE)
target_include_directories(microprofile INTERFACE ./microprofile)

# GCC bugs
if (CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL "12" AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND MINGW)
    target_compile_options(microprofile INTERFACE "-Wno-array-bounds")
endif()

# libusb
if (ENABLE_LIBUSB AND NOT TARGET libusb::usb)
    add_subdirectory(libusb)
endif()

# SDL2
if (USE_SDL2_FROM_EXTERNALS)
    if (NOT WIN32)
        # Sudachi itself needs: Atomic Audio Events Joystick Haptic Sensor Threads Timers
        # Since 2.0.18 Atomic+Threads required for HIDAPI/libusb (see https://github.com/libsdl-org/SDL/issues/5095)
        # Sudachi-cmd also needs: Video (depends on Loadso/Dlopen)
        # CPUinfo also required for SDL Audio, at least until 2.28.0 (see https://github.com/libsdl-org/SDL/issues/7809)
        set(SDL_UNUSED_SUBSYSTEMS
            File Filesystem
            Locale Power Render)
        foreach(_SUB ${SDL_UNUSED_SUBSYSTEMS})
          string(TOUPPER ${_SUB} _OPT)
          set(SDL_${_OPT} OFF)
        endforeach()

        set(HIDAPI ON)
    endif()
    if (APPLE)
        set(SDL_FILE ON)
    endif()

    add_subdirectory(SDL)
endif()

# ENet
if (NOT TARGET enet::enet)
    add_subdirectory(enet)
    target_include_directories(enet INTERFACE ./enet/include)
    add_library(enet::enet ALIAS enet)
endif()

# Cubeb
if (ENABLE_CUBEB AND NOT TARGET cubeb::cubeb)
    set(BUILD_TESTS OFF)
    set(BUILD_TOOLS OFF)
    add_subdirectory(cubeb)
    add_library(cubeb::cubeb ALIAS cubeb)
    if (NOT MSVC)
        if (TARGET speex)
            target_compile_options(speex PRIVATE -Wno-sign-compare)
        endif()
        target_compile_options(cubeb PRIVATE -Wno-implicit-const-int-float-conversion)
    endif()
endif()

# DiscordRPC
if (USE_DISCORD_PRESENCE AND NOT TARGET DiscordRPC::discord-rpc)
    set(BUILD_EXAMPLES OFF)
    add_subdirectory(discord-rpc)
    target_include_directories(discord-rpc INTERFACE ./discord-rpc/include)
    add_library(DiscordRPC::discord-rpc ALIAS discord-rpc)
endif()

# Sirit
add_subdirectory(sirit)

# httplib
if (ENABLE_WEB_SERVICE AND NOT TARGET httplib::httplib)
    set(HTTPLIB_REQUIRE_OPENSSL ON)
    add_subdirectory(cpp-httplib)
endif()

# cpp-jwt
if (ENABLE_WEB_SERVICE AND NOT TARGET cpp-jwt::cpp-jwt)
    set(CPP_JWT_BUILD_EXAMPLES OFF)
    set(CPP_JWT_BUILD_TESTS OFF)
    set(CPP_JWT_USE_VENDORED_NLOHMANN_JSON OFF)
    add_subdirectory(cpp-jwt)
endif()

# Opus
if (NOT TARGET Opus::opus)
    set(OPUS_BUILD_TESTING OFF)
    set(OPUS_BUILD_PROGRAMS OFF)
    set(OPUS_INSTALL_PKG_CONFIG_MODULE OFF)
    set(OPUS_INSTALL_CMAKE_CONFIG_MODULE OFF)
    add_subdirectory(opus)
endif()

# FFMpeg
if (SUDACHI_USE_BUNDLED_FFMPEG)
    add_subdirectory(ffmpeg)
    set(FFmpeg_PATH "${FFmpeg_PATH}" PARENT_SCOPE)
    set(FFmpeg_LDFLAGS "${FFmpeg_LDFLAGS}" PARENT_SCOPE)
    set(FFmpeg_LIBRARIES "${FFmpeg_LIBRARIES}" PARENT_SCOPE)
    set(FFmpeg_INCLUDE_DIR "${FFmpeg_INCLUDE_DIR}" PARENT_SCOPE)
endif()

# Vulkan-Headers
if (SUDACHI_USE_EXTERNAL_VULKAN_HEADERS)
    add_subdirectory(Vulkan-Headers)
endif()

# Vulkan-Utility-Libraries
if (SUDACHI_USE_EXTERNAL_VULKAN_UTILITY_LIBRARIES)
    add_subdirectory(Vulkan-Utility-Libraries)
endif()

# TZDB (Time Zone Database)
add_subdirectory(nx_tzdb)

# VMA
if (NOT TARGET GPUOpen::VulkanMemoryAllocator)
    add_subdirectory(VulkanMemoryAllocator)
endif()

if (NOT TARGET LLVM::Demangle)
    add_library(demangle demangle/ItaniumDemangle.cpp)
    target_include_directories(demangle PUBLIC ./demangle)
    if (NOT MSVC)
        target_compile_options(demangle PRIVATE -Wno-deprecated-declarations) # std::is_pod
    endif()
    add_library(LLVM::Demangle ALIAS demangle)
endif()

add_library(stb stb/stb_dxt.cpp)
target_include_directories(stb PUBLIC ./stb)

if (NOT TARGET stb::headers)
    add_library(stb::headers ALIAS stb)
endif()

add_library(tz tz/tz/tz.cpp)
target_include_directories(tz PUBLIC ./tz)

add_library(bc_decoder bc_decoder/bc_decoder.cpp)
target_include_directories(bc_decoder PUBLIC ./bc_decoder)

if (NOT TARGET RenderDoc::API)
    add_library(renderdoc INTERFACE)
    target_include_directories(renderdoc SYSTEM INTERFACE ./renderdoc)
    add_library(RenderDoc::API ALIAS renderdoc)
endif()

if (ANDROID)
   if (ARCHITECTURE_arm64)
       add_subdirectory(libadrenotools)
   endif()
endif()

if (UNIX AND NOT APPLE AND NOT TARGET gamemode::headers)
    add_library(gamemode INTERFACE)
    target_include_directories(gamemode INTERFACE gamemode)
    add_library(gamemode::headers ALIAS gamemode)
endif()

# Breakpad
# https://github.com/microsoft/vcpkg/blob/master/ports/breakpad/CMakeLists.txt
if (SUDACHI_CRASH_DUMPS AND NOT TARGET libbreakpad_client)
    set(BREAKPAD_WIN32_DEFINES
        NOMINMAX
        UNICODE
        WIN32_LEAN_AND_MEAN
        _CRT_SECURE_NO_WARNINGS
        _CRT_SECURE_NO_DEPRECATE
        _CRT_NONSTDC_NO_DEPRECATE
    )

    # libbreakpad
    add_library(libbreakpad STATIC)
    file(GLOB_RECURSE LIBBREAKPAD_SOURCES breakpad/src/processor/*.cc)
    file(GLOB_RECURSE LIBDISASM_SOURCES breakpad/src/third_party/libdisasm/*.c)
    list(FILTER LIBBREAKPAD_SOURCES EXCLUDE REGEX "_unittest|_selftest|synth_minidump|/tests|/testdata|/solaris|microdump_stackwalk|minidump_dump|minidump_stackwalk")
    if (WIN32)
        list(FILTER LIBBREAKPAD_SOURCES EXCLUDE REGEX "/linux|/mac|/android")
        target_compile_definitions(libbreakpad PRIVATE ${BREAKPAD_WIN32_DEFINES})
        target_include_directories(libbreakpad PRIVATE "${CMAKE_GENERATOR_INSTANCE}/DIA SDK/include")
    elseif (APPLE)
        list(FILTER LIBBREAKPAD_SOURCES EXCLUDE REGEX "/linux|/windows|/android")
    else()
        list(FILTER LIBBREAKPAD_SOURCES EXCLUDE REGEX "/mac|/windows|/android")
    endif()
    target_sources(libbreakpad PRIVATE ${LIBBREAKPAD_SOURCES} ${LIBDISASM_SOURCES})
    target_include_directories(libbreakpad
        PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}/breakpad/src
            ${CMAKE_CURRENT_SOURCE_DIR}/breakpad/src/third_party/libdisasm
    )

    # libbreakpad_client
    add_library(libbreakpad_client STATIC)
    file(GLOB LIBBREAKPAD_COMMON_SOURCES breakpad/src/common/*.cc breakpad/src/common/*.c breakpad/src/client/*.cc)

    if (WIN32)
        file(GLOB_RECURSE LIBBREAKPAD_CLIENT_SOURCES breakpad/src/client/windows/*.cc breakpad/src/common/windows/*.cc)
        list(FILTER LIBBREAKPAD_COMMON_SOURCES EXCLUDE REGEX "language.cc|path_helper.cc|stabs_to_module.cc|stabs_reader.cc|minidump_file_writer.cc")
        target_include_directories(libbreakpad_client PRIVATE "${CMAKE_GENERATOR_INSTANCE}/DIA SDK/include")
        target_compile_definitions(libbreakpad_client PRIVATE ${BREAKPAD_WIN32_DEFINES})
    elseif (APPLE)
        target_compile_definitions(libbreakpad_client PRIVATE HAVE_MACH_O_NLIST_H)
        file(GLOB_RECURSE LIBBREAKPAD_CLIENT_SOURCES breakpad/src/client/mac/*.cc breakpad/src/common/mac/*.cc)
        list(APPEND LIBBREAKPAD_CLIENT_SOURCES breakpad/src/common/mac/MachIPC.mm)
    else()
        target_compile_definitions(libbreakpad_client PUBLIC -DHAVE_A_OUT_H)
        file(GLOB_RECURSE LIBBREAKPAD_CLIENT_SOURCES breakpad/src/client/linux/*.cc breakpad/src/common/linux/*.cc)
    endif()
    list(APPEND LIBBREAKPAD_CLIENT_SOURCES ${LIBBREAKPAD_COMMON_SOURCES})
    list(FILTER LIBBREAKPAD_CLIENT_SOURCES EXCLUDE REGEX "/sender|/tests|/unittests|/testcases|_unittest|_test")
    target_sources(libbreakpad_client PRIVATE ${LIBBREAKPAD_CLIENT_SOURCES})
    target_include_directories(libbreakpad_client PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/breakpad/src)

    if (WIN32)
        target_link_libraries(libbreakpad_client PRIVATE wininet.lib)
    elseif (APPLE)
        find_library(CoreFoundation_FRAMEWORK CoreFoundation)
        target_link_libraries(libbreakpad_client PRIVATE ${CoreFoundation_FRAMEWORK})
    else()
        find_library(PTHREAD_LIBRARIES pthread)
        target_compile_definitions(libbreakpad_client PRIVATE HAVE_GETCONTEXT=1)
        if (PTHREAD_LIBRARIES)
            target_link_libraries(libbreakpad_client PRIVATE ${PTHREAD_LIBRARIES})
        endif()
    endif()

    # Host tools for symbol processing
    if (LINUX)
        find_package(ZLIB REQUIRED)

        add_executable(minidump_stackwalk breakpad/src/processor/minidump_stackwalk.cc)
        target_link_libraries(minidump_stackwalk PRIVATE libbreakpad libbreakpad_client)

        add_executable(dump_syms
            breakpad/src/common/dwarf_cfi_to_module.cc
            breakpad/src/common/dwarf_cu_to_module.cc
            breakpad/src/common/dwarf_line_to_module.cc
            breakpad/src/common/dwarf_range_list_handler.cc
            breakpad/src/common/language.cc
            breakpad/src/common/module.cc
            breakpad/src/common/path_helper.cc
            breakpad/src/common/stabs_reader.cc
            breakpad/src/common/stabs_to_module.cc
            breakpad/src/common/dwarf/bytereader.cc
            breakpad/src/common/dwarf/dwarf2diehandler.cc
            breakpad/src/common/dwarf/dwarf2reader.cc
            breakpad/src/common/dwarf/elf_reader.cc
            breakpad/src/common/linux/crc32.cc
            breakpad/src/common/linux/dump_symbols.cc
            breakpad/src/common/linux/elf_symbols_to_module.cc
            breakpad/src/common/linux/elfutils.cc
            breakpad/src/common/linux/file_id.cc
            breakpad/src/common/linux/linux_libc_support.cc
            breakpad/src/common/linux/memory_mapped_file.cc
            breakpad/src/common/linux/safe_readlink.cc
            breakpad/src/tools/linux/dump_syms/dump_syms.cc)
        target_link_libraries(dump_syms PRIVATE libbreakpad_client ZLIB::ZLIB)
    endif()
endif()

# SimpleIni
if (NOT TARGET SimpleIni::SimpleIni)
    add_subdirectory(simpleini)
endif()

# sse2neon
if (ARCHITECTURE_arm64 AND NOT TARGET sse2neon)
    add_library(sse2neon INTERFACE)
    target_include_directories(sse2neon INTERFACE sse2neon)
endif()

