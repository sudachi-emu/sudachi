# SPDX-FileCopyrightText: 2018 sudachi Emulator Project
# SPDX-License-Identifier: GPL-2.0-or-later

# Enable modules to include each other's files
include_directories(.)

# CMake seems to only define _DEBUG on Windows
set_property(DIRECTORY APPEND PROPERTY
    COMPILE_DEFINITIONS $<$<CONFIG:Debug>:_DEBUG> $<$<NOT:$<CONFIG:Debug>>:NDEBUG>)

# Set compilation flags
if (MSVC)
    set(CMAKE_CONFIGURATION_TYPES Debug Release CACHE STRING "" FORCE)

    # Silence "deprecation" warnings
    add_definitions(-D_CRT_SECURE_NO_WARNINGS -D_CRT_NONSTDC_NO_DEPRECATE -D_SCL_SECURE_NO_WARNINGS)

    # Avoid windows.h junk
    add_definitions(-DNOMINMAX)

    # Avoid windows.h from including some usually unused libs like winsocks.h, since this might cause some redefinition errors.
    add_definitions(-DWIN32_LEAN_AND_MEAN)

    # Ensure that projects are built with Unicode support.
    add_definitions(-DUNICODE -D_UNICODE)

    # /W4                 - Level 4 warnings
    # /MP                 - Multi-threaded compilation
    # /Zi                 - Output debugging information
    # /Zm                 - Specifies the precompiled header memory allocation limit
    # /Zo                 - Enhanced debug info for optimized builds
    # /permissive-        - Enables stricter C++ standards conformance checks
    # /EHsc               - C++-only exception handling semantics
    # /utf-8              - Set source and execution character sets to UTF-8
    # /volatile:iso       - Use strict standards-compliant volatile semantics.
    # /Zc:externConstexpr - Allow extern constexpr variables to have external linkage, like the standard mandates
    # /Zc:inline          - Let codegen omit inline functions in object files
    # /Zc:preprocessor    - Enable standards-conforming preprocessor
    # /Zc:throwingNew     - Let codegen assume `operator new` (without std::nothrow) will never return null
    # /GT                 - Supports fiber safety for data allocated using static thread-local storage
    add_compile_options(
        /MP
        /Zm200
        /Zo
        /permissive-
        /EHsc
        /std:c++20
        /utf-8
        /volatile:iso
        /Zc:externConstexpr
        /Zc:inline
        /Zc:preprocessor
        /Zc:throwingNew
        /GT

        # Modules
        /experimental:module- # Explicitly disable module support due to conflicts with precompiled headers.

        # External headers diagnostics
        /external:anglebrackets # Treats all headers included by #include <header>, where the header file is enclosed in angle brackets (< >), as external headers
        /external:W0            # Sets the default warning level to 0 for external headers, effectively disabling warnings for them.

        # Warnings
        # /W4
        # /WX

        # /we4062 # Enumerator 'identifier' in a switch of enum 'enumeration' is not handled
        # /we4189 # 'identifier': local variable is initialized but not referenced
        # /we4265 # 'class': class has virtual functions, but destructor is not virtual
        # /we4388 # 'expression': signed/unsigned mismatch
        # /we4389 # 'operator': signed/unsigned mismatch
        # /we4456 # Declaration of 'identifier' hides previous local declaration
        # /we4457 # Declaration of 'identifier' hides function parameter
        # /we4458 # Declaration of 'identifier' hides class member
        # /we4459 # Declaration of 'identifier' hides global declaration
        # /we4505 # 'function': unreferenced local function has been removed
        # /we4547 # 'operator': operator before comma has no effect; expected operator with side-effect
        # /we4549 # 'operator1': operator before comma has no effect; did you intend 'operator2'?
        # /we4555 # Expression has no effect; expected expression with side-effect
        # /we4826 # Conversion from 'type1' to 'type2' is sign-extended. This may cause unexpected runtime behavior.
        # /we5038 # data member 'member1' will be initialized after data member 'member2'
        # /we5233 # explicit lambda capture 'identifier' is not used
        # /we5245 # 'function': unreferenced function with internal linkage has been removed

        # /wd4189
        # /wd4101
        # /wd4100 # 'identifier': unreferenced formal parameter
        # /wd4324 # 'struct_name': structure was padded due to __declspec(align())
        # /wd4201 # nonstandard extension used : nameless struct/union
        # /wd4702 # unreachable code (when used with LTO)
    )

    if (USE_CCACHE OR SUDACHI_USE_PRECOMPILED_HEADERS)
    # when caching, we need to use /Z7 to downgrade debug info to use an older but more cacheable format
    # Precompiled headers are deleted if not using /Z7. See https://github.com/nanoant/CMakePCHCompiler/issues/21
        add_compile_options(/Z7)
        # Avoid D9025 warning
        string(REPLACE "/Zi" "" CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")
        string(REPLACE "/Zi" "" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
    else()
        add_compile_options(/Zi)
    endif()

    if (ARCHITECTURE_x86_64)
        add_compile_options(/QIntel-jcc-erratum)
    endif()

    # /GS- - No stack buffer overflow checks
    add_compile_options("$<$<CONFIG:Release>:/GS->")

    set(CMAKE_EXE_LINKER_FLAGS_DEBUG   "/DEBUG /MANIFEST:NO" CACHE STRING "" FORCE)
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "/DEBUG /MANIFEST:NO /INCREMENTAL:NO /OPT:REF,ICF" CACHE STRING "" FORCE)
else()
    add_compile_options(
        -fwrapv

        -Werror=all
        -Werror=extra
        -Werror=missing-declarations
        -Werror=shadow
        -Werror=unused

        -Wno-attributes
        -Wno-invalid-offsetof
        -Wno-unused-parameter
        -Wno-missing-field-initializers
    )

    if (CMAKE_CXX_COMPILER_ID MATCHES Clang) # Clang or AppleClang
        add_compile_options(
            -Wno-braced-scalar-init
            -Wno-unused-private-field
            -Wno-nullability-completeness
            -Werror=shadow-uncaptured-local
            -Werror=implicit-fallthrough
            -Werror=type-limits
        )
    endif()

    if (ARCHITECTURE_x86_64)
        add_compile_options("-mcx16")
    endif()

    if (APPLE AND CMAKE_CXX_COMPILER_ID STREQUAL Clang)
        add_compile_options("-stdlib=libc++")
    endif()

    # GCC bugs
    if (CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL "11" AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        # These diagnostics would be great if they worked, but are just completely broken
        # and produce bogus errors on external libraries like fmt.
        add_compile_options(
            -Wno-array-bounds
            -Wno-stringop-overread
            -Wno-stringop-overflow
        )
    endif()

    # Set file offset size to 64 bits.
    #
    # On modern Unixes, this is typically already the case. The lone exception is
    # glibc, which may default to 32 bits. glibc allows this to be configured
    # by setting _FILE_OFFSET_BITS.
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux" OR MINGW)
        add_definitions(-D_FILE_OFFSET_BITS=64)
    endif()

    if (MINGW)
        add_definitions(-DMINGW_HAS_SECURE_API)
        add_compile_options("-msse4.1")

        if (MINGW_STATIC_BUILD)
            add_definitions(-DQT_STATICPLUGIN)
            add_compile_options("-static")
        endif()
    endif()

    if(CMAKE_SYSTEM_NAME STREQUAL "Linux" OR MINGW)
        # GNU ar: Create thin archive files.
        # Requires binutils-2.19 or later.
        set(CMAKE_C_ARCHIVE_CREATE   "<CMAKE_AR> qcTP <TARGET> <LINK_FLAGS> <OBJECTS>")
        set(CMAKE_C_ARCHIVE_APPEND   "<CMAKE_AR> qTP  <TARGET> <LINK_FLAGS> <OBJECTS>")
        set(CMAKE_CXX_ARCHIVE_CREATE "<CMAKE_AR> qcTP <TARGET> <LINK_FLAGS> <OBJECTS>")
        set(CMAKE_CXX_ARCHIVE_APPEND "<CMAKE_AR> qTP  <TARGET> <LINK_FLAGS> <OBJECTS>")
    endif()
endif()

add_subdirectory(common)
add_subdirectory(core)
add_subdirectory(audio_core)
add_subdirectory(video_core)
add_subdirectory(hid_core)
add_subdirectory(network)
add_subdirectory(input_common)
add_subdirectory(frontend_common)
add_subdirectory(shader_recompiler)

if (SUDACHI_ROOM)
     add_subdirectory(dedicated_room)
endif()

if (SUDACHI_TESTS)
    add_subdirectory(tests)
endif()

if (ENABLE_SDL2)
    add_subdirectory(sudachi_cmd)
endif()

if (ENABLE_QT)
    add_subdirectory(sudachi)
endif()

if (ENABLE_WEB_SERVICE)
    add_subdirectory(web_service)
endif()

if (ANDROID)
    add_subdirectory(android/sudachi/src/main/jni)
    target_include_directories(sudachi-android PRIVATE android/sudachi/src/main)
endif()
