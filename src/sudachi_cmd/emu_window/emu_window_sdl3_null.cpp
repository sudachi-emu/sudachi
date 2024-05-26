// SPDX-FileCopyrightText: Copyright 2022 sudachi Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <cstdlib>
#include <memory>
#include <string>

#include <fmt/format.h>

#include "common/logging/log.h"
#include "common/scm_rev.h"
#include "sudachi_cmd/emu_window/emu_window_sdl3_null.h"
#include "video_core/renderer_null/renderer_null.h"

#ifdef SUDACHI_USE_EXTERNAL_SDL3
// Include this before SDL.h to prevent the external from including a dummy
#define USING_GENERATED_CONFIG_H
#include <SDL_config.h>
#endif

#include <SDL.h>

EmuWindow_SDL3_Null::EmuWindow_SDL3_Null(InputCommon::InputSubsystem* input_subsystem_,
                                         Core::System& system_, bool fullscreen)
    : EmuWindow_SDL3{input_subsystem_, system_} {
    const std::string window_title =
        fmt::format("sudachi {} | {}-{} (Vulkan)", Common::g_build_name, Common::g_scm_branch,
                    Common::g_scm_desc);
    render_window = SDL_CreateWindow(window_title.c_str(), Layout::ScreenUndocked::Width,
                                     Layout::ScreenUndocked::Height, SDL_WINDOW_RESIZABLE);

    SetWindowIcon();

    if (fullscreen) {
        Fullscreen();
        ShowCursor(false);
    }

    OnResize();
    OnMinimalClientAreaChangeRequest(GetActiveConfig().min_client_area_size);
    SDL_PumpEvents();
    LOG_INFO(Frontend, "sudachi Version: {} | {}-{} (Null)", Common::g_build_name,
             Common::g_scm_branch, Common::g_scm_desc);
}

EmuWindow_SDL3_Null::~EmuWindow_SDL3_Null() = default;

std::unique_ptr<Core::Frontend::GraphicsContext> EmuWindow_SDL3_Null::CreateSharedContext() const {
    return std::make_unique<DummyContext>();
}
