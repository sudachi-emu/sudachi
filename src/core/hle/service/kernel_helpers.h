// SPDX-FileCopyrightText: Copyright 2021 sudachi Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string>

namespace Core {
class System;
}

namespace Kernel {
class KernelCore;
class KEvent;
class KProcess;
} // namespace Kernel

namespace Service::KernelHelpers {

class ServiceContext {
public:
    ServiceContext(Core::System& system_, std::string name_);
    ~ServiceContext();

    Kernel::KEvent* CreateEvent(std::string&& name);

    void CloseEvent(Kernel::KEvent* event);

    Core::System& m_system;

private:
    Kernel::KernelCore& kernel;
    Kernel::KProcess* process{};
    bool process_created{false};
};

} // namespace Service::KernelHelpers
