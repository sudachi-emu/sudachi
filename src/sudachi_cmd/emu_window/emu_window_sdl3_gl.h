// SPDX-FileCopyrightText: Copyright 2019 sudachi Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>
#include "core/frontend/emu_window.h"
#include "sudachi_cmd/emu_window/emu_window_sdl3.h"

namespace Core {
class System;
}

namespace InputCommon {
class InputSubsystem;
}

class EmuWindow_SDL3_GL final : public EmuWindow_SDL3 {
public:
    explicit EmuWindow_SDL3_GL(InputCommon::InputSubsystem* input_subsystem_, Core::System& system_,
                               bool fullscreen);
    ~EmuWindow_SDL3_GL();

    std::unique_ptr<Core::Frontend::GraphicsContext> CreateSharedContext() const override;

private:
    /// Whether the GPU and driver supports the OpenGL extension required
    bool SupportsRequiredGLExtensions();

    using SDL_GLContext = void*;

    /// The OpenGL context associated with the window
    SDL_GLContext window_context;

    /// The OpenGL context associated with the core
    std::unique_ptr<Core::Frontend::GraphicsContext> core_context;
};
