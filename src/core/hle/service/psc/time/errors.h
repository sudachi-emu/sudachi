// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "core/hle/result.h"

namespace Service::PSC::Time {

constexpr Result ResultPermissionDenied{ErrorModule::Time, 1};
constexpr Result ResultClockMismatch{ErrorModule::Time, 102};
constexpr Result ResultClockUninitialized{ErrorModule::Time, 103};
constexpr Result ResultTimeNotFound{ErrorModule::Time, 200};
constexpr Result ResultOverflow{ErrorModule::Time, 201};
constexpr Result ResultFailed{ErrorModule::Time, 801};
constexpr Result ResultInvalidArgument{ErrorModule::Time, 901};
constexpr Result ResultTimeZoneOutOfRange{ErrorModule::Time, 902};
constexpr Result ResultTimeZoneParseFailed{ErrorModule::Time, 903};
constexpr Result ResultRtcTimeout{ErrorModule::Time, 988};
constexpr Result ResultTimeZoneNotFound{ErrorModule::Time, 989};
constexpr Result ResultNotImplemented{ErrorModule::Time, 990};
constexpr Result ResultAlarmNotRegistered{ErrorModule::Time, 1502};

} // namespace Service::PSC::Time
