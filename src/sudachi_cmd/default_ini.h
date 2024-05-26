// SPDX-FileCopyrightText: 2014 Citra Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

namespace DefaultINI {

const char* sdl3_config_file =
    R"(
[ControlsP0]
# The input devices and parameters for each Switch native input
# The config section determines the player number where the config will be applied on. For example "ControlsP0", "ControlsP1", ...
# It should be in the format of "engine:[engine_name],[param1]:[value1],[param2]:[value2]..."
# Escape characters $0 (for ':'), $1 (for ',') and $2 (for '$') can be used in values

# Indicates if this player should be connected at boot
# 0 (default): Disabled, 1: Enabled
connected=

# for button input, the following devices are available:
#  - "keyboard" (default) for keyboard input. Required parameters:
#      - "code": the code of the key to bind
#  - "sdl" for joystick input using SDL. Required parameters:
#      - "guid": SDL identification GUID of the joystick
#      - "port": the index of the joystick to bind
#      - "button"(optional): the index of the button to bind
#      - "hat"(optional): the index of the hat to bind as direction buttons
#      - "axis"(optional): the index of the axis to bind
#      - "direction"(only used for hat): the direction name of the hat to bind. Can be "up", "down", "left" or "right"
#      - "threshold"(only used for axis): a float value in (-1.0, 1.0) which the button is
#          triggered if the axis value crosses
#      - "direction"(only used for axis): "+" means the button is triggered when the axis value
#          is greater than the threshold; "-" means the button is triggered when the axis value
#          is smaller than the threshold
button_a=
button_b=
button_x=
button_y=
button_lstick=
button_rstick=
button_l=
button_r=
button_zl=
button_zr=
button_plus=
button_minus=
button_dleft=
button_dup=
button_dright=
button_ddown=
button_lstick_left=
button_lstick_up=
button_lstick_right=
button_lstick_down=
button_sl=
button_sr=
button_home=
button_screenshot=

# for analog input, the following devices are available:
#  - "analog_from_button" (default) for emulating analog input from direction buttons. Required parameters:
#      - "up", "down", "left", "right": sub-devices for each direction.
#          Should be in the format as a button input devices using escape characters, for example, "engine$0keyboard$1code$00"
#      - "modifier": sub-devices as a modifier.
#      - "modifier_scale": a float number representing the applied modifier scale to the analog input.
#          Must be in range of 0.0-1.0. Defaults to 0.5
#  - "sdl" for joystick input using SDL. Required parameters:
#      - "guid": SDL identification GUID of the joystick
#      - "port": the index of the joystick to bind
#      - "axis_x": the index of the axis to bind as x-axis (default to 0)
#      - "axis_y": the index of the axis to bind as y-axis (default to 1)
lstick=
rstick=

# for motion input, the following devices are available:
#  - "keyboard" (default) for emulating random motion input from buttons. Required parameters:
#      - "code": the code of the key to bind
#  - "sdl" for motion input using SDL. Required parameters:
#      - "guid": SDL identification GUID of the joystick
#      - "port": the index of the joystick to bind
#      - "motion": the index of the motion sensor to bind
#  - "cemuhookudp" for motion input using Cemu Hook protocol. Required parameters:
#      - "guid": the IP address of the cemu hook server encoded to a hex string. for example 192.168.0.1 = "c0a80001"
#      - "port": the port of the cemu hook server
#      - "pad": the index of the joystick
#      - "motion": the index of the motion sensor of the joystick to bind
motionleft=
motionright=

[ControlsGeneral]
# To use the debug_pad, prepend `debug_pad_` before each button setting above.
# i.e. debug_pad_button_a=

# Enable debug pad inputs to the guest
# 0 (default): Disabled, 1: Enabled
debug_pad_enabled =

# Enable sdl raw input. Allows to configure up to 8 xinput controllers.
# 0 (default): Disabled, 1: Enabled
enable_raw_input =

# Enable sudachi joycon driver instead of SDL drive.
# 0: Disabled, 1 (default): Enabled
enable_joycon_driver =

# Emulates an analog input from buttons. Allowing to dial any angle.
# 0 (default): Disabled, 1: Enabled
emulate_analog_keyboard =

# Whether to enable or disable vibration
# 0: Disabled, 1 (default): Enabled
vibration_enabled=

# Whether to enable or disable accurate vibrations
# 0 (default): Disabled, 1: Enabled
enable_accurate_vibrations=

# Enables controller motion inputs
# 0: Disabled, 1 (default): Enabled
motion_enabled =

# Defines the udp device's touch screen coordinate system for cemuhookudp devices
#  - "min_x", "min_y", "max_x", "max_y"
touch_device=

# for mapping buttons to touch inputs.
#touch_from_button_map=1
#touch_from_button_maps_0_name=default
#touch_from_button_maps_0_count=2
#touch_from_button_maps_0_bind_0=foo
#touch_from_button_maps_0_bind_1=bar
# etc.

# List of Cemuhook UDP servers, delimited by ','.
# Default: 127.0.0.1:26760
# Example: 127.0.0.1:26760,123.4.5.67:26761
udp_input_servers =

# Enable controlling an axis via a mouse input.
# 0 (default): Off, 1: On
mouse_panning =

# Set mouse panning horizontal sensitivity.
# Default: 50.0
mouse_panning_x_sensitivity =

# Set mouse panning vertical sensitivity.
# Default: 50.0
mouse_panning_y_sensitivity =

# Set mouse panning deadzone horizontal counterweight.
# Default: 0.0
mouse_panning_deadzone_x_counterweight =

# Set mouse panning deadzone vertical counterweight.
# Default: 0.0
mouse_panning_deadzone_y_counterweight =

# Set mouse panning stick decay strength.
# Default: 22.0
mouse_panning_decay_strength =

# Set mouse panning stick minimum decay.
# Default: 5.0
mouse_panning_minimum_decay =

# Emulate an analog control stick from keyboard inputs.
# 0 (default): Disabled, 1: Enabled
emulate_analog_keyboard =

# Enable mouse inputs to the guest
# 0 (default): Disabled, 1: Enabled
mouse_enabled =

# Enable keyboard inputs to the guest
# 0 (default): Disabled, 1: Enabled
keyboard_enabled =

)"
    R"(
