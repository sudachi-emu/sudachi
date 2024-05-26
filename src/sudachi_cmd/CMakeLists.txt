# SPDX-FileCopyrightText: 2018 sudachi Emulator Project
# SPDX-License-Identifier: GPL-2.0-or-later

# Credits to Samantas5855 and others for this function.
function(create_resource file output filename)
    # Read hex data from file
    file(READ ${file} filedata HEX)
    # Convert hex data for C compatibility
    string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," filedata ${filedata})
    # Write data to output file
    set(RESOURCES_DIR "${PROJECT_BINARY_DIR}/dist" PARENT_SCOPE)
    file(WRITE "${PROJECT_BINARY_DIR}/dist/${output}" "const unsigned char ${filename}[] = {${filedata}};\nconst unsigned ${filename}_size = sizeof(${filename});\n")
endfunction()

add_executable(sudachi-cmd
    emu_window/emu_window_sdl3.cpp
    emu_window/emu_window_sdl3.h
    emu_window/emu_window_sdl3_gl.cpp
    emu_window/emu_window_sdl3_gl.h
    emu_window/emu_window_sdl3_null.cpp
    emu_window/emu_window_sdl3_null.h
    emu_window/emu_window_sdl3_vk.cpp
    emu_window/emu_window_sdl3_vk.h
    precompiled_headers.h
    sdl_config.cpp
    sdl_config.h
    sudachi.cpp
    sudachi.rc
)

target_link_libraries(sudachi-cmd PRIVATE common core input_common frontend_common)
target_link_libraries(sudachi-cmd PRIVATE glad)
if (MSVC)
    target_link_libraries(sudachi-cmd PRIVATE getopt)
endif()
target_link_libraries(sudachi-cmd PRIVATE ${PLATFORM_LIBRARIES} Threads::Threads)

create_resource("../../dist/sudachi.bmp" "sudachi_cmd/sudachi_icon.h" "sudachi_icon")
target_include_directories(sudachi-cmd PRIVATE ${RESOURCES_DIR})

target_link_libraries(sudachi-cmd PRIVATE SDL3::SDL3 Vulkan::Headers)

if(UNIX AND NOT APPLE)
    install(TARGETS sudachi-cmd)
endif()

if(WIN32)
    # compile as a win32 gui application instead of a console application
    if(MSVC)
        set_target_properties(sudachi-cmd PROPERTIES LINK_FLAGS_RELEASE "/SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup")
    elseif(MINGW)
        set_target_properties(sudachi-cmd PROPERTIES LINK_FLAGS_RELEASE "-Wl,--subsystem,windows")
    endif()
endif()

if (MSVC)
    include(CopySudachiSDLDeps)
    copy_sudachi_SDL_deps(sudachi-cmd)
endif()

if (SUDACHI_USE_PRECOMPILED_HEADERS)
    target_precompile_headers(sudachi-cmd PRIVATE precompiled_headers.h)
endif()

create_target_directory_groups(sudachi-cmd)
