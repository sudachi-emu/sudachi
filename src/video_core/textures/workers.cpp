// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "video_core/textures/workers.h"

namespace Tegra::Texture {

Common::ThreadWorker& GetThreadWorkers() {
    static Common::ThreadWorker workers{std::max(std::thread::hardware_concurrency(), 2U) / 2,
                                        "ImageTranscode"};

    return workers;
}

} // namespace Tegra::Texture