[Core]
# Whether to use multi-core for CPU emulation
# 0: Disabled, 1 (default): Enabled
use_multi_core =

# Enable unsafe extended guest system memory layout (8GB DRAM)
# 0 (default): Disabled, 1: Enabled
use_unsafe_extended_memory_layout =

[Cpu]
# Adjusts various optimizations.
# Auto-select mode enables choice unsafe optimizations.
# Accurate enables only safe optimizations.
# Unsafe allows any unsafe optimizations.
# 0 (default): Auto-select, 1: Accurate, 2: Enable unsafe optimizations
cpu_accuracy =

# Allow disabling safe optimizations.
# 0 (default): Disabled, 1: Enabled
cpu_debug_mode =

# Enable inline page tables optimization (faster guest memory access)
# 0: Disabled, 1 (default): Enabled
cpuopt_page_tables =

# Enable block linking CPU optimization (reduce block dispatcher use during predictable jumps)
# 0: Disabled, 1 (default): Enabled
cpuopt_block_linking =

# Enable return stack buffer CPU optimization (reduce block dispatcher use during predictable returns)
# 0: Disabled, 1 (default): Enabled
cpuopt_return_stack_buffer =

# Enable fast dispatcher CPU optimization (use a two-tiered dispatcher architecture)
# 0: Disabled, 1 (default): Enabled
cpuopt_fast_dispatcher =

# Enable context elimination CPU Optimization (reduce host memory use for guest context)
# 0: Disabled, 1 (default): Enabled
cpuopt_context_elimination =

# Enable constant propagation CPU optimization (basic IR optimization)
# 0: Disabled, 1 (default): Enabled
cpuopt_const_prop =

# Enable miscellaneous CPU optimizations (basic IR optimization)
# 0: Disabled, 1 (default): Enabled
cpuopt_misc_ir =

