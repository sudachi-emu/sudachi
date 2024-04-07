// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/hle/service/glue/time/pm_state_change_handler.h"

namespace Service::Glue::Time {

PmStateChangeHandler::PmStateChangeHandler(AlarmWorker& alarm_worker)
    : m_alarm_worker{alarm_worker} {
    // TODO Initialize IPmModule, dependent on Rtc and Fs
}

} // namespace Service::Glue::Time
