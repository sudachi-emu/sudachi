// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "common/common_types.h"
#include "hid_core/hid_types.h"
#include "hid_core/resources/controller_base.h"
#include "hid_core/resources/ring_lifo.h"

namespace Core::HID {
class EmulatedController;
} // namespace Core::HID

namespace Service::HID {
class NPad;

class SixAxis final : public ControllerBase {
public:
    explicit SixAxis(Core::HID::HIDCore& hid_core_, std::shared_ptr<NPad> npad_);
    ~SixAxis() override;

    // Called when the controller is initialized
    void OnInit() override;

    // When the controller is released
    void OnRelease() override;

    // When the controller is requesting an update for the shared memory
    void OnUpdate(const Core::Timing::CoreTiming& core_timing) override;

    Result SetGyroscopeZeroDriftMode(const Core::HID::SixAxisSensorHandle& sixaxis_handle,
                                     Core::HID::GyroscopeZeroDriftMode drift_mode);
    Result GetGyroscopeZeroDriftMode(const Core::HID::SixAxisSensorHandle& sixaxis_handle,
                                     Core::HID::GyroscopeZeroDriftMode& drift_mode) const;
    Result IsSixAxisSensorAtRest(const Core::HID::SixAxisSensorHandle& sixaxis_handle,
                                 bool& is_at_rest) const;
    Result EnableSixAxisSensorUnalteredPassthrough(
        const Core::HID::SixAxisSensorHandle& sixaxis_handle, bool is_enabled);
    Result IsSixAxisSensorUnalteredPassthroughEnabled(
        const Core::HID::SixAxisSensorHandle& sixaxis_handle, bool& is_enabled) const;
    Result LoadSixAxisSensorCalibrationParameter(
        const Core::HID::SixAxisSensorHandle& sixaxis_handle,
        Core::HID::SixAxisSensorCalibrationParameter& calibration) const;
    Result GetSixAxisSensorIcInformation(
        const Core::HID::SixAxisSensorHandle& sixaxis_handle,
        Core::HID::SixAxisSensorIcInformation& ic_information) const;
    Result SetSixAxisEnabled(const Core::HID::SixAxisSensorHandle& sixaxis_handle,
                             bool sixaxis_status);
    Result IsSixAxisSensorFusionEnabled(const Core::HID::SixAxisSensorHandle& sixaxis_handle,
                                        bool& is_fusion_enabled) const;
    Result SetSixAxisFusionEnabled(const Core::HID::SixAxisSensorHandle& sixaxis_handle,
                                   bool is_fusion_enabled);
    Result SetSixAxisFusionParameters(
        const Core::HID::SixAxisSensorHandle& sixaxis_handle,
        Core::HID::SixAxisSensorFusionParameters sixaxis_fusion_parameters);
    Result GetSixAxisFusionParameters(const Core::HID::SixAxisSensorHandle& sixaxis_handle,
                                      Core::HID::SixAxisSensorFusionParameters& parameters) const;

private:
    static constexpr std::size_t NPAD_COUNT = 10;

    struct SixaxisParameters {
        bool is_fusion_enabled{true};
        bool unaltered_passthrough{false};
        Core::HID::SixAxisSensorFusionParameters fusion{};
        Core::HID::SixAxisSensorCalibrationParameter calibration{};
        Core::HID::SixAxisSensorIcInformation ic_information{};
        Core::HID::GyroscopeZeroDriftMode gyroscope_zero_drift_mode{
            Core::HID::GyroscopeZeroDriftMode::Standard};
    };

    struct NpadControllerData {
        Core::HID::EmulatedController* device = nullptr;

        // Motion parameters
        bool sixaxis_at_rest{true};
        bool sixaxis_sensor_enabled{true};
        SixaxisParameters sixaxis_fullkey{};
        SixaxisParameters sixaxis_handheld{};
        SixaxisParameters sixaxis_dual_left{};
        SixaxisParameters sixaxis_dual_right{};
        SixaxisParameters sixaxis_left{};
        SixaxisParameters sixaxis_right{};
        SixaxisParameters sixaxis_unknown{};

        // Current pad state
        Core::HID::SixAxisSensorState sixaxis_fullkey_state{};
        Core::HID::SixAxisSensorState sixaxis_handheld_state{};
        Core::HID::SixAxisSensorState sixaxis_dual_left_state{};
        Core::HID::SixAxisSensorState sixaxis_dual_right_state{};
        Core::HID::SixAxisSensorState sixaxis_left_lifo_state{};
        Core::HID::SixAxisSensorState sixaxis_right_lifo_state{};
        int callback_key{};
    };

    SixaxisParameters& GetSixaxisState(const Core::HID::SixAxisSensorHandle& device_handle);
    const SixaxisParameters& GetSixaxisState(
        const Core::HID::SixAxisSensorHandle& device_handle) const;

    NpadControllerData& GetControllerFromHandle(
        const Core::HID::SixAxisSensorHandle& device_handle);
    const NpadControllerData& GetControllerFromHandle(
        const Core::HID::SixAxisSensorHandle& device_handle) const;
    NpadControllerData& GetControllerFromNpadIdType(Core::HID::NpadIdType npad_id);
    const NpadControllerData& GetControllerFromNpadIdType(Core::HID::NpadIdType npad_id) const;

    std::shared_ptr<NPad> npad;
    std::array<NpadControllerData, NPAD_COUNT> controller_data{};
};
} // namespace Service::HID
