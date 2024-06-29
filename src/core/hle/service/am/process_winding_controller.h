// SPDX-FileCopyrightText: Copyright 2024 sudachi Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "core/hle/service/service.h"

namespace Service::AM {

struct Applet;
class IStorage;

class IProcessWindingController final : public ServiceFramework<IProcessWindingController> {
public:
    explicit IProcessWindingController(Core::System& system_, std::shared_ptr<Applet> applet_);
    ~IProcessWindingController() override;

private:
    void GetLaunchReason(HLERequestContext& ctx);
    void OpenCallingLibraryApplet(HLERequestContext& ctx);
    void PushContext(HLERequestContext& ctx);
    void WindAndDoReserved(HLERequestContext& ctx);
    void ReserveToStartAndWaitAndUnwindThis(HLERequestContext& ctx);

    const std::shared_ptr<Applet> applet;
};

} // namespace Service::AM
