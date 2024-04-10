// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "common/common_types.h"

namespace Service::Glue::Time {
class AlarmWorker;

class PmStateChangeHandler {
public:
    explicit PmStateChangeHandler(AlarmWorker& alarm_worker);

    AlarmWorker& m_alarm_worker;
    s32 m_priority{};
};
} // namespace Service::Glue::Time
