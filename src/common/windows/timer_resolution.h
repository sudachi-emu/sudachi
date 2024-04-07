// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <chrono>

namespace Common::Windows {

/// Returns the minimum (least precise) supported timer resolution in nanoseconds.
std::chrono::nanoseconds GetMinimumTimerResolution();

/// Returns the maximum (most precise) supported timer resolution in nanoseconds.
std::chrono::nanoseconds GetMaximumTimerResolution();

/// Returns the current timer resolution in nanoseconds.
std::chrono::nanoseconds GetCurrentTimerResolution();

/**
 * Sets the current timer resolution.
 *
 * @param timer_resolution Timer resolution in nanoseconds.
 *
 * @returns The current timer resolution.
 */
std::chrono::nanoseconds SetCurrentTimerResolution(std::chrono::nanoseconds timer_resolution);

/**
 * Sets the current timer resolution to the maximum supported timer resolution.
 *
 * @returns The current timer resolution.
 */
std::chrono::nanoseconds SetCurrentTimerResolutionToMaximum();

/// Sleep for one tick of the current timer resolution.
void SleepForOneTick();

} // namespace Common::Windows
