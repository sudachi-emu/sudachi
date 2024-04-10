// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "core/hle/result.h"

namespace Service::Capture {

constexpr Result ResultWorkMemoryError(ErrorModule::Capture, 3);
constexpr Result ResultUnknown5(ErrorModule::Capture, 5);
constexpr Result ResultUnknown6(ErrorModule::Capture, 6);
constexpr Result ResultUnknown7(ErrorModule::Capture, 7);
constexpr Result ResultOutOfRange(ErrorModule::Capture, 8);
constexpr Result ResultInvalidTimestamp(ErrorModule::Capture, 12);
constexpr Result ResultInvalidStorage(ErrorModule::Capture, 13);
constexpr Result ResultInvalidFileContents(ErrorModule::Capture, 14);
constexpr Result ResultIsNotMounted(ErrorModule::Capture, 21);
constexpr Result ResultUnknown22(ErrorModule::Capture, 22);
constexpr Result ResultFileNotFound(ErrorModule::Capture, 23);
constexpr Result ResultInvalidFileData(ErrorModule::Capture, 24);
constexpr Result ResultUnknown25(ErrorModule::Capture, 25);
constexpr Result ResultReadBufferShortage(ErrorModule::Capture, 30);
constexpr Result ResultUnknown810(ErrorModule::Capture, 810);
constexpr Result ResultUnknown1024(ErrorModule::Capture, 1024);
constexpr Result ResultUnknown1202(ErrorModule::Capture, 1202);
constexpr Result ResultUnknown1203(ErrorModule::Capture, 1203);
constexpr Result ResultFileCountLimit(ErrorModule::Capture, 1401);
constexpr Result ResultUnknown1701(ErrorModule::Capture, 1701);
constexpr Result ResultUnknown1801(ErrorModule::Capture, 1801);
constexpr Result ResultUnknown1802(ErrorModule::Capture, 1802);
constexpr Result ResultUnknown1803(ErrorModule::Capture, 1803);
constexpr Result ResultUnknown1804(ErrorModule::Capture, 1804);

} // namespace Service::Capture
