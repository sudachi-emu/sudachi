// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <cstring>
#include "common/common_types.h"
#include "core/core.h"
#include "core/core_timing.h"
#include "core/frontend/emu_window.h"
#include "core/hid/emulated_console.h"
#include "core/hid/emulated_devices.h"
#include "core/hid/hid_core.h"
#include "core/hle/service/hid/controllers/seven_six_axis.h"
#include "core/memory.h"

namespace Service::HID {
SevenSixAxis::SevenSixAxis(Core::System& system_)
    : ControllerBase{system_.HIDCore()}, system{system_} {
    console = hid_core.GetEmulatedConsole();
}

SevenSixAxis::~SevenSixAxis() = default;

void SevenSixAxis::OnInit() {}
void SevenSixAxis::OnRelease() {}

void SevenSixAxis::OnUpdate(const Core::Timing::CoreTiming& core_timing) {
    if (!IsControllerActivated() || transfer_memory == 0) {
        seven_sixaxis_lifo.buffer_count = 0;
        seven_sixaxis_lifo.buffer_tail = 0;
        return;
    }

    const auto& last_entry = seven_sixaxis_lifo.ReadCurrentEntry().state;
    next_seven_sixaxis_state.sampling_number = last_entry.sampling_number + 1;

    const auto motion_status = console->GetMotion();
    last_global_timestamp = core_timing.GetGlobalTimeNs().count();

    // This value increments every time the switch goes to sleep
    next_seven_sixaxis_state.unknown = 1;
    next_seven_sixaxis_state.timestamp = last_global_timestamp - last_saved_timestamp;
    next_seven_sixaxis_state.accel = motion_status.accel;
    next_seven_sixaxis_state.gyro = motion_status.gyro;
    next_seven_sixaxis_state.quaternion = {
        {
            motion_status.quaternion.xyz.y,
            motion_status.quaternion.xyz.x,
            -motion_status.quaternion.w,
        },
        -motion_status.quaternion.xyz.z,
    };

    seven_sixaxis_lifo.WriteNextEntry(next_seven_sixaxis_state);
    system.ApplicationMemory().WriteBlock(transfer_memory, &seven_sixaxis_lifo,
                                          sizeof(seven_sixaxis_lifo));
}

void SevenSixAxis::SetTransferMemoryAddress(Common::ProcessAddress t_mem) {
    transfer_memory = t_mem;
}

void SevenSixAxis::ResetTimestamp() {
    last_saved_timestamp = last_global_timestamp;
}

} // namespace Service::HID
