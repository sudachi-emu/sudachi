# SPDX-FileCopyrightText: 2018 sudachi Emulator Project
# SPDX-License-Identifier: GPL-2.0-or-later

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

# Set the RPATH for Qt Libraries
# This must be done before the `sudachi` target is created
if (SUDACHI_USE_BUNDLED_QT AND (${CMAKE_SYSTEM_NAME} STREQUAL "Linux"))
    set(CMAKE_BUILD_RPATH "${CMAKE_BINARY_DIR}/bin/lib/")
endif()

add_executable(sudachi
    Info.plist
    about_dialog.cpp
    about_dialog.h
    aboutdialog.ui
    applets/qt_amiibo_settings.cpp
    applets/qt_amiibo_settings.h
    applets/qt_amiibo_settings.ui
    applets/qt_controller.cpp
    applets/qt_controller.h
    applets/qt_controller.ui
    applets/qt_error.cpp
    applets/qt_error.h
    applets/qt_profile_select.cpp
    applets/qt_profile_select.h
    applets/qt_software_keyboard.cpp
    applets/qt_software_keyboard.h
    applets/qt_software_keyboard.ui
    applets/qt_web_browser.cpp
    applets/qt_web_browser.h
    applets/qt_web_browser_scripts.h
    bootmanager.cpp
    bootmanager.h
    compatdb.ui
    compatibility_list.cpp
    compatibility_list.h
    configuration/configuration_shared.cpp
    configuration/configuration_shared.h
    configuration/configure.ui
    configuration/configure_applets.cpp
    configuration/configure_applets.h
    configuration/configure_applets.ui
    configuration/configure_audio.cpp
    configuration/configure_audio.h
    configuration/configure_audio.ui
    configuration/configure_camera.cpp
    configuration/configure_camera.h
    configuration/configure_camera.ui
    configuration/configure_cpu.cpp
    configuration/configure_cpu.h
    configuration/configure_cpu.ui
    configuration/configure_cpu_debug.cpp
    configuration/configure_cpu_debug.h
    configuration/configure_cpu_debug.ui
    configuration/configure_debug.cpp
    configuration/configure_debug.h
    configuration/configure_debug.ui
    configuration/configure_debug_controller.cpp
    configuration/configure_debug_controller.h
    configuration/configure_debug_controller.ui
    configuration/configure_debug_tab.cpp
    configuration/configure_debug_tab.h
    configuration/configure_debug_tab.ui
    configuration/configure_dialog.cpp
    configuration/configure_dialog.h
    configuration/configure_filesystem.cpp
    configuration/configure_filesystem.h
    configuration/configure_filesystem.ui
    configuration/configure_general.cpp
    configuration/configure_general.h
    configuration/configure_general.ui
    configuration/configure_graphics.cpp
    configuration/configure_graphics.h
    configuration/configure_graphics.ui
    configuration/configure_graphics_advanced.cpp
    configuration/configure_graphics_advanced.h
    configuration/configure_graphics_advanced.ui
    configuration/configure_hotkeys.cpp
    configuration/configure_hotkeys.h
    configuration/configure_hotkeys.ui
    configuration/configure_input.cpp
    configuration/configure_input.h
    configuration/configure_input.ui
    configuration/configure_input_advanced.cpp
    configuration/configure_input_advanced.h
    configuration/configure_input_advanced.ui
    configuration/configure_input_per_game.cpp
    configuration/configure_input_per_game.h
    configuration/configure_input_per_game.ui
    configuration/configure_input_player.cpp
    configuration/configure_input_player.h
    configuration/configure_input_player.ui
    configuration/configure_input_player_widget.cpp
    configuration/configure_input_player_widget.h
    configuration/configure_input_profile_dialog.cpp
    configuration/configure_input_profile_dialog.h
    configuration/configure_input_profile_dialog.ui
    configuration/configure_linux_tab.cpp
    configuration/configure_linux_tab.h
    configuration/configure_linux_tab.ui
    configuration/configure_mouse_panning.cpp
    configuration/configure_mouse_panning.h
    configuration/configure_mouse_panning.ui
    configuration/configure_motion_touch.cpp
    configuration/configure_motion_touch.h
    configuration/configure_motion_touch.ui
    configuration/configure_per_game.cpp
    configuration/configure_per_game.h
    configuration/configure_per_game.ui
    configuration/configure_per_game_addons.cpp
    configuration/configure_per_game_addons.h
    configuration/configure_per_game_addons.ui
    configuration/configure_profile_manager.cpp
    configuration/configure_profile_manager.h
    configuration/configure_profile_manager.ui
    configuration/configure_ringcon.cpp
    configuration/configure_ringcon.h
    configuration/configure_ringcon.ui
    configuration/configure_network.cpp
    configuration/configure_network.h
    configuration/configure_network.ui
    configuration/configure_system.cpp
    configuration/configure_system.h
    configuration/configure_system.ui
    configuration/configure_tas.cpp
    configuration/configure_tas.h
    configuration/configure_tas.ui
    configuration/configure_touch_from_button.cpp
    configuration/configure_touch_from_button.h
    configuration/configure_touch_from_button.ui
    configuration/configure_touchscreen_advanced.cpp
    configuration/configure_touchscreen_advanced.h
    configuration/configure_touchscreen_advanced.ui
    configuration/configure_touch_widget.h
    configuration/configure_ui.cpp
    configuration/configure_ui.h
    configuration/configure_ui.ui
    configuration/configure_vibration.cpp
    configuration/configure_vibration.h
    configuration/configure_vibration.ui
    configuration/configure_web.cpp
    configuration/configure_web.h
    configuration/configure_web.ui
    configuration/input_profiles.cpp
    configuration/input_profiles.h
    configuration/shared_translation.cpp
    configuration/shared_translation.h
    configuration/shared_widget.cpp
    configuration/shared_widget.h
    configuration/qt_config.cpp
    configuration/qt_config.h
    debugger/console.cpp
    debugger/console.h
    debugger/controller.cpp
    debugger/controller.h
    debugger/profiler.cpp
    debugger/profiler.h
    debugger/wait_tree.cpp
    debugger/wait_tree.h
    discord.h
    game_list.cpp
    game_list.h
    game_list_p.h
    game_list_worker.cpp
    game_list_worker.h
    hotkeys.cpp
    hotkeys.h
    install_dialog.cpp
    install_dialog.h
    loading_screen.cpp
    loading_screen.h
    loading_screen.ui
    main.cpp
    main.h
    main.ui
    multiplayer/chat_room.cpp
    multiplayer/chat_room.h
    multiplayer/chat_room.ui
    multiplayer/client_room.h
    multiplayer/client_room.cpp
    multiplayer/client_room.ui
    multiplayer/direct_connect.cpp
    multiplayer/direct_connect.h
    multiplayer/direct_connect.ui
    multiplayer/host_room.cpp
    multiplayer/host_room.h
    multiplayer/host_room.ui
    multiplayer/lobby.cpp
    multiplayer/lobby.h
    multiplayer/lobby.ui
    multiplayer/lobby_p.h
    multiplayer/message.cpp
    multiplayer/message.h
    multiplayer/moderation_dialog.cpp
    multiplayer/moderation_dialog.h
    multiplayer/moderation_dialog.ui
    multiplayer/state.cpp
    multiplayer/state.h
    multiplayer/validation.h
    play_time_manager.cpp
    play_time_manager.h
    precompiled_headers.h
    qt_common.cpp
    qt_common.h
    startup_checks.cpp
    startup_checks.h
    uisettings.cpp
    uisettings.h
    util/clickable_label.cpp
    util/clickable_label.h
    util/controller_navigation.cpp
    util/controller_navigation.h
    util/limitable_input_dialog.cpp
    util/limitable_input_dialog.h
    util/overlay_dialog.cpp
    util/overlay_dialog.h
    util/overlay_dialog.ui
    util/sequence_dialog/sequence_dialog.cpp
    util/sequence_dialog/sequence_dialog.h
    util/url_request_interceptor.cpp
    util/url_request_interceptor.h
    util/util.cpp
    util/util.h
    vk_device_info.cpp
    vk_device_info.h
    compatdb.cpp
    compatdb.h
    sudachi.qrc
    sudachi.rc
)

