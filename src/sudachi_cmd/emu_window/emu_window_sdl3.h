// SPDX-FileCopyrightText: 2016 Citra Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <utility>

#include "core/frontend/emu_window.h"
#include "core/frontend/graphics_context.h"

struct SDL_Window;

namespace Core {
class System;
}

namespace InputCommon {
class InputSubsystem;
enum class MouseButton;
} // namespace InputCommon

class EmuWindow_SDL3 : public Core::Frontend::EmuWindow {
public:
    explicit EmuWindow_SDL3(InputCommon::InputSubsystem* input_subsystem_, Core::System& system_);
    ~EmuWindow_SDL3();

    /// Whether the window is still open, and a close request hasn't yet been sent
    bool IsOpen() const;

    /// Returns if window is shown (not minimized)
    bool IsShown() const override;

    /// Wait for the next event on the main thread.
    void WaitEvent();

    // Sets the window icon from sudachi.bmp
    void SetWindowIcon();

protected:
    /// Called by WaitEvent when a key is pressed or released.
    void OnKeyEvent(int key, u8 state);

    /// Converts a SDL mouse button into MouseInput mouse button
    InputCommon::MouseButton SDLButtonToMouseButton(u32 button) const;

    /// Translates pixel position to float position
    std::pair<float, float> MouseToTouchPos(s32 touch_x, s32 touch_y) const;

    /// Called by WaitEvent when a mouse button is pressed or released
    void OnMouseButton(u32 button, u8 state, s32 x, s32 y);

    /// Called by WaitEvent when the mouse moves.
    void OnMouseMotion(s32 x, s32 y);

    /// Called by WaitEvent when a finger starts touching the touchscreen
    void OnFingerDown(float x, float y, std::size_t id);

    /// Called by WaitEvent when a finger moves while touching the touchscreen
    void OnFingerMotion(float x, float y, std::size_t id);

    /// Called by WaitEvent when a finger stops touching the touchscreen
    void OnFingerUp();

    /// Called by WaitEvent when any event that may cause the window to be resized occurs
    void OnResize();

    /// Called when users want to hide the mouse cursor
    void ShowCursor(bool show_cursor);

    /// Called when user passes the fullscreen parameter flag
    void Fullscreen();

    /// Called when a configuration change affects the minimal size of the window
    void OnMinimalClientAreaChangeRequest(std::pair<u32, u32> minimal_size) override;

    /// Is the window still open?
    bool is_open = true;

    /// Is the window being shown?
    bool is_shown = true;

    /// Internal SDL3 render window
    SDL_Window* render_window{};

    /// Keeps track of how often to update the title bar during gameplay
    u64 last_time = 0;

    /// Input subsystem to use with this window.
    InputCommon::InputSubsystem* input_subsystem;

    /// sudachi core instance
    Core::System& system;
};

class DummyContext : public Core::Frontend::GraphicsContext {};
