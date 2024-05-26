# SPDX-FileCopyrightText: 2018 sudachi Emulator Project
# SPDX-License-Identifier: GPL-2.0-or-later

add_library(input_common STATIC
    drivers/camera.cpp
    drivers/camera.h
    drivers/keyboard.cpp
    drivers/keyboard.h
    drivers/mouse.cpp
    drivers/mouse.h
    drivers/tas_input.cpp
    drivers/tas_input.h
    drivers/touch_screen.cpp
    drivers/touch_screen.h
    drivers/udp_client.cpp
    drivers/udp_client.h
    drivers/virtual_amiibo.cpp
    drivers/virtual_amiibo.h
    drivers/virtual_gamepad.cpp
    drivers/virtual_gamepad.h
    helpers/stick_from_buttons.cpp
    helpers/stick_from_buttons.h
    helpers/touch_from_buttons.cpp
    helpers/touch_from_buttons.h
    helpers/udp_protocol.cpp
    helpers/udp_protocol.h
    input_engine.cpp
    input_engine.h
    input_mapping.cpp
    input_mapping.h
    input_poller.cpp
    input_poller.h
    main.cpp
    main.h
    precompiled_headers.h
)

if (MSVC)
    target_compile_options(input_common PRIVATE
        /we4242 # 'identifier': conversion from 'type1' to 'type2', possible loss of data
        /we4254 # 'operator': conversion from 'type1:field_bits' to 'type2:field_bits', possible loss of data
        /we4800 # Implicit conversion from 'type' to bool. Possible information loss
    )
else()
    target_compile_options(input_common PRIVATE
        -Werror=conversion
    )
endif()

if (ENABLE_SDL3)
    target_sources(input_common PRIVATE
        drivers/joycon.cpp
        drivers/joycon.h
        drivers/sdl_driver.cpp
        drivers/sdl_driver.h
        helpers/joycon_driver.cpp
        helpers/joycon_driver.h
        helpers/joycon_protocol/calibration.cpp
        helpers/joycon_protocol/calibration.h
        helpers/joycon_protocol/common_protocol.cpp
        helpers/joycon_protocol/common_protocol.h
        helpers/joycon_protocol/generic_functions.cpp
        helpers/joycon_protocol/generic_functions.h
        helpers/joycon_protocol/joycon_types.h
        helpers/joycon_protocol/irs.cpp
        helpers/joycon_protocol/irs.h
        helpers/joycon_protocol/nfc.cpp
        helpers/joycon_protocol/nfc.h
        helpers/joycon_protocol/poller.cpp
        helpers/joycon_protocol/poller.h
        helpers/joycon_protocol/ringcon.cpp
        helpers/joycon_protocol/ringcon.h
        helpers/joycon_protocol/rumble.cpp
        helpers/joycon_protocol/rumble.h
    )
    target_link_libraries(input_common PRIVATE SDL3::SDL3)
    target_compile_definitions(input_common PRIVATE HAVE_SDL3)
endif()

if (ENABLE_LIBUSB)
    target_sources(input_common PRIVATE
        drivers/gc_adapter.cpp
        drivers/gc_adapter.h
    )
    target_link_libraries(input_common PRIVATE libusb::usb)
    target_compile_definitions(input_common PRIVATE HAVE_LIBUSB)
endif()

create_target_directory_groups(input_common)
target_link_libraries(input_common PUBLIC hid_core PRIVATE common Boost::headers)

if (SUDACHI_USE_PRECOMPILED_HEADERS)
    target_precompile_headers(input_common PRIVATE precompiled_headers.h)
endif()

if (ANDROID)
    target_sources(input_common PRIVATE
        drivers/android.cpp
        drivers/android.h
    )
    target_link_libraries(input_common PRIVATE android)
endif()
