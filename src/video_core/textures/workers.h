// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "common/thread_worker.h"

namespace Tegra::Texture {

Common::ThreadWorker& GetThreadWorkers();

}
