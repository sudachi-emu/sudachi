// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "common/common_types.h"

namespace Core {
class System;
}

namespace Kernel {
class KEvent;
}

namespace Service {

class Mutex {
public:
    explicit Mutex(Core::System& system);
    ~Mutex();

    void lock();
    void unlock();

private:
    Core::System& m_system;
    Kernel::KEvent* m_event{};
};

} // namespace Service
