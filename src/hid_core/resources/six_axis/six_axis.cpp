// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "common/common_types.h"
#include "core/core_timing.h"
#include "hid_core/frontend/emulated_controller.h"
#include "hid_core/hid_core.h"
#include "hid_core/hid_result.h"
#include "hid_core/hid_util.h"
#include "hid_core/resources/npad/npad.h"
#include "hid_core/resources/shared_memory_format.h"
#include "hid_core/resources/six_axis/six_axis.h"

namespace Service::HID {

SixAxis::SixAxis(Core::HID::HIDCore& hid_core_, std::shared_ptr<NPad> npad_)
    : ControllerBase{hid_core_}, npad{npad_} {
    for (std::size_t i = 0; i < controller_data.size(); ++i) {
        auto& controller = controller_data[i];
        controller.device = hid_core.GetEmulatedControllerByIndex(i);
    }
}

SixAxis::~SixAxis() = default;

void SixAxis::OnInit() {}
void SixAxis::OnRelease() {}

void SixAxis::OnUpdate(const Core::Timing::CoreTiming& core_timing) {
    std::scoped_lock shared_lock{*shared_mutex};

    for (std::size_t aruid_index = 0; aruid_index < AruidIndexMax; ++aruid_index) {
        const auto* data = applet_resource->GetAruidDataByIndex(aruid_index);

        if (data == nullptr || !data->flag.is_assigned) {
            continue;
        }

        if (!IsControllerActivated()) {
            return;
        }

        for (std::size_t i = 0; i < controller_data.size(); ++i) {
            NpadSharedMemoryEntry& shared_memory = data->shared_memory_format->npad.npad_entry[i];
            auto& controller = controller_data[i];
            const auto& controller_type = controller.device->GetNpadStyleIndex();

            if (!data->flag.enable_six_axis_sensor) {
                continue;
            }

            if (controller_type == Core::HID::NpadStyleIndex::None ||
                !controller.device->IsConnected()) {
                continue;
            }

            const auto& motion_state = controller.device->GetMotions();
            auto& sixaxis_fullkey_state = controller.sixaxis_fullkey_state;
            auto& sixaxis_handheld_state = controller.sixaxis_handheld_state;
            auto& sixaxis_dual_left_state = controller.sixaxis_dual_left_state;
            auto& sixaxis_dual_right_state = controller.sixaxis_dual_right_state;
            auto& sixaxis_left_lifo_state = controller.sixaxis_left_lifo_state;
            auto& sixaxis_right_lifo_state = controller.sixaxis_right_lifo_state;

            auto& sixaxis_fullkey_lifo = shared_memory.internal_state.sixaxis_fullkey_lifo;
            auto& sixaxis_handheld_lifo = shared_memory.internal_state.sixaxis_handheld_lifo;
            auto& sixaxis_dual_left_lifo = shared_memory.internal_state.sixaxis_dual_left_lifo;
            auto& sixaxis_dual_right_lifo = shared_memory.internal_state.sixaxis_dual_right_lifo;
            auto& sixaxis_left_lifo = shared_memory.internal_state.sixaxis_left_lifo;
            auto& sixaxis_right_lifo = shared_memory.internal_state.sixaxis_right_lifo;

            // Clear previous state
            sixaxis_fullkey_state = {};
            sixaxis_handheld_state = {};
            sixaxis_dual_left_state = {};
            sixaxis_dual_right_state = {};
            sixaxis_left_lifo_state = {};
            sixaxis_right_lifo_state = {};

            if (controller.sixaxis_sensor_enabled && Settings::values.motion_enabled.GetValue()) {
                controller.sixaxis_at_rest = true;
                for (std::size_t e = 0; e < motion_state.size(); ++e) {
                    controller.sixaxis_at_rest =
                        controller.sixaxis_at_rest && motion_state[e].is_at_rest;
                }
            }

            const auto set_motion_state = [&](Core::HID::SixAxisSensorState& state,
                                              const Core::HID::ControllerMotion& hid_state) {
                using namespace std::literals::chrono_literals;
                static constexpr Core::HID::SixAxisSensorState default_motion_state = {
                    .delta_time = std::chrono::nanoseconds(5ms).count(),
                    .accel = {0, 0, -1.0f},
                    .orientation =
                        {
                            Common::Vec3f{1.0f, 0, 0},
                            Common::Vec3f{0, 1.0f, 0},
                            Common::Vec3f{0, 0, 1.0f},
                        },
                    .attribute = {1},
                };
                if (!controller.sixaxis_sensor_enabled) {
                    state = default_motion_state;
                    return;
                }
                if (!Settings::values.motion_enabled.GetValue()) {
                    state = default_motion_state;
                    return;
                }
                state.attribute.is_connected.Assign(1);
                state.delta_time = std::chrono::nanoseconds(5ms).count();
                state.accel = hid_state.accel;
                state.gyro = hid_state.gyro;
                state.rotation = hid_state.rotation;
                state.orientation = hid_state.orientation;
            };

            switch (controller_type) {
            case Core::HID::NpadStyleIndex::None:
                ASSERT(false);
                break;
            case Core::HID::NpadStyleIndex::Fullkey:
                set_motion_state(sixaxis_fullkey_state, motion_state[0]);
                break;
            case Core::HID::NpadStyleIndex::Handheld:
                set_motion_state(sixaxis_handheld_state, motion_state[0]);
                break;
            case Core::HID::NpadStyleIndex::JoyconDual:
                set_motion_state(sixaxis_dual_left_state, motion_state[0]);
                set_motion_state(sixaxis_dual_right_state, motion_state[1]);
                break;
            case Core::HID::NpadStyleIndex::JoyconLeft:
                set_motion_state(sixaxis_left_lifo_state, motion_state[0]);
                break;
            case Core::HID::NpadStyleIndex::JoyconRight:
                set_motion_state(sixaxis_right_lifo_state, motion_state[1]);
                break;
            case Core::HID::NpadStyleIndex::Pokeball:
                using namespace std::literals::chrono_literals;
                set_motion_state(sixaxis_fullkey_state, motion_state[0]);
                sixaxis_fullkey_state.delta_time = std::chrono::nanoseconds(15ms).count();
                break;
            default:
                break;
            }

            sixaxis_fullkey_state.sampling_number =
                sixaxis_fullkey_lifo.lifo.ReadCurrentEntry().state.sampling_number + 1;
            sixaxis_handheld_state.sampling_number =
                sixaxis_handheld_lifo.lifo.ReadCurrentEntry().state.sampling_number + 1;
            sixaxis_dual_left_state.sampling_number =
                sixaxis_dual_left_lifo.lifo.ReadCurrentEntry().state.sampling_number + 1;
            sixaxis_dual_right_state.sampling_number =
                sixaxis_dual_right_lifo.lifo.ReadCurrentEntry().state.sampling_number + 1;
            sixaxis_left_lifo_state.sampling_number =
                sixaxis_left_lifo.lifo.ReadCurrentEntry().state.sampling_number + 1;
            sixaxis_right_lifo_state.sampling_number =
                sixaxis_right_lifo.lifo.ReadCurrentEntry().state.sampling_number + 1;

            if (IndexToNpadIdType(i) == Core::HID::NpadIdType::Handheld) {
                // This buffer only is updated on handheld on HW
                sixaxis_handheld_lifo.lifo.WriteNextEntry(sixaxis_handheld_state);
            } else {
                // Handheld doesn't update this buffer on HW
                sixaxis_fullkey_lifo.lifo.WriteNextEntry(sixaxis_fullkey_state);
            }

            sixaxis_dual_left_lifo.lifo.WriteNextEntry(sixaxis_dual_left_state);
            sixaxis_dual_right_lifo.lifo.WriteNextEntry(sixaxis_dual_right_state);
            sixaxis_left_lifo.lifo.WriteNextEntry(sixaxis_left_lifo_state);
            sixaxis_right_lifo.lifo.WriteNextEntry(sixaxis_right_lifo_state);
        }
    }
}

Result SixAxis::SetGyroscopeZeroDriftMode(const Core::HID::SixAxisSensorHandle& sixaxis_handle,
                                          Core::HID::GyroscopeZeroDriftMode drift_mode) {
    const auto is_valid = IsSixaxisHandleValid(sixaxis_handle);
    if (is_valid.IsError()) {
        LOG_ERROR(Service_HID, "Invalid handle, error_code={}", is_valid.raw);
        return is_valid;
    }

    auto& sixaxis = GetSixaxisState(sixaxis_handle);
    auto& controller = GetControllerFromHandle(sixaxis_handle);
    sixaxis.gyroscope_zero_drift_mode = drift_mode;
    controller.device->SetGyroscopeZeroDriftMode(drift_mode);

    return ResultSuccess;
}

Result SixAxis::GetGyroscopeZeroDriftMode(const Core::HID::SixAxisSensorHandle& sixaxis_handle,
                                          Core::HID::GyroscopeZeroDriftMode& drift_mode) const {
    const auto is_valid = IsSixaxisHandleValid(sixaxis_handle);
    if (is_valid.IsError()) {
        LOG_ERROR(Service_HID, "Invalid handle, error_code={}", is_valid.raw);
        return is_valid;
    }

    const auto& sixaxis = GetSixaxisState(sixaxis_handle);
    drift_mode = sixaxis.gyroscope_zero_drift_mode;

    return ResultSuccess;
}

Result SixAxis::IsSixAxisSensorAtRest(const Core::HID::SixAxisSensorHandle& sixaxis_handle,
                                      bool& is_at_rest) const {
    const auto is_valid = IsSixaxisHandleValid(sixaxis_handle);
    if (is_valid.IsError()) {
        LOG_ERROR(Service_HID, "Invalid handle, error_code={}", is_valid.raw);
        return is_valid;
    }

    const auto& controller = GetControllerFromHandle(sixaxis_handle);
    is_at_rest = controller.sixaxis_at_rest;
    return ResultSuccess;
}

Result SixAxis::LoadSixAxisSensorCalibrationParameter(
    const Core::HID::SixAxisSensorHandle& sixaxis_handle,
    Core::HID::SixAxisSensorCalibrationParameter& calibration) const {
    const auto is_valid = IsSixaxisHandleValid(sixaxis_handle);
    if (is_valid.IsError()) {
        LOG_ERROR(Service_HID, "Invalid handle, error_code={}", is_valid.raw);
        return is_valid;
    }

    // TODO: Request this data to the controller. On error return 0xd8ca
    const auto& sixaxis = GetSixaxisState(sixaxis_handle);
    calibration = sixaxis.calibration;
    return ResultSuccess;
}

Result SixAxis::GetSixAxisSensorIcInformation(
    const Core::HID::SixAxisSensorHandle& sixaxis_handle,
    Core::HID::SixAxisSensorIcInformation& ic_information) const {
    const auto is_valid = IsSixaxisHandleValid(sixaxis_handle);
    if (is_valid.IsError()) {
        LOG_ERROR(Service_HID, "Invalid handle, error_code={}", is_valid.raw);
        return is_valid;
    }

    // TODO: Request this data to the controller. On error return 0xd8ca
    const auto& sixaxis = GetSixaxisState(sixaxis_handle);
    ic_information = sixaxis.ic_information;
    return ResultSuccess;
}

Result SixAxis::EnableSixAxisSensorUnalteredPassthrough(
    const Core::HID::SixAxisSensorHandle& sixaxis_handle, bool is_enabled) {
    const auto is_valid = IsSixaxisHandleValid(sixaxis_handle);
    if (is_valid.IsError()) {
        LOG_ERROR(Service_HID, "Invalid handle, error_code={}", is_valid.raw);
        return is_valid;
    }

    auto& sixaxis = GetSixaxisState(sixaxis_handle);
    sixaxis.unaltered_passthrough = is_enabled;
    return ResultSuccess;
}

Result SixAxis::IsSixAxisSensorUnalteredPassthroughEnabled(
    const Core::HID::SixAxisSensorHandle& sixaxis_handle, bool& is_enabled) const {
    const auto is_valid = IsSixaxisHandleValid(sixaxis_handle);
    if (is_valid.IsError()) {
        LOG_ERROR(Service_HID, "Invalid handle, error_code={}", is_valid.raw);
        return is_valid;
    }

    const auto& sixaxis = GetSixaxisState(sixaxis_handle);
    is_enabled = sixaxis.unaltered_passthrough;
    return ResultSuccess;
}

Result SixAxis::SetSixAxisEnabled(const Core::HID::SixAxisSensorHandle& sixaxis_handle,
                                  bool sixaxis_status) {
    const auto is_valid = IsSixaxisHandleValid(sixaxis_handle);
    if (is_valid.IsError()) {
        LOG_ERROR(Service_HID, "Invalid handle, error_code={}", is_valid.raw);
        return is_valid;
    }

    auto& controller = GetControllerFromHandle(sixaxis_handle);
    controller.sixaxis_sensor_enabled = sixaxis_status;
    return ResultSuccess;
}

Result SixAxis::IsSixAxisSensorFusionEnabled(const Core::HID::SixAxisSensorHandle& sixaxis_handle,
                                             bool& is_fusion_enabled) const {
    const auto is_valid = IsSixaxisHandleValid(sixaxis_handle);
    if (is_valid.IsError()) {
        LOG_ERROR(Service_HID, "Invalid handle, error_code={}", is_valid.raw);
        return is_valid;
    }

    const auto& sixaxis = GetSixaxisState(sixaxis_handle);
    is_fusion_enabled = sixaxis.is_fusion_enabled;

    return ResultSuccess;
}
Result SixAxis::SetSixAxisFusionEnabled(const Core::HID::SixAxisSensorHandle& sixaxis_handle,
                                        bool is_fusion_enabled) {
    const auto is_valid = IsSixaxisHandleValid(sixaxis_handle);
    if (is_valid.IsError()) {
        LOG_ERROR(Service_HID, "Invalid handle, error_code={}", is_valid.raw);
        return is_valid;
    }

    auto& sixaxis = GetSixaxisState(sixaxis_handle);
    sixaxis.is_fusion_enabled = is_fusion_enabled;

    return ResultSuccess;
}

Result SixAxis::SetSixAxisFusionParameters(
    const Core::HID::SixAxisSensorHandle& sixaxis_handle,
    Core::HID::SixAxisSensorFusionParameters sixaxis_fusion_parameters) {
    const auto is_valid = IsSixaxisHandleValid(sixaxis_handle);
    if (is_valid.IsError()) {
        LOG_ERROR(Service_HID, "Invalid handle, error_code={}", is_valid.raw);
        return is_valid;
    }

    const auto param1 = sixaxis_fusion_parameters.parameter1;
    if (param1 < 0.0f || param1 > 1.0f) {
        return InvalidSixAxisFusionRange;
    }

    auto& sixaxis = GetSixaxisState(sixaxis_handle);
    sixaxis.fusion = sixaxis_fusion_parameters;

    return ResultSuccess;
}

Result SixAxis::GetSixAxisFusionParameters(
    const Core::HID::SixAxisSensorHandle& sixaxis_handle,
    Core::HID::SixAxisSensorFusionParameters& parameters) const {
    const auto is_valid = IsSixaxisHandleValid(sixaxis_handle);
    if (is_valid.IsError()) {
        LOG_ERROR(Service_HID, "Invalid handle, error_code={}", is_valid.raw);
        return is_valid;
    }

    const auto& sixaxis = GetSixaxisState(sixaxis_handle);
    parameters = sixaxis.fusion;

    return ResultSuccess;
}

SixAxis::SixaxisParameters& SixAxis::GetSixaxisState(
    const Core::HID::SixAxisSensorHandle& sixaxis_handle) {
    auto& controller = GetControllerFromHandle(sixaxis_handle);
    switch (sixaxis_handle.npad_type) {
    case Core::HID::NpadStyleIndex::Fullkey:
    case Core::HID::NpadStyleIndex::Pokeball:
        return controller.sixaxis_fullkey;
    case Core::HID::NpadStyleIndex::Handheld:
        return controller.sixaxis_handheld;
    case Core::HID::NpadStyleIndex::JoyconDual:
        if (sixaxis_handle.device_index == Core::HID::DeviceIndex::Left) {
            return controller.sixaxis_dual_left;
        }
        return controller.sixaxis_dual_right;
    case Core::HID::NpadStyleIndex::JoyconLeft:
        return controller.sixaxis_left;
    case Core::HID::NpadStyleIndex::JoyconRight:
        return controller.sixaxis_right;
    default:
        return controller.sixaxis_unknown;
    }
}

const SixAxis::SixaxisParameters& SixAxis::GetSixaxisState(
    const Core::HID::SixAxisSensorHandle& sixaxis_handle) const {
    const auto& controller = GetControllerFromHandle(sixaxis_handle);
    switch (sixaxis_handle.npad_type) {
    case Core::HID::NpadStyleIndex::Fullkey:
    case Core::HID::NpadStyleIndex::Pokeball:
        return controller.sixaxis_fullkey;
    case Core::HID::NpadStyleIndex::Handheld:
        return controller.sixaxis_handheld;
    case Core::HID::NpadStyleIndex::JoyconDual:
        if (sixaxis_handle.device_index == Core::HID::DeviceIndex::Left) {
            return controller.sixaxis_dual_left;
        }
        return controller.sixaxis_dual_right;
    case Core::HID::NpadStyleIndex::JoyconLeft:
        return controller.sixaxis_left;
    case Core::HID::NpadStyleIndex::JoyconRight:
        return controller.sixaxis_right;
    default:
        return controller.sixaxis_unknown;
    }
}

SixAxis::NpadControllerData& SixAxis::GetControllerFromHandle(
    const Core::HID::SixAxisSensorHandle& device_handle) {
    const auto npad_id = static_cast<Core::HID::NpadIdType>(device_handle.npad_id);
    return GetControllerFromNpadIdType(npad_id);
}

const SixAxis::NpadControllerData& SixAxis::GetControllerFromHandle(
    const Core::HID::SixAxisSensorHandle& device_handle) const {
    const auto npad_id = static_cast<Core::HID::NpadIdType>(device_handle.npad_id);
    return GetControllerFromNpadIdType(npad_id);
}

SixAxis::NpadControllerData& SixAxis::GetControllerFromNpadIdType(Core::HID::NpadIdType npad_id) {
    if (!IsNpadIdValid(npad_id)) {
        LOG_ERROR(Service_HID, "Invalid NpadIdType npad_id:{}", npad_id);
        npad_id = Core::HID::NpadIdType::Player1;
    }
    const auto npad_index = NpadIdTypeToIndex(npad_id);
    return controller_data[npad_index];
}

const SixAxis::NpadControllerData& SixAxis::GetControllerFromNpadIdType(
    Core::HID::NpadIdType npad_id) const {
    if (!IsNpadIdValid(npad_id)) {
        LOG_ERROR(Service_HID, "Invalid NpadIdType npad_id:{}", npad_id);
        npad_id = Core::HID::NpadIdType::Player1;
    }
    const auto npad_index = NpadIdTypeToIndex(npad_id);
    return controller_data[npad_index];
}

} // namespace Service::HID