# Enable reduction of memory misalignment checks (reduce memory fallbacks for misaligned access)
# 0: Disabled, 1 (default): Enabled
cpuopt_reduce_misalign_checks =

# Enable Host MMU Emulation (faster guest memory access)
# 0: Disabled, 1 (default): Enabled
cpuopt_fastmem =

# Enable Host MMU Emulation for exclusive memory instructions (faster guest memory access)
# 0: Disabled, 1 (default): Enabled
cpuopt_fastmem_exclusives =

# Enable fallback on failure of fastmem of exclusive memory instructions (faster guest memory access)
# 0: Disabled, 1 (default): Enabled
cpuopt_recompile_exclusives =

# Enable optimization to ignore invalid memory accesses (faster guest memory access)
# 0: Disabled, 1 (default): Enabled
cpuopt_ignore_memory_aborts =

# Enable unfuse FMA (improve performance on CPUs without FMA)
# Only enabled if cpu_accuracy is set to Unsafe. Automatically chosen with cpu_accuracy = Auto-select.
# 0: Disabled, 1 (default): Enabled
cpuopt_unsafe_unfuse_fma =

# Enable faster FRSQRTE and FRECPE
# Only enabled if cpu_accuracy is set to Unsafe.
# 0: Disabled, 1 (default): Enabled
cpuopt_unsafe_reduce_fp_error =

# Enable faster ASIMD instructions (32 bits only)
# Only enabled if cpu_accuracy is set to Unsafe. Automatically chosen with cpu_accuracy = Auto-select.
# 0: Disabled, 1 (default): Enabled
cpuopt_unsafe_ignore_standard_fpcr =

# Enable inaccurate NaN handling
# Only enabled if cpu_accuracy is set to Unsafe. Automatically chosen with cpu_accuracy = Auto-select.
# 0: Disabled, 1 (default): Enabled
cpuopt_unsafe_inaccurate_nan =

# Disable address space checks (64 bits only)
# Only enabled if cpu_accuracy is set to Unsafe. Automatically chosen with cpu_accuracy = Auto-select.
# 0: Disabled, 1 (default): Enabled
cpuopt_unsafe_fastmem_check =

# Enable faster exclusive instructions
# Only enabled if cpu_accuracy is set to Unsafe. Automatically chosen with cpu_accuracy = Auto-select.
# 0: Disabled, 1 (default): Enabled
cpuopt_unsafe_ignore_global_monitor =

)"
    R"(
[Renderer]
# Which backend API to use.
# 0: OpenGL, 1 (default): Vulkan
backend =

# Whether to enable asynchronous presentation (Vulkan only)
# 0 (default): Off, 1: On
async_presentation =

# Enable graphics API debugging mode.
# 0 (default): Disabled, 1: Enabled
debug =

# Enable shader feedback.
# 0 (default): Disabled, 1: Enabled
renderer_shader_feedback =

# Enable Nsight Aftermath crash dumps
# 0 (default): Disabled, 1: Enabled
nsight_aftermath =

# Disable shader loop safety checks, executing the shader without loop logic changes
# 0 (default): Disabled, 1: Enabled
disable_shader_loop_safety_checks =

# Which Vulkan physical device to use (defaults to 0)
vulkan_device =

# 0: 0.5x (360p/540p) [EXPERIMENTAL]
# 1: 0.75x (540p/810p) [EXPERIMENTAL]
# 2 (default): 1x (720p/1080p)
# 3: 1.5x (1080p/1620p) [EXPERIMENTAL]
# 4: 2x (1440p/2160p)
# 5: 3x (2160p/3240p)
# 6: 4x (2880p/4320p)
# 7: 5x (3600p/5400p)
# 8: 6x (4320p/6480p)
# 9: 7x (5040p/7560p)
# 10: 8x (5760/8640p)
resolution_setup =

# Pixel filter to use when up- or down-sampling rendered frames.
# 0: Nearest Neighbor
# 1 (default): Bilinear
# 2: Bicubic
# 3: Gaussian
# 4: ScaleForce
# 5: AMD FidelityFX™️ Super Resolution
scaling_filter =

# Anti-Aliasing (AA)
# 0 (default): None, 1: FXAA, 2: SMAA
anti_aliasing =

