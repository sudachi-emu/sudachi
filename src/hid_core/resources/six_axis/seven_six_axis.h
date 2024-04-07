// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "common/common_types.h"
#include "common/quaternion.h"
#include "common/typed_address.h"
#include "hid_core/resources/controller_base.h"
#include "hid_core/resources/ring_lifo.h"

namespace Core {
class System;
} // namespace Core

namespace Core::HID {
class EmulatedConsole;
} // namespace Core::HID

namespace Service::HID {
class SevenSixAxis final : public ControllerBase {
public:
    explicit SevenSixAxis(Core::System& system_);
    ~SevenSixAxis() override;

    // Called when the controller is initialized
    void OnInit() override;

    // When the controller is released
    void OnRelease() override;

    // When the controller is requesting an update for the shared memory
    void OnUpdate(const Core::Timing::CoreTiming& core_timing) override;

    // Called on InitializeSevenSixAxisSensor
    void SetTransferMemoryAddress(Common::ProcessAddress t_mem);

    // Called on ResetSevenSixAxisSensorTimestamp
    void ResetTimestamp();

private:
    struct SevenSixAxisState {
        INSERT_PADDING_WORDS(2); // unused
        u64 timestamp{};
        u64 sampling_number{};
        u64 unknown{};
        Common::Vec3f accel{};
        Common::Vec3f gyro{};
        Common::Quaternion<f32> quaternion{};
    };
    static_assert(sizeof(SevenSixAxisState) == 0x48, "SevenSixAxisState is an invalid size");

    Lifo<SevenSixAxisState, 0x21> seven_sixaxis_lifo{};
    static_assert(sizeof(seven_sixaxis_lifo) == 0xA70, "SevenSixAxisState is an invalid size");

    u64 last_saved_timestamp{};
    u64 last_global_timestamp{};

    SevenSixAxisState next_seven_sixaxis_state{};
    Common::ProcessAddress transfer_memory{};
    Core::HID::EmulatedConsole* console = nullptr;

    Core::System& system;
};
} // namespace Service::HID
