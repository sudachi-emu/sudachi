// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <array>
#include <mutex>
#include <span>

#include "common/common_types.h"
#include "core/hle/result.h"
#include "core/hle/service/kernel_helpers.h"
#include "hid_core/hid_types.h"
#include "hid_core/resources/applet_resource.h"
#include "hid_core/resources/npad/npad_data.h"
#include "hid_core/resources/npad/npad_types.h"

namespace Core {
class System;
}

namespace Kernel {
class KReadableEvent;
}

namespace Service::HID {
struct DataStatusFlag;

struct NpadControllerState {
    bool is_styleset_update_event_initialized{};
    INSERT_PADDING_BYTES(0x7);
    Kernel::KEvent* style_set_update_event{nullptr};
    INSERT_PADDING_BYTES(0x27);
};

struct NpadState {
    DataStatusFlag flag{};
    u64 aruid{};
    NPadData data{};
    std::array<std::array<Core::HID::NpadButton, StyleIndexCount>, MaxSupportedNpadIdTypes>
        button_config;
    std::array<NpadControllerState, MaxSupportedNpadIdTypes> controller_state;
    NpadRevision npad_revision;
};

/// Handles Npad request from HID interfaces
class NPadResource final {
public:
    explicit NPadResource(KernelHelpers::ServiceContext& context);
    ~NPadResource();

    NPadData* GetActiveData();
    u64 GetActiveDataAruid();

    Result RegisterAppletResourceUserId(u64 aruid);
    void UnregisterAppletResourceUserId(u64 aruid);

    void FreeAppletResourceId(u64 aruid);

    Result Activate(u64 aruid);
    Result Activate();
    Result Deactivate();

    void SetAppletResourceUserId(u64 aruid);
    std::size_t GetIndexFromAruid(u64 aruid) const;

    Result ApplyNpadSystemCommonPolicy(u64 aruid, bool is_full_policy);
    Result ClearNpadSystemCommonPolicy(u64 aruid);

    Result SetSupportedNpadStyleSet(u64 aruid, Core::HID::NpadStyleSet style_set);
    Result GetSupportedNpadStyleSet(Core::HID::NpadStyleSet& out_style_Set, u64 aruid) const;
    Result GetMaskedSupportedNpadStyleSet(Core::HID::NpadStyleSet& out_style_set, u64 aruid) const;
    Result GetAvailableStyleset(Core::HID::NpadStyleSet& out_style_set, u64 aruid) const;

    NpadRevision GetNpadRevision(u64 aruid) const;
    void SetNpadRevision(u64 aruid, NpadRevision revision);

    Result IsSupportedNpadStyleSet(bool& is_set, u64 aruid);

    Result SetNpadJoyHoldType(u64 aruid, NpadJoyHoldType hold_type);
    Result GetNpadJoyHoldType(NpadJoyHoldType& hold_type, u64 aruid) const;

    Result SetNpadHandheldActivationMode(u64 aruid, NpadHandheldActivationMode activation_mode);
    Result GetNpadHandheldActivationMode(NpadHandheldActivationMode& activation_mode,
                                         u64 aruid) const;

    Result SetSupportedNpadIdType(u64 aruid,
                                  std::span<const Core::HID::NpadIdType> supported_npad_list);
    bool IsControllerSupported(u64 aruid, Core::HID::NpadStyleIndex style_index) const;

    Result SetLrAssignmentMode(u64 aruid, bool is_enabled);
    Result GetLrAssignmentMode(bool& is_enabled, u64 aruid) const;

    Result SetAssigningSingleOnSlSrPress(u64 aruid, bool is_enabled);
    Result IsAssigningSingleOnSlSrPressEnabled(bool& is_enabled, u64 aruid) const;

    Result AcquireNpadStyleSetUpdateEventHandle(u64 aruid, Kernel::KReadableEvent** out_event,
                                                Core::HID::NpadIdType npad_id);
    Result SignalStyleSetUpdateEvent(u64 aruid, Core::HID::NpadIdType npad_id);

    Result GetHomeProtectionEnabled(bool& is_enabled, u64 aruid,
                                    Core::HID::NpadIdType npad_id) const;
    Result SetHomeProtectionEnabled(u64 aruid, Core::HID::NpadIdType npad_id, bool is_enabled);

    Result SetNpadAnalogStickUseCenterClamp(u64 aruid, bool is_enabled);

    Result SetButtonConfig(u64 aruid, Core::HID::NpadIdType npad_id, std::size_t index,
                           Core::HID::NpadButton button_config);
    Core::HID::NpadButton GetButtonConfig(u64 aruid, Core::HID::NpadIdType npad_id,
                                          std::size_t index, Core::HID::NpadButton mask,
                                          bool is_enabled);
    void ResetButtonConfig();

    Result SetNpadCaptureButtonAssignment(u64 aruid, Core::HID::NpadStyleSet npad_style_set,
                                          Core::HID::NpadButton button_assignment);
    Result ClearNpadCaptureButtonAssignment(u64 aruid);
    std::size_t GetNpadCaptureButtonAssignment(std::span<Core::HID::NpadButton> out_list,
                                               u64 aruid) const;

    Result SetNpadSystemExtStateEnabled(u64 aruid, bool is_enabled);

private:
    NPadData active_data{};
    AruidRegisterList registration_list{};
    std::array<NpadState, AruidIndexMax> state{};
    u64 active_data_aruid{};
    NpadJoyHoldType default_hold_type{};
    s32 ref_counter{};

    KernelHelpers::ServiceContext& service_context;
};
} // namespace Service::HID