if (SUDACHI_CRASH_DUMPS)
    target_sources(sudachi PRIVATE
        breakpad.cpp
        breakpad.h
    )

    target_link_libraries(sudachi PRIVATE libbreakpad_client)
    target_compile_definitions(sudachi PRIVATE SUDACHI_CRASH_DUMPS)
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_definitions(sudachi PRIVATE
        $<$<VERSION_LESS:$<CXX_COMPILER_VERSION>,15>:CANNOT_EXPLICITLY_INSTANTIATE>
    )
endif()

file(GLOB COMPAT_LIST
     ${PROJECT_BINARY_DIR}/dist/compatibility_list/compatibility_list.qrc
     ${PROJECT_BINARY_DIR}/dist/compatibility_list/compatibility_list.json)
file(GLOB_RECURSE ICONS ${PROJECT_SOURCE_DIR}/dist/icons/*)
file(GLOB_RECURSE THEMES ${PROJECT_SOURCE_DIR}/dist/qt_themes/*)

if (ENABLE_QT_TRANSLATION)
    set(SUDACHI_QT_LANGUAGES "${PROJECT_SOURCE_DIR}/dist/languages" CACHE PATH "Path to the translation bundle for the Qt frontend")
    option(GENERATE_QT_TRANSLATION "Generate en.ts as the translation source file" OFF)
    option(WORKAROUND_BROKEN_LUPDATE "Run lupdate directly through CMake if Qt's convenience wrappers don't work" OFF)

    # Update source TS file if enabled
    if (GENERATE_QT_TRANSLATION)
        get_target_property(SRCS sudachi SOURCES)
        # these calls to qt_create_translation also creates a rule to generate en.qm which conflicts with providing english plurals
        # so we have to set a OUTPUT_LOCATION so that we don't have multiple rules to generate en.qm
        set_source_files_properties(${SUDACHI_QT_LANGUAGES}/en.ts PROPERTIES OUTPUT_LOCATION "${CMAKE_CURRENT_BINARY_DIR}/translations")
        if (WORKAROUND_BROKEN_LUPDATE)
            add_custom_command(OUTPUT ${SUDACHI_QT_LANGUAGES}/en.ts
                COMMAND lupdate
                    -source-language en_US
                    -target-language en_US
                    ${SRCS}
                    ${UIS}
                    -ts ${SUDACHI_QT_LANGUAGES}/en.ts
                DEPENDS
                    ${SRCS}
                    ${UIS}
                WORKING_DIRECTORY
                    ${CMAKE_CURRENT_SOURCE_DIR}
            )
        else()
            qt_create_translation(QM_FILES
                ${SRCS}
                ${UIS}
                ${SUDACHI_QT_LANGUAGES}/en.ts
            OPTIONS
                -source-language en_US
                -target-language en_US
            )
        endif()

        # Generate plurals into dist/english_plurals/generated_en.ts so it can be used to revise dist/english_plurals/en.ts
        set(GENERATED_PLURALS_FILE ${PROJECT_SOURCE_DIR}/dist/english_plurals/generated_en.ts)
        set_source_files_properties(${GENERATED_PLURALS_FILE} PROPERTIES OUTPUT_LOCATION "${CMAKE_CURRENT_BINARY_DIR}/plurals")
        if (WORKAROUND_BROKEN_LUPDATE)
            add_custom_command(OUTPUT ${GENERATED_PLURALS_FILE}
                COMMAND lupdate
                    -source-language en_US
                    -target-language en_US
                    ${SRCS}
                    ${UIS}
                    -ts ${GENERATED_PLURALS_FILE}
                DEPENDS
                    ${SRCS}
                    ${UIS}
                WORKING_DIRECTORY
                    ${CMAKE_CURRENT_SOURCE_DIR}
            )
        else()
            qt_create_translation(QM_FILES ${SRCS} ${UIS} ${GENERATED_PLURALS_FILE} OPTIONS -pluralonly -source-language en_US -target-language en_US)
        endif()

        add_custom_target(translation ALL DEPENDS ${SUDACHI_QT_LANGUAGES}/en.ts ${GENERATED_PLURALS_FILE})
    endif()

    # Find all TS files except en.ts
    file(GLOB_RECURSE LANGUAGES_TS ${SUDACHI_QT_LANGUAGES}/*.ts)
    list(REMOVE_ITEM LANGUAGES_TS ${SUDACHI_QT_LANGUAGES}/en.ts)

    # Compile TS files to QM files
    qt_add_translation(LANGUAGES_QM ${LANGUAGES_TS})

    # Compile english plurals TS file to en.qm
    qt_add_translation(LANGUAGES_QM ${PROJECT_SOURCE_DIR}/dist/english_plurals/en.ts)

    # Build a QRC file from the QM file list
    set(LANGUAGES_QRC ${CMAKE_CURRENT_BINARY_DIR}/languages.qrc)
    file(WRITE ${LANGUAGES_QRC} "<RCC><qresource prefix=\"languages\">\n")
    foreach (QM ${LANGUAGES_QM})
        get_filename_component(QM_FILE ${QM} NAME)
        file(APPEND ${LANGUAGES_QRC} "<file>${QM_FILE}</file>\n")
    endforeach (QM)
    file(APPEND ${LANGUAGES_QRC} "</qresource></RCC>")

    # Add the QRC file to package in all QM files
    qt_add_resources(LANGUAGES ${LANGUAGES_QRC})
else()
    set(LANGUAGES)
endif()

target_sources(sudachi
    PRIVATE
        ${COMPAT_LIST}
        ${ICONS}
        ${LANGUAGES}
        ${THEMES}
)

if (APPLE)
    set(MACOSX_ICON "../../dist/sudachi.icns")
    set_source_files_properties(${MACOSX_ICON} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    target_sources(sudachi PRIVATE ${MACOSX_ICON})
    set_target_properties(sudachi PROPERTIES MACOSX_BUNDLE TRUE)
    set_target_properties(sudachi PROPERTIES MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/Info.plist)

    if (NOT USE_SYSTEM_MOLTENVK)
        set(MOLTENVK_PLATFORM "macOS")
        set(MOLTENVK_VERSION "v1.2.7")
        download_moltenvk_external(${MOLTENVK_PLATFORM} ${MOLTENVK_VERSION})
    endif()
    find_library(MOLTENVK_LIBRARY MoltenVK REQUIRED)
    message(STATUS "Using MoltenVK at ${MOLTENVK_LIBRARY}.")
    set_source_files_properties(${MOLTENVK_LIBRARY} PROPERTIES MACOSX_PACKAGE_LOCATION Frameworks
                                    XCODE_FILE_ATTRIBUTES "CodeSignOnCopy")
    target_sources(sudachi PRIVATE ${MOLTENVK_LIBRARY})

elseif(WIN32)
    # compile as a win32 gui application instead of a console application
    if (QT_VERSION VERSION_GREATER_EQUAL 6)
        target_link_libraries(sudachi PRIVATE Qt6::EntryPointPrivate)
    else()
        target_link_libraries(sudachi PRIVATE Qt5::WinMain)
    endif()
    if(MSVC)
        target_link_libraries(sudachi PRIVATE version.lib)
        set_target_properties(sudachi PROPERTIES LINK_FLAGS_RELEASE "/SUBSYSTEM:WINDOWS")
    elseif(MINGW)
        set_target_properties(sudachi PROPERTIES LINK_FLAGS_RELEASE "-Wl,--subsystem,windows")
    endif()
endif()

target_link_libraries(sudachi PRIVATE common core input_common frontend_common network video_core)
target_link_libraries(sudachi PRIVATE Boost::headers glad Qt${QT_MAJOR_VERSION}::Widgets)
target_link_libraries(sudachi PRIVATE ${PLATFORM_LIBRARIES} Threads::Threads)

target_link_libraries(sudachi PRIVATE Vulkan::Headers)
if (NOT WIN32)
    target_include_directories(sudachi PRIVATE ${Qt${QT_MAJOR_VERSION}Gui_PRIVATE_INCLUDE_DIRS})
endif()
if (UNIX AND NOT APPLE)
    target_link_libraries(sudachi PRIVATE Qt${QT_MAJOR_VERSION}::DBus)
endif()

target_compile_definitions(sudachi PRIVATE
    # Use QStringBuilder for string concatenation to reduce
    # the overall number of temporary strings created.
    -DQT_USE_QSTRINGBUILDER

    # Disable implicit conversions from/to C strings
    -DQT_NO_CAST_FROM_ASCII
    -DQT_NO_CAST_TO_ASCII

    # Disable implicit type narrowing in signal/slot connect() calls.
    -DQT_NO_NARROWING_CONVERSIONS_IN_CONNECT

    # Disable unsafe overloads of QProcess' start() function.
    -DQT_NO_PROCESS_COMBINED_ARGUMENT_START

    # Disable implicit QString->QUrl conversions to enforce use of proper resolving functions.
    -DQT_NO_URL_CAST_FROM_STRING
)

if (SUDACHI_ENABLE_COMPATIBILITY_REPORTING)
    target_compile_definitions(sudachi PRIVATE -DSUDACHI_ENABLE_COMPATIBILITY_REPORTING)
endif()

if (USE_DISCORD_PRESENCE)
    target_sources(sudachi PUBLIC
        discord_impl.cpp
        discord_impl.h
    )
    target_link_libraries(sudachi PRIVATE DiscordRPC::discord-rpc httplib::httplib Qt${QT_MAJOR_VERSION}::Network)
    target_compile_definitions(sudachi PRIVATE -DUSE_DISCORD_PRESENCE)
endif()

if (ENABLE_WEB_SERVICE)
    target_compile_definitions(sudachi PRIVATE -DENABLE_WEB_SERVICE)
endif()

if (SUDACHI_USE_QT_MULTIMEDIA)
    target_link_libraries(sudachi PRIVATE Qt${QT_MAJOR_VERSION}::Multimedia)
    target_compile_definitions(sudachi PRIVATE -DSUDACHI_USE_QT_MULTIMEDIA)
endif ()

if (SUDACHI_USE_QT_WEB_ENGINE)
    target_link_libraries(sudachi PRIVATE Qt${QT_MAJOR_VERSION}::WebEngineCore Qt${QT_MAJOR_VERSION}::WebEngineWidgets)
    target_compile_definitions(sudachi PRIVATE -DSUDACHI_USE_QT_WEB_ENGINE)
endif ()

if(UNIX AND NOT APPLE)
    install(TARGETS sudachi)
endif()

if (WIN32 AND QT_VERSION VERSION_GREATER_EQUAL 6)
    set(SUDACHI_EXE_DIR "$<TARGET_FILE_DIR:sudachi>")
    add_custom_command(TARGET sudachi POST_BUILD COMMAND ${WINDEPLOYQT_EXECUTABLE} "${SUDACHI_EXE_DIR}/sudachi.exe" --dir "${SUDACHI_EXE_DIR}" --libdir "${SUDACHI_EXE_DIR}" --plugindir "${SUDACHI_EXE_DIR}/plugins" --no-compiler-runtime --no-opengl-sw --no-system-d3d-compiler --no-translations --verbose 0)
endif()

if (SUDACHI_USE_BUNDLED_QT AND QT_VERSION VERSION_LESS 6)
    include(CopySudachiQt5Deps)
    copy_sudachi_Qt5_deps(sudachi)
endif()

if (ENABLE_SDL3)
    target_link_libraries(sudachi PRIVATE SDL3::SDL3)
    target_compile_definitions(sudachi PRIVATE HAVE_SDL3)
endif()

if (MSVC)
    include(CopySudachiSDLDeps)
    include(CopySudachiFFmpegDeps)
    copy_sudachi_SDL_deps(sudachi)
    copy_sudachi_FFmpeg_deps(sudachi)
endif()

if (NOT APPLE AND ENABLE_OPENGL)
    target_compile_definitions(sudachi PRIVATE HAS_OPENGL)
endif()

if (ARCHITECTURE_x86_64 OR ARCHITECTURE_arm64)
    target_link_libraries(sudachi PRIVATE dynarmic::dynarmic)
endif()

if (SUDACHI_USE_PRECOMPILED_HEADERS)
    target_precompile_headers(sudachi PRIVATE precompiled_headers.h)
endif()

create_target_directory_groups(sudachi)