# Whether to use fullscreen or borderless window mode
# 0 (Windows default): Borderless window, 1 (All other default): Exclusive fullscreen
fullscreen_mode =

# Aspect ratio
# 0: Default (16:9), 1: Force 4:3, 2: Force 21:9, 3: Force 16:10, 4: Stretch to Window
aspect_ratio =

# Anisotropic filtering
# 0: Default, 1: 2x, 2: 4x, 3: 8x, 4: 16x
max_anisotropy =

# Whether to enable VSync or not.
# OpenGL: Values other than 0 enable VSync
# Vulkan: FIFO is selected if the requested mode is not supported by the driver.
# FIFO (VSync) does not drop frames or exhibit tearing but is limited by the screen refresh rate.
# FIFO Relaxed is similar to FIFO but allows tearing as it recovers from a slow down.
# Mailbox can have lower latency than FIFO and does not tear but may drop frames.
# Immediate (no synchronization) just presents whatever is available and can exhibit tearing.
# 0: Immediate (Off), 1: Mailbox, 2 (Default): FIFO (On), 3: FIFO Relaxed
use_vsync =

# Selects the OpenGL shader backend. NV_gpu_program5 is required for GLASM. If NV_gpu_program5 is
# not available and GLASM is selected, GLSL will be used.
# 0: GLSL, 1 (default): GLASM, 2: SPIR-V
shader_backend =

# Uses reactive flushing instead of predictive flushing. Allowing a more accurate syncing of memory.
# 0: Off, 1 (default): On
use_reactive_flushing =

# Whether to allow asynchronous shader building.
# 0 (default): Off, 1: On
use_asynchronous_shaders =

# NVDEC emulation.
# 0: Disabled, 1: CPU Decoding, 2 (default): GPU Decoding
nvdec_emulation =

# Accelerate ASTC texture decoding.
# 0: Off, 1 (default): On
accelerate_astc =

# Decode ASTC textures asynchronously.
# 0 (default): Off, 1: On
async_astc =

# Recompress ASTC textures to a different format.
# 0 (default): Uncompressed, 1: BC1 (Low quality), 2: BC3: (Medium quality)
async_astc =

# Turns on the speed limiter, which will limit the emulation speed to the desired speed limit value
# 0: Off, 1: On (default)
use_speed_limit =

# Limits the speed of the game to run no faster than this value as a percentage of target speed
# 1 - 9999: Speed limit as a percentage of target game speed. 100 (default)
speed_limit =

# Whether to use disk based shader cache
# 0: Off, 1 (default): On
use_disk_shader_cache =

# Which gpu accuracy level to use
# 0: Normal, 1 (default): High, 2: Extreme (Very slow)
gpu_accuracy =

# Whether to use asynchronous GPU emulation
# 0 : Off (slow), 1 (default): On (fast)
use_asynchronous_gpu_emulation =

# Inform the guest that GPU operations completed more quickly than they did.
# 0: Off, 1 (default): On
use_fast_gpu_time =

# Whether to use garbage collection or not for GPU caches.
# 0 (default): Off, 1: On
use_caches_gc =

# The clear color for the renderer. What shows up on the sides of the bottom screen.
# Must be in range of 0-255. Defaults to 0 for all.
bg_red =
bg_blue =
bg_green =

)"
    R"(
[Audio]
# Which audio output engine to use.
# auto (default): Auto-select
# cubeb: Cubeb audio engine (if available)
# sdl3: SDL3 audio engine (if available)
# null: No audio output
output_engine =

# Which audio device to use.
# auto (default): Auto-select
output_device =

# Output volume.
# 100 (default): 100%, 0; mute
volume =

[Data Storage]
# Whether to create a virtual SD card.
# 1 (default): Yes, 0: No
use_virtual_sd =

# Whether or not to enable gamecard emulation
# 1: Yes, 0 (default): No
gamecard_inserted =

# Whether or not the gamecard should be emulated as the current game
# If 'gamecard_inserted' is 0 this setting is irrelevant
# 1: Yes, 0 (default): No
gamecard_current_game =

