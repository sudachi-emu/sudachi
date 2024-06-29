// SPDX-FileCopyrightText: Copyright 2018 sudachi Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "core/hle/service/service.h"

namespace Core {
class System;
}

namespace Service::Friend {

class Module final {
public:
    class Interface : public ServiceFramework<Interface> {
    public:
        explicit Interface(std::shared_ptr<Module> module_, Core::System& system_,
                           const char* name);
        ~Interface() override;

        void CreateFriendService(HLERequestContext& ctx);
        void CreateNotificationService(HLERequestContext& ctx);
        void GetUserPresenceView(HLERequestContext& ctx);

    protected:
        std::shared_ptr<Module> module;
    };
};

void LoopProcess(Core::System& system);

} // namespace Service::Friend
