# SPDX-FileCopyrightText: 2022 sudachi Emulator Project
# SPDX-License-Identifier: GPL-3.0-or-later

set(MINGW_PREFIX                /usr/x86_64-w64-mingw32/)
set(CMAKE_SYSTEM_NAME           Windows)
set(CMAKE_SYSTEM_PROCESSOR      x86_64)

set(CMAKE_FIND_ROOT_PATH        ${MINGW_PREFIX})
set(SDL3_PATH                   ${MINGW_PREFIX})
set(MINGW_TOOL_PREFIX           ${CMAKE_SYSTEM_PROCESSOR}-w64-mingw32-)

# Specify the cross compiler
set(CMAKE_C_COMPILER            ${MINGW_TOOL_PREFIX}clang)
set(CMAKE_CXX_COMPILER          ${MINGW_TOOL_PREFIX}clang++)
set(CMAKE_RC_COMPILER           ${MINGW_TOOL_PREFIX}windres)
set(CMAKE_C_COMPILER_AR         ${MINGW_TOOL_PREFIX}ar)
set(CMAKE_CXX_COMPILER_AR       ${MINGW_TOOL_PREFIX}ar)
set(CMAKE_C_COMPILER_RANLIB     ${MINGW_TOOL_PREFIX}ranlib)
set(CMAKE_CXX_COMPILER_RANLIB   ${MINGW_TOOL_PREFIX}ranlib)

# Mingw tools
set(STRIP                       ${MINGW_TOOL_PREFIX}strip)
set(WINDRES                     ${MINGW_TOOL_PREFIX}windres)
set(ENV{PKG_CONFIG}             ${MINGW_TOOL_PREFIX}pkg-config)

# ccache wrapper
option(USE_CCACHE "Use ccache for compilation" OFF)
if(USE_CCACHE)
    find_program(CCACHE ccache)
    if(CCACHE)
        message(STATUS "Using ccache found in PATH")
        set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ${CCACHE})
        set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK ${CCACHE})
    else(CCACHE)
        message(WARNING "USE_CCACHE enabled, but no ccache found")
    endif(CCACHE)
endif(USE_CCACHE)

# Search for programs in the build host directories
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)


# Echo modified cmake vars to screen for debugging purposes
if(NOT DEFINED ENV{MINGW_DEBUG_INFO})
        message("")
        message("Custom cmake vars: (blank = system default)")
        message("-----------------------------------------")
        message("* CMAKE_C_COMPILER                     : ${CMAKE_C_COMPILER}")
        message("* CMAKE_CXX_COMPILER                   : ${CMAKE_CXX_COMPILER}")
        message("* CMAKE_RC_COMPILER                    : ${CMAKE_RC_COMPILER}")
        message("* WINDRES                              : ${WINDRES}")
        message("* ENV{PKG_CONFIG}                      : $ENV{PKG_CONFIG}")
        message("* STRIP                                : ${STRIP}")
        message("* USE_CCACHE                           : ${USE_CCACHE}")
        message("")
        # So that the debug info only appears once
        set(ENV{MINGW_DEBUG_INFO} SHOWN)
endif()
