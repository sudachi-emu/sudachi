// SPDX-FileCopyrightText: Copyright 2024 sudachi Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "core/hle/service/am/am_types.h"
#include "core/hle/service/cmif_types.h"
#include "core/hle/service/service.h"

namespace Service::AM {

struct Applet;
class ILibraryAppletAccessor;
class IStorage;

class IProcessWindingController final : public ServiceFramework<IProcessWindingController> {
public:
    explicit IProcessWindingController(Core::System& system_, std::shared_ptr<Applet> applet_);
    ~IProcessWindingController() override;

private:
    Result GetLaunchReason(Out<AppletProcessLaunchReason> out_launch_reason);
    Result OpenCallingLibraryApplet(
        Out<SharedPointer<ILibraryAppletAccessor>> out_calling_library_applet);
    Result PushContext(SharedPointer<IStorage> storage);
    Result ReserveToStartAndWaitAndUnwindThis(SharedPointer<ILibraryAppletAccessor> library_applet);
    Result WindAndDoReserved();

    const std::shared_ptr<Applet> m_applet;
};

} // namespace Service::AM
