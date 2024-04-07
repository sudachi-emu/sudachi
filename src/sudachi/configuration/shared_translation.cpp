// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "sudachi/configuration/shared_translation.h"

#include <map>
#include <memory>
#include <tuple>
#include <utility>
#include <QCoreApplication>
#include <QWidget>
#include "common/settings.h"
#include "common/settings_enums.h"
#include "common/settings_setting.h"
#include "common/time_zone.h"
#include "sudachi/uisettings.h"

namespace ConfigurationShared {

std::unique_ptr<TranslationMap> InitializeTranslations(QWidget* parent) {
    std::unique_ptr<TranslationMap> translations = std::make_unique<TranslationMap>();
    const auto& tr = [parent](const char* text) -> QString { return parent->tr(text); };

#define INSERT(SETTINGS, ID, NAME, TOOLTIP)                                                        \
    translations->insert(std::pair{SETTINGS::values.ID.Id(), std::pair{(NAME), (TOOLTIP)}})

    // A setting can be ignored by giving it a blank name

    // Applets
    INSERT(Settings, cabinet_applet_mode, tr("Amiibo editor"), QStringLiteral());
    INSERT(Settings, controller_applet_mode, tr("Controller configuration"), QStringLiteral());
    INSERT(Settings, data_erase_applet_mode, tr("Data erase"), QStringLiteral());
    INSERT(Settings, error_applet_mode, tr("Error"), QStringLiteral());
    INSERT(Settings, net_connect_applet_mode, tr("Net connect"), QStringLiteral());
    INSERT(Settings, player_select_applet_mode, tr("Player select"), QStringLiteral());
    INSERT(Settings, swkbd_applet_mode, tr("Software keyboard"), QStringLiteral());
    INSERT(Settings, mii_edit_applet_mode, tr("Mii Edit"), QStringLiteral());
    INSERT(Settings, web_applet_mode, tr("Online web"), QStringLiteral());
    INSERT(Settings, shop_applet_mode, tr("Shop"), QStringLiteral());
    INSERT(Settings, photo_viewer_applet_mode, tr("Photo viewer"), QStringLiteral());
    INSERT(Settings, offline_web_applet_mode, tr("Offline web"), QStringLiteral());
    INSERT(Settings, login_share_applet_mode, tr("Login share"), QStringLiteral());
    INSERT(Settings, wifi_web_auth_applet_mode, tr("Wifi web auth"), QStringLiteral());
    INSERT(Settings, my_page_applet_mode, tr("My page"), QStringLiteral());

    // Audio
    INSERT(Settings, sink_id, tr("Output Engine:"), QStringLiteral());
    INSERT(Settings, audio_output_device_id, tr("Output Device:"), QStringLiteral());
    INSERT(Settings, audio_input_device_id, tr("Input Device:"), QStringLiteral());
    INSERT(Settings, audio_muted, tr("Mute audio"), QStringLiteral());
    INSERT(Settings, volume, tr("Volume:"), QStringLiteral());
    INSERT(Settings, dump_audio_commands, QStringLiteral(), QStringLiteral());
    INSERT(UISettings, mute_when_in_background, tr("Mute audio when in background"),
           QStringLiteral());

    // Core
    INSERT(
        Settings, use_multi_core, tr("Multicore CPU Emulation"),
        tr("This option increases CPU emulation thread use from 1 to the Switch’s maximum of 4.\n"
           "This is mainly a debug option and shouldn’t be disabled."));
    INSERT(
        Settings, memory_layout_mode, tr("Memory Layout"),
        tr("Increases the amount of emulated RAM from the stock 4GB of the retail Switch to the "
           "developer kit's 8/6GB.\nIt’s doesn’t improve stability or performance and is intended "
           "to let big texture mods fit in emulated RAM.\nEnabling it will increase memory "
           "use. It is not recommended to enable unless a specific game with a texture mod needs "
           "it."));
    INSERT(Settings, use_speed_limit, QStringLiteral(), QStringLiteral());
    INSERT(Settings, speed_limit, tr("Limit Speed Percent"),
           tr("Controls the game's maximum rendering speed, but it’s up to each game if it runs "
              "faster or not.\n200% for a 30 FPS game is 60 FPS, and for a "
              "60 FPS game it will be 120 FPS.\nDisabling it means unlocking the framerate to the "
              "maximum your PC can reach."));

    // Cpu
    INSERT(Settings, cpu_accuracy, tr("Accuracy:"),
           tr("This setting controls the accuracy of the emulated CPU.\nDon't change this unless "
              "you know what you are doing."));
    INSERT(Settings, cpu_backend, tr("Backend:"), QStringLiteral());

    // Cpu Debug

    // Cpu Unsafe
    INSERT(
        Settings, cpuopt_unsafe_unfuse_fma,
        tr("Unfuse FMA (improve performance on CPUs without FMA)"),
        tr("This option improves speed by reducing accuracy of fused-multiply-add instructions on "
           "CPUs without native FMA support."));
    INSERT(
        Settings, cpuopt_unsafe_reduce_fp_error, tr("Faster FRSQRTE and FRECPE"),
        tr("This option improves the speed of some approximate floating-point functions by using "
           "less accurate native approximations."));
    INSERT(Settings, cpuopt_unsafe_ignore_standard_fpcr,
           tr("Faster ASIMD instructions (32 bits only)"),
           tr("This option improves the speed of 32 bits ASIMD floating-point functions by running "
              "with incorrect rounding modes."));
    INSERT(Settings, cpuopt_unsafe_inaccurate_nan, tr("Inaccurate NaN handling"),
           tr("This option improves speed by removing NaN checking.\nPlease note this also reduces "
              "accuracy of certain floating-point instructions."));
    INSERT(Settings, cpuopt_unsafe_fastmem_check, tr("Disable address space checks"),
           tr("This option improves speed by eliminating a safety check before every memory "
              "read/write in guest.\nDisabling it may allow a game to read/write the emulator's "
              "memory."));
    INSERT(
        Settings, cpuopt_unsafe_ignore_global_monitor, tr("Ignore global monitor"),
        tr("This option improves speed by relying only on the semantics of cmpxchg to ensure "
           "safety of exclusive access instructions.\nPlease note this may result in deadlocks and "
           "other race conditions."));

    // Renderer
    INSERT(
        Settings, renderer_backend, tr("API:"),
        tr("Switches between the available graphics APIs.\nVulkan is recommended in most cases."));
    INSERT(Settings, vulkan_device, tr("Device:"),
           tr("This setting selects the GPU to use with the Vulkan backend."));
    INSERT(Settings, shader_backend, tr("Shader Backend:"),
           tr("The shader backend to use for the OpenGL renderer.\nGLSL is the fastest in "
              "performance and the best in rendering accuracy.\n"
              "GLASM is a deprecated NVIDIA-only backend that offers much better shader building "
              "performance at the cost of FPS and rendering accuracy.\n"
              "SPIR-V compiles the fastest, but yields poor results on most GPU drivers."));
    INSERT(Settings, resolution_setup, tr("Resolution:"),
           tr("Forces the game to render at a different resolution.\nHigher resolutions require "
              "much more VRAM and bandwidth.\n"
              "Options lower than 1X can cause rendering issues."));
    INSERT(Settings, scaling_filter, tr("Window Adapting Filter:"), QStringLiteral());
    INSERT(Settings, fsr_sharpening_slider, tr("FSR Sharpness:"),
           tr("Determines how sharpened the image will look while using FSR’s dynamic contrast."));
    INSERT(Settings, anti_aliasing, tr("Anti-Aliasing Method:"),
           tr("The anti-aliasing method to use.\nSMAA offers the best quality.\nFXAA has a "
              "lower performance impact and can produce a better and more stable picture under "
              "very low resolutions."));
    INSERT(Settings, fullscreen_mode, tr("Fullscreen Mode:"),
           tr("The method used to render the window in fullscreen.\nBorderless offers the best "
              "compatibility with the on-screen keyboard that some games request for "
              "input.\nExclusive "
              "fullscreen may offer better performance and better Freesync/Gsync support."));
    INSERT(Settings, aspect_ratio, tr("Aspect Ratio:"),
           tr("Stretches the game to fit the specified aspect ratio.\nSwitch games only support "
              "16:9, so custom game mods are required to get other ratios.\nAlso controls the "
              "aspect ratio of captured screenshots."));
    INSERT(Settings, use_disk_shader_cache, tr("Use disk pipeline cache"),
           tr("Allows saving shaders to storage for faster loading on following game "
              "boots.\nDisabling "
              "it is only intended for debugging."));
    INSERT(
        Settings, use_asynchronous_gpu_emulation, tr("Use asynchronous GPU emulation"),
        tr("Uses an extra CPU thread for rendering.\nThis option should always remain enabled."));
    INSERT(Settings, nvdec_emulation, tr("NVDEC emulation:"),
           tr("Specifies how videos should be decoded.\nIt can either use the CPU or the GPU for "
              "decoding, or perform no decoding at all (black screen on videos).\n"
              "In most cases, GPU decoding provides the best performance."));
    INSERT(Settings, accelerate_astc, tr("ASTC Decoding Method:"),
           tr("This option controls how ASTC textures should be decoded.\n"
              "CPU: Use the CPU for decoding, slowest but safest method.\n"
              "GPU: Use the GPU's compute shaders to decode ASTC textures, recommended for most "
              "games and users.\n"
              "CPU Asynchronously: Use the CPU to decode ASTC textures as they arrive. Completely "
              "eliminates ASTC decoding\nstuttering at the cost of rendering issues while the "
              "texture is being decoded."));
    INSERT(
        Settings, astc_recompression, tr("ASTC Recompression Method:"),
        tr("Almost all desktop and laptop dedicated GPUs lack support for ASTC textures, forcing "
           "the emulator to decompress to an intermediate format any card supports, RGBA8.\n"
           "This option recompresses RGBA8 to either the BC1 or BC3 format, saving VRAM but "
           "negatively affecting image quality."));
    INSERT(Settings, vram_usage_mode, tr("VRAM Usage Mode:"),
           tr("Selects whether the emulator should prefer to conserve memory or make maximum usage "
              "of available video memory for performance. Has no effect on integrated graphics. "
              "Aggressive mode may severely impact the performance of other applications such as "
              "recording software."));
    INSERT(
        Settings, vsync_mode, tr("VSync Mode:"),
        tr("FIFO (VSync) does not drop frames or exhibit tearing but is limited by the screen "
           "refresh rate.\nFIFO Relaxed is similar to FIFO but allows tearing as it recovers from "
           "a slow down.\nMailbox can have lower latency than FIFO and does not tear but may drop "
           "frames.\nImmediate (no synchronization) just presents whatever is available and can "
           "exhibit tearing."));
    INSERT(Settings, bg_red, QStringLiteral(), QStringLiteral());
    INSERT(Settings, bg_green, QStringLiteral(), QStringLiteral());
    INSERT(Settings, bg_blue, QStringLiteral(), QStringLiteral());

    // Renderer (Advanced Graphics)
    INSERT(Settings, async_presentation, tr("Enable asynchronous presentation (Vulkan only)"),
           tr("Slightly improves performance by moving presentation to a separate CPU thread."));
    INSERT(
        Settings, renderer_force_max_clock, tr("Force maximum clocks (Vulkan only)"),
        tr("Runs work in the background while waiting for graphics commands to keep the GPU from "
           "lowering its clock speed."));
    INSERT(Settings, max_anisotropy, tr("Anisotropic Filtering:"),
           tr("Controls the quality of texture rendering at oblique angles.\nIt’s a light setting "
              "and safe to set at 16x on most GPUs."));
    INSERT(Settings, gpu_accuracy, tr("Accuracy Level:"),
           tr("GPU emulation accuracy.\nMost games render fine with Normal, but High is still "
              "required for some.\nParticles tend to only render correctly with High "
              "accuracy.\nExtreme should only be used for debugging.\nThis option can "
              "be changed while playing.\nSome games may require booting on high to render "
              "properly."));
    INSERT(Settings, use_asynchronous_shaders, tr("Use asynchronous shader building (Hack)"),
           tr("Enables asynchronous shader compilation, which may reduce shader stutter.\nThis "
              "feature "
              "is experimental."));
    INSERT(Settings, use_fast_gpu_time, tr("Use Fast GPU Time (Hack)"),
           tr("Enables Fast GPU Time. This option will force most games to run at their highest "
              "native resolution."));
    INSERT(Settings, use_vulkan_driver_pipeline_cache, tr("Use Vulkan pipeline cache"),
           tr("Enables GPU vendor-specific pipeline cache.\nThis option can improve shader loading "
              "time significantly in cases where the Vulkan driver does not store pipeline cache "
              "files internally."));
    INSERT(
        Settings, enable_compute_pipelines, tr("Enable Compute Pipelines (Intel Vulkan Only)"),
        tr("Enable compute pipelines, required by some games.\nThis setting only exists for Intel "
           "proprietary drivers, and may crash if enabled.\nCompute pipelines are always enabled "
           "on all other drivers."));
    INSERT(
        Settings, use_reactive_flushing, tr("Enable Reactive Flushing"),
        tr("Uses reactive flushing instead of predictive flushing, allowing more accurate memory "
           "syncing."));
    INSERT(Settings, use_video_framerate, tr("Sync to framerate of video playback"),
           tr("Run the game at normal speed during video playback, even when the framerate is "
              "unlocked."));
    INSERT(Settings, barrier_feedback_loops, tr("Barrier feedback loops"),
           tr("Improves rendering of transparency effects in specific games."));

    // Renderer (Debug)

    // System
    INSERT(Settings, rng_seed, tr("RNG Seed"),
           tr("Controls the seed of the random number generator.\nMainly used for speedrunning "
              "purposes."));
    INSERT(Settings, rng_seed_enabled, QStringLiteral(), QStringLiteral());
    INSERT(Settings, device_name, tr("Device Name"), tr("The name of the emulated Switch."));
    INSERT(Settings, custom_rtc, tr("Custom RTC Date:"),
           tr("This option allows to change the emulated clock of the Switch.\n"
              "Can be used to manipulate time in games."));
    INSERT(Settings, custom_rtc_enabled, QStringLiteral(), QStringLiteral());
    INSERT(Settings, custom_rtc_offset, QStringLiteral(" "),
           QStringLiteral("The number of seconds from the current unix time"));
    INSERT(Settings, language_index, tr("Language:"),
           tr("Note: this can be overridden when region setting is auto-select"));
    INSERT(Settings, region_index, tr("Region:"), tr("The region of the emulated Switch."));
    INSERT(Settings, time_zone_index, tr("Time Zone:"),
           tr("The time zone of the emulated Switch."));
    INSERT(Settings, sound_index, tr("Sound Output Mode:"), QStringLiteral());
    INSERT(Settings, use_docked_mode, tr("Console Mode:"),
           tr("Selects if the console is emulated in Docked or Handheld mode.\nGames will change "
              "their resolution, details and supported controllers and depending on this setting.\n"
              "Setting to Handheld can help improve performance for low end systems."));
    INSERT(Settings, current_user, QStringLiteral(), QStringLiteral());

    // Controls

    // Data Storage

    // Debugging

    // Debugging Graphics

    // Network

    // Web Service

    // Ui

    // Ui General
    INSERT(UISettings, select_user_on_boot, tr("Prompt for user on game boot"),
           tr("Ask to select a user profile on each boot, useful if multiple people use sudachi on "
              "the same PC."));
    INSERT(UISettings, pause_when_in_background, tr("Pause emulation when in background"),
           tr("This setting pauses sudachi when focusing other windows."));
    INSERT(UISettings, confirm_before_stopping, tr("Confirm before stopping emulation"),
           tr("This setting overrides game prompts asking to confirm stopping the game.\nEnabling "
              "it bypasses such prompts and directly exits the emulation."));
    INSERT(UISettings, hide_mouse, tr("Hide mouse on inactivity"),
           tr("This setting hides the mouse after 2.5s of inactivity."));
    INSERT(UISettings, controller_applet_disabled, tr("Disable controller applet"),
           tr("Forcibly disables the use of the controller applet by guests.\nWhen a guest "
              "attempts to open the controller applet, it is immediately closed."));

    // Linux
    INSERT(Settings, enable_gamemode, tr("Enable Gamemode"), QStringLiteral());

    // Ui Debugging

    // Ui Multiplayer

    // Ui Games list

#undef INSERT

    return translations;
}

std::unique_ptr<ComboboxTranslationMap> ComboboxEnumeration(QWidget* parent) {
    std::unique_ptr<ComboboxTranslationMap> translations =
        std::make_unique<ComboboxTranslationMap>();
    const auto& tr = [&](const char* text, const char* context = "") {
        return parent->tr(text, context);
    };

#define PAIR(ENUM, VALUE, TRANSLATION) {static_cast<u32>(Settings::ENUM::VALUE), (TRANSLATION)}

    // Intentionally skipping VSyncMode to let the UI fill that one out
    translations->insert({Settings::EnumMetadata<Settings::AppletMode>::Index(),
                          {
                              PAIR(AppletMode, HLE, tr("Custom frontend")),
                              PAIR(AppletMode, LLE, tr("Real applet")),
                          }});

    translations->insert({Settings::EnumMetadata<Settings::AstcDecodeMode>::Index(),
                          {
                              PAIR(AstcDecodeMode, Cpu, tr("CPU")),
                              PAIR(AstcDecodeMode, Gpu, tr("GPU")),
                              PAIR(AstcDecodeMode, CpuAsynchronous, tr("CPU Asynchronous")),
                          }});
    translations->insert(
        {Settings::EnumMetadata<Settings::AstcRecompression>::Index(),
         {
             PAIR(AstcRecompression, Uncompressed, tr("Uncompressed (Best quality)")),
             PAIR(AstcRecompression, Bc1, tr("BC1 (Low quality)")),
             PAIR(AstcRecompression, Bc3, tr("BC3 (Medium quality)")),
         }});
    translations->insert({Settings::EnumMetadata<Settings::VramUsageMode>::Index(),
                          {
                              PAIR(VramUsageMode, Conservative, tr("Conservative")),
                              PAIR(VramUsageMode, Aggressive, tr("Aggressive")),
                          }});
    translations->insert({Settings::EnumMetadata<Settings::RendererBackend>::Index(),
                          {
#ifdef HAS_OPENGL
                              PAIR(RendererBackend, OpenGL, tr("OpenGL")),
#endif
                              PAIR(RendererBackend, Vulkan, tr("Vulkan")),
                              PAIR(RendererBackend, Null, tr("Null")),
                          }});
    translations->insert(
        {Settings::EnumMetadata<Settings::ShaderBackend>::Index(),
         {
             PAIR(ShaderBackend, Glsl, tr("GLSL")),
             PAIR(ShaderBackend, Glasm, tr("GLASM (Assembly Shaders, NVIDIA Only)")),
             PAIR(ShaderBackend, SpirV, tr("SPIR-V (Experimental, AMD/Mesa Only)")),
         }});
    translations->insert({Settings::EnumMetadata<Settings::GpuAccuracy>::Index(),
                          {
                              PAIR(GpuAccuracy, Normal, tr("Normal")),
                              PAIR(GpuAccuracy, High, tr("High")),
                              PAIR(GpuAccuracy, Extreme, tr("Extreme")),
                          }});
    translations->insert(
        {Settings::EnumMetadata<Settings::CpuAccuracy>::Index(),
         {
             PAIR(CpuAccuracy, Auto, tr("Auto")),
             PAIR(CpuAccuracy, Accurate, tr("Accurate")),
             PAIR(CpuAccuracy, Unsafe, tr("Unsafe")),
             PAIR(CpuAccuracy, Paranoid, tr("Paranoid (disables most optimizations)")),
         }});
    translations->insert({Settings::EnumMetadata<Settings::CpuBackend>::Index(),
                          {
                              PAIR(CpuBackend, Dynarmic, tr("Dynarmic")),
                              PAIR(CpuBackend, Nce, tr("NCE")),
                          }});
    translations->insert({Settings::EnumMetadata<Settings::FullscreenMode>::Index(),
                          {
                              PAIR(FullscreenMode, Borderless, tr("Borderless Windowed")),
                              PAIR(FullscreenMode, Exclusive, tr("Exclusive Fullscreen")),
                          }});
    translations->insert({Settings::EnumMetadata<Settings::NvdecEmulation>::Index(),
                          {
                              PAIR(NvdecEmulation, Off, tr("No Video Output")),
                              PAIR(NvdecEmulation, Cpu, tr("CPU Video Decoding")),
                              PAIR(NvdecEmulation, Gpu, tr("GPU Video Decoding (Default)")),
                          }});
    translations->insert(
        {Settings::EnumMetadata<Settings::ResolutionSetup>::Index(),
         {
             PAIR(ResolutionSetup, Res1_2X, tr("0.5X (360p/540p) [EXPERIMENTAL]")),
             PAIR(ResolutionSetup, Res3_4X, tr("0.75X (540p/810p) [EXPERIMENTAL]")),
             PAIR(ResolutionSetup, Res1X, tr("1X (720p/1080p)")),
             PAIR(ResolutionSetup, Res3_2X, tr("1.5X (1080p/1620p) [EXPERIMENTAL]")),
             PAIR(ResolutionSetup, Res2X, tr("2X (1440p/2160p)")),
             PAIR(ResolutionSetup, Res3X, tr("3X (2160p/3240p)")),
             PAIR(ResolutionSetup, Res4X, tr("4X (2880p/4320p)")),
             PAIR(ResolutionSetup, Res5X, tr("5X (3600p/5400p)")),
             PAIR(ResolutionSetup, Res6X, tr("6X (4320p/6480p)")),
             PAIR(ResolutionSetup, Res7X, tr("7X (5040p/7560p)")),
             PAIR(ResolutionSetup, Res8X, tr("8X (5760p/8640p)")),
         }});
    translations->insert({Settings::EnumMetadata<Settings::ScalingFilter>::Index(),
                          {
                              PAIR(ScalingFilter, NearestNeighbor, tr("Nearest Neighbor")),
                              PAIR(ScalingFilter, Bilinear, tr("Bilinear")),
                              PAIR(ScalingFilter, Bicubic, tr("Bicubic")),
                              PAIR(ScalingFilter, Gaussian, tr("Gaussian")),
                              PAIR(ScalingFilter, ScaleForce, tr("ScaleForce")),
                              PAIR(ScalingFilter, Fsr, tr("AMD FidelityFX™️ Super Resolution")),
                          }});
    translations->insert({Settings::EnumMetadata<Settings::AntiAliasing>::Index(),
                          {
                              PAIR(AntiAliasing, None, tr("None")),
                              PAIR(AntiAliasing, Fxaa, tr("FXAA")),
                              PAIR(AntiAliasing, Smaa, tr("SMAA")),
                          }});
    translations->insert({Settings::EnumMetadata<Settings::AspectRatio>::Index(),
                          {
                              PAIR(AspectRatio, R16_9, tr("Default (16:9)")),
                              PAIR(AspectRatio, R4_3, tr("Force 4:3")),
                              PAIR(AspectRatio, R21_9, tr("Force 21:9")),
                              PAIR(AspectRatio, R16_10, tr("Force 16:10")),
                              PAIR(AspectRatio, Stretch, tr("Stretch to Window")),
                          }});
    translations->insert({Settings::EnumMetadata<Settings::AnisotropyMode>::Index(),
                          {
                              PAIR(AnisotropyMode, Automatic, tr("Automatic")),
                              PAIR(AnisotropyMode, Default, tr("Default")),
                              PAIR(AnisotropyMode, X2, tr("2x")),
                              PAIR(AnisotropyMode, X4, tr("4x")),
                              PAIR(AnisotropyMode, X8, tr("8x")),
                              PAIR(AnisotropyMode, X16, tr("16x")),
                          }});
    translations->insert(
        {Settings::EnumMetadata<Settings::Language>::Index(),
         {
             PAIR(Language, Japanese, tr("Japanese (日本語)")),
             PAIR(Language, EnglishAmerican, tr("American English")),
             PAIR(Language, French, tr("French (français)")),
             PAIR(Language, German, tr("German (Deutsch)")),
             PAIR(Language, Italian, tr("Italian (italiano)")),
             PAIR(Language, Spanish, tr("Spanish (español)")),
             PAIR(Language, Chinese, tr("Chinese")),
             PAIR(Language, Korean, tr("Korean (한국어)")),
             PAIR(Language, Dutch, tr("Dutch (Nederlands)")),
             PAIR(Language, Portuguese, tr("Portuguese (português)")),
             PAIR(Language, Russian, tr("Russian (Русский)")),
             PAIR(Language, Taiwanese, tr("Taiwanese")),
             PAIR(Language, EnglishBritish, tr("British English")),
             PAIR(Language, FrenchCanadian, tr("Canadian French")),
             PAIR(Language, SpanishLatin, tr("Latin American Spanish")),
             PAIR(Language, ChineseSimplified, tr("Simplified Chinese")),
             PAIR(Language, ChineseTraditional, tr("Traditional Chinese (正體中文)")),
             PAIR(Language, PortugueseBrazilian, tr("Brazilian Portuguese (português do Brasil)")),
         }});
    translations->insert({Settings::EnumMetadata<Settings::Region>::Index(),
                          {
                              PAIR(Region, Japan, tr("Japan")),
                              PAIR(Region, Usa, tr("USA")),
                              PAIR(Region, Europe, tr("Europe")),
                              PAIR(Region, Australia, tr("Australia")),
                              PAIR(Region, China, tr("China")),
                              PAIR(Region, Korea, tr("Korea")),
                              PAIR(Region, Taiwan, tr("Taiwan")),
                          }});
    translations->insert(
        {Settings::EnumMetadata<Settings::TimeZone>::Index(),
         {
             {static_cast<u32>(Settings::TimeZone::Auto),
              tr("Auto (%1)", "Auto select time zone")
                  .arg(QString::fromStdString(
                      Settings::GetTimeZoneString(Settings::TimeZone::Auto)))},
             {static_cast<u32>(Settings::TimeZone::Default),
              tr("Default (%1)", "Default time zone")
                  .arg(QString::fromStdString(Common::TimeZone::GetDefaultTimeZone()))},
             PAIR(TimeZone, Cet, tr("CET")),
             PAIR(TimeZone, Cst6Cdt, tr("CST6CDT")),
             PAIR(TimeZone, Cuba, tr("Cuba")),
             PAIR(TimeZone, Eet, tr("EET")),
             PAIR(TimeZone, Egypt, tr("Egypt")),
             PAIR(TimeZone, Eire, tr("Eire")),
             PAIR(TimeZone, Est, tr("EST")),
             PAIR(TimeZone, Est5Edt, tr("EST5EDT")),
             PAIR(TimeZone, Gb, tr("GB")),
             PAIR(TimeZone, GbEire, tr("GB-Eire")),
             PAIR(TimeZone, Gmt, tr("GMT")),
             PAIR(TimeZone, GmtPlusZero, tr("GMT+0")),
             PAIR(TimeZone, GmtMinusZero, tr("GMT-0")),
             PAIR(TimeZone, GmtZero, tr("GMT0")),
             PAIR(TimeZone, Greenwich, tr("Greenwich")),
             PAIR(TimeZone, Hongkong, tr("Hongkong")),
             PAIR(TimeZone, Hst, tr("HST")),
             PAIR(TimeZone, Iceland, tr("Iceland")),
             PAIR(TimeZone, Iran, tr("Iran")),
             PAIR(TimeZone, Israel, tr("Israel")),
             PAIR(TimeZone, Jamaica, tr("Jamaica")),
             PAIR(TimeZone, Japan, tr("Japan")),
             PAIR(TimeZone, Kwajalein, tr("Kwajalein")),
             PAIR(TimeZone, Libya, tr("Libya")),
             PAIR(TimeZone, Met, tr("MET")),
             PAIR(TimeZone, Mst, tr("MST")),
             PAIR(TimeZone, Mst7Mdt, tr("MST7MDT")),
             PAIR(TimeZone, Navajo, tr("Navajo")),
             PAIR(TimeZone, Nz, tr("NZ")),
             PAIR(TimeZone, NzChat, tr("NZ-CHAT")),
             PAIR(TimeZone, Poland, tr("Poland")),
             PAIR(TimeZone, Portugal, tr("Portugal")),
             PAIR(TimeZone, Prc, tr("PRC")),
             PAIR(TimeZone, Pst8Pdt, tr("PST8PDT")),
             PAIR(TimeZone, Roc, tr("ROC")),
             PAIR(TimeZone, Rok, tr("ROK")),
             PAIR(TimeZone, Singapore, tr("Singapore")),
             PAIR(TimeZone, Turkey, tr("Turkey")),
             PAIR(TimeZone, Uct, tr("UCT")),
             PAIR(TimeZone, Universal, tr("Universal")),
             PAIR(TimeZone, Utc, tr("UTC")),
             PAIR(TimeZone, WSu, tr("W-SU")),
             PAIR(TimeZone, Wet, tr("WET")),
             PAIR(TimeZone, Zulu, tr("Zulu")),
         }});
    translations->insert({Settings::EnumMetadata<Settings::AudioMode>::Index(),
                          {
                              PAIR(AudioMode, Mono, tr("Mono")),
                              PAIR(AudioMode, Stereo, tr("Stereo")),
                              PAIR(AudioMode, Surround, tr("Surround")),
                          }});
    translations->insert({Settings::EnumMetadata<Settings::MemoryLayout>::Index(),
                          {
                              PAIR(MemoryLayout, Memory_4Gb, tr("4GB DRAM (Default)")),
                              PAIR(MemoryLayout, Memory_6Gb, tr("6GB DRAM (Unsafe)")),
                              PAIR(MemoryLayout, Memory_8Gb, tr("8GB DRAM (Unsafe)")),
                          }});
    translations->insert({Settings::EnumMetadata<Settings::ConsoleMode>::Index(),
                          {
                              PAIR(ConsoleMode, Docked, tr("Docked")),
                              PAIR(ConsoleMode, Handheld, tr("Handheld")),
                          }});
    translations->insert(
        {Settings::EnumMetadata<Settings::ConfirmStop>::Index(),
         {
             PAIR(ConfirmStop, Ask_Always, tr("Always ask (Default)")),
             PAIR(ConfirmStop, Ask_Based_On_Game, tr("Only if game specifies not to stop")),
             PAIR(ConfirmStop, Ask_Never, tr("Never ask")),
         }});

#undef PAIR
#undef CTX_PAIR

    return translations;
}
} // namespace ConfigurationShared