# Path to an XCI file to use as the gamecard
# If 'gamecard_inserted' is 0 this setting is irrelevant
# If 'gamecard_current_game' is 1 this setting is irrelevant
gamecard_path =

[System]
# Whether the system is docked
# 1 (default): Yes, 0: No
use_docked_mode =

# Sets the seed for the RNG generator built into the switch
# rng_seed will be ignored and randomly generated if rng_seed_enabled is false
rng_seed_enabled =
rng_seed =

# Sets the current time (in seconds since 12:00 AM Jan 1, 1970) that will be used by the time service
# This will auto-increment, with the time set being the time the game is started
# This override will only occur if custom_rtc_enabled is true, otherwise the current time is used
custom_rtc_enabled =
custom_rtc =

# Sets the systems language index
# 0: Japanese, 1: English (default), 2: French, 3: German, 4: Italian, 5: Spanish, 6: Chinese,
# 7: Korean, 8: Dutch, 9: Portuguese, 10: Russian, 11: Taiwanese, 12: British English, 13: Canadian French,
# 14: Latin American Spanish, 15: Simplified Chinese, 16: Traditional Chinese, 17: Brazilian Portuguese
language_index =

# The system region that sudachi will use during emulation
# -1: Auto-select (default), 0: Japan, 1: USA, 2: Europe, 3: Australia, 4: China, 5: Korea, 6: Taiwan
region_index =

# The system time zone that sudachi will use during emulation
# 0: Auto-select (default), 1: Default (system archive value), Others: Index for specified time zone
time_zone_index =

# Sets the sound output mode.
# 0: Mono, 1 (default): Stereo, 2: Surround
sound_index =

[Miscellaneous]
# A filter which removes logs below a certain logging level.
# Examples: *:Debug Kernel.SVC:Trace Service.*:Critical
log_filter = *:Trace

# Use developer keys
# 0 (default): Disabled, 1: Enabled
use_dev_keys =

[Debugging]
# Record frame time data, can be found in the log directory. Boolean value
record_frame_times =
# Determines whether or not sudachi will dump the ExeFS of all games it attempts to load while loading them
dump_exefs=false
# Determines whether or not sudachi will dump all NSOs it attempts to load while loading them
dump_nso=false
# Determines whether or not sudachi will save the filesystem access log.
enable_fs_access_log=false
# Enables verbose reporting services
reporting_services =
# Determines whether or not sudachi will report to the game that the emulated console is in Kiosk Mode
# false: Retail/Normal Mode (default), true: Kiosk Mode
quest_flag =
# Determines whether debug asserts should be enabled, which will throw an exception on asserts.
# false: Disabled (default), true: Enabled
use_debug_asserts =
# Determines whether unimplemented HLE service calls should be automatically stubbed.
# false: Disabled (default), true: Enabled
use_auto_stub =
# Enables/Disables the macro JIT compiler
disable_macro_jit=false
# Determines whether to enable the GDB stub and wait for the debugger to attach before running.
# false: Disabled (default), true: Enabled
use_gdbstub=false
# The port to use for the GDB server, if it is enabled.
gdbstub_port=6543

[WebService]
# Whether or not to enable telemetry
# 0: No, 1 (default): Yes
enable_telemetry =
# URL for Web API
web_api_url = https://api.sudachi-emu.org
# Username and token for sudachi Web Service
# See https://profile.sudachi-emu.org/ for more info
sudachi_username =
sudachi_token =

[Network]
# Name of the network interface device to use with sudachi LAN play.
# e.g. On *nix: 'enp7s0', 'wlp6s0u1u3u3', 'lo'
# e.g. On Windows: 'Ethernet', 'Wi-Fi'
network_interface =

[AddOns]
# Used to disable add-ons
# List of title IDs of games that will have add-ons disabled (separated by '|'):
title_ids =
# For each title ID, have a key/value pair called `disabled_<title_id>` equal to the names of the add-ons to disable (sep. by '|')
# e.x. disabled_0100000000010000 = Update|DLC <- disables Updates and DLC on Super Mario Odyssey
)";
} // namespace DefaultINI
