// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <array>
#include <span>

#include "common/common_types.h"
#include "core/hid/hid_types.h"
#include "core/hle/result.h"
#include "core/hle/service/hid/controllers/types/npad_types.h"

namespace Service::HID {

struct NpadStatus {
    union {
        u32 raw{};

        BitField<0, 1, u32> is_supported_styleset_set;
        BitField<1, 1, u32> is_hold_type_set;
        BitField<2, 1, u32> lr_assignment_mode;
        BitField<3, 1, u32> assigning_single_on_sl_sr_press;
        BitField<4, 1, u32> is_full_policy;
        BitField<5, 1, u32> is_policy;
        BitField<6, 1, u32> use_center_clamp;
        BitField<7, 1, u32> system_ext_state;
    };
};
static_assert(sizeof(NpadStatus) == 4, "NpadStatus is an invalid size");

/// Handles Npad request from HID interfaces
class NPadData final {
public:
    explicit NPadData();
    ~NPadData();

    NpadStatus GetNpadStatus() const;

    void SetNpadAnalogStickUseCenterClamp(bool is_enabled);
    bool GetNpadAnalogStickUseCenterClamp() const;

    void SetNpadSystemExtStateEnabled(bool is_enabled);
    bool GetNpadSystemExtState() const;

    Result SetSupportedNpadIdType(std::span<const Core::HID::NpadIdType> list);
    std::size_t GetSupportedNpadIdType(std::span<Core::HID::NpadIdType> out_list) const;
    bool IsNpadIdTypeSupported(Core::HID::NpadIdType npad_id) const;

    void SetNpadSystemCommonPolicy(bool is_full_policy);
    void ClearNpadSystemCommonPolicy();

    void SetNpadJoyHoldType(NpadJoyHoldType hold_type);
    NpadJoyHoldType GetNpadJoyHoldType() const;

    void SetHandheldActivationMode(NpadHandheldActivationMode activation_mode);
    NpadHandheldActivationMode GetHandheldActivationMode() const;

    void SetSupportedNpadStyleSet(Core::HID::NpadStyleSet style_set);
    Core::HID::NpadStyleSet GetSupportedNpadStyleSet() const;
    bool IsNpadStyleIndexSupported(Core::HID::NpadStyleIndex style_index) const;

    void SetLrAssignmentMode(bool is_enabled);
    bool GetLrAssignmentMode() const;

    void SetAssigningSingleOnSlSrPress(bool is_enabled);
    bool GetAssigningSingleOnSlSrPress() const;

    void SetHomeProtectionEnabled(bool is_enabled, Core::HID::NpadIdType npad_id);
    bool GetHomeProtectionEnabled(Core::HID::NpadIdType npad_id) const;

    void SetCaptureButtonAssignment(Core::HID::NpadButton button_assignment,
                                    std::size_t style_index);
    Core::HID::NpadButton GetCaptureButtonAssignment(std::size_t style_index) const;
    std::size_t GetNpadCaptureButtonAssignmentList(std::span<Core::HID::NpadButton> out_list) const;

private:
    NpadStatus status{};
    Core::HID::NpadStyleSet supported_npad_style_set{Core::HID::NpadStyleSet::All};
    NpadJoyHoldType npad_hold_type{NpadJoyHoldType::Vertical};
    NpadHandheldActivationMode handheld_activation_mode{};
    std::array<Core::HID::NpadIdType, MaxSupportedNpadIdTypes> supported_npad_id_types{};
    std::array<Core::HID::NpadButton, StyleIndexCount> npad_button_assignment{};
    std::size_t supported_npad_id_types_count{};
    std::array<bool, MaxSupportedNpadIdTypes> is_unintended_home_button_input_protection{};
};

} // namespace Service::HID
