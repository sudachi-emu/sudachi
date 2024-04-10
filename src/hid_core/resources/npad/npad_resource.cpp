// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/hle/kernel/k_event.h"
#include "core/hle/kernel/k_readable_event.h"
#include "hid_core/hid_result.h"
#include "hid_core/hid_util.h"
#include "hid_core/resources/npad/npad_resource.h"
#include "hid_core/resources/npad/npad_types.h"

namespace Service::HID {

NPadResource::NPadResource(KernelHelpers::ServiceContext& context) : service_context{context} {}

NPadResource::~NPadResource() = default;

Result NPadResource::RegisterAppletResourceUserId(u64 aruid) {
    const auto aruid_index = GetIndexFromAruid(aruid);
    if (aruid_index < AruidIndexMax) {
        return ResultAruidAlreadyRegistered;
    }

    std::size_t data_index = AruidIndexMax;
    for (std::size_t i = 0; i < AruidIndexMax; i++) {
        if (!state[i].flag.is_initialized) {
            data_index = i;
            break;
        }
    }

    if (data_index == AruidIndexMax) {
        return ResultAruidNoAvailableEntries;
    }

    auto& aruid_data = state[data_index];

    aruid_data.aruid = aruid;
    aruid_data.flag.is_initialized.Assign(true);

    data_index = AruidIndexMax;
    for (std::size_t i = 0; i < AruidIndexMax; i++) {
        if (registration_list.flag[i] == RegistrationStatus::Initialized) {
            if (registration_list.aruid[i] != aruid) {
                continue;
            }
            data_index = i;
            break;
        }
        // TODO: Don't Handle pending delete here
        if (registration_list.flag[i] == RegistrationStatus::None ||
            registration_list.flag[i] == RegistrationStatus::PendingDelete) {
            data_index = i;
            break;
        }
    }

    if (data_index == AruidIndexMax) {
        return ResultSuccess;
    }

    registration_list.flag[data_index] = RegistrationStatus::Initialized;
    registration_list.aruid[data_index] = aruid;

    return ResultSuccess;
}

void NPadResource::UnregisterAppletResourceUserId(u64 aruid) {
    const u64 aruid_index = GetIndexFromAruid(aruid);

    FreeAppletResourceId(aruid);
    if (aruid_index < AruidIndexMax) {
        state[aruid_index] = {};
        registration_list.flag[aruid_index] = RegistrationStatus::PendingDelete;
    }

    for (std::size_t i = 0; i < AruidIndexMax; i++) {
        if (registration_list.flag[i] == RegistrationStatus::Initialized) {
            active_data_aruid = registration_list.aruid[i];
        }
    }
}

void NPadResource::FreeAppletResourceId(u64 aruid) {
    const u64 aruid_index = GetIndexFromAruid(aruid);

    if (aruid_index >= AruidIndexMax) {
        return;
    }

    auto& aruid_data = state[aruid_index];

    aruid_data.flag.is_assigned.Assign(false);

    for (auto& controller_state : aruid_data.controller_state) {
        if (!controller_state.is_styleset_update_event_initialized) {
            continue;
        }
        service_context.CloseEvent(controller_state.style_set_update_event);
        controller_state.is_styleset_update_event_initialized = false;
    }
}

Result NPadResource::Activate(u64 aruid) {
    const u64 aruid_index = GetIndexFromAruid(aruid);

    if (aruid_index >= AruidIndexMax) {
        return ResultSuccess;
    }

    auto& state_data = state[aruid_index];

    if (state_data.flag.is_assigned) {
        return ResultAruidAlreadyRegistered;
    }

    state_data.flag.is_assigned.Assign(true);
    state_data.data.ClearNpadSystemCommonPolicy();
    state_data.npad_revision = NpadRevision::Revision0;
    state_data.button_config = {};

    if (active_data_aruid == aruid) {
        default_hold_type = active_data.GetNpadJoyHoldType();
        active_data.SetNpadJoyHoldType(default_hold_type);
    }
    return ResultSuccess;
}

Result NPadResource::Activate() {
    if (ref_counter == std::numeric_limits<s32>::max() - 1) {
        return ResultAppletResourceOverflow;
    }
    if (ref_counter == 0) {
        RegisterAppletResourceUserId(SystemAruid);
        Activate(SystemAruid);
    }
    ref_counter++;
    return ResultSuccess;
}

Result NPadResource::Deactivate() {
    if (ref_counter == 0) {
        return ResultAppletResourceNotInitialized;
    }

    UnregisterAppletResourceUserId(SystemAruid);
    ref_counter--;
    return ResultSuccess;
}

NPadData* NPadResource::GetActiveData() {
    return &active_data;
}

u64 NPadResource::GetActiveDataAruid() {
    return active_data_aruid;
}

void NPadResource::SetAppletResourceUserId(u64 aruid) {
    if (active_data_aruid == aruid) {
        return;
    }

    active_data_aruid = aruid;
    default_hold_type = active_data.GetNpadJoyHoldType();
    const u64 aruid_index = GetIndexFromAruid(aruid);

    if (aruid_index >= AruidIndexMax) {
        return;
    }

    auto& data = state[aruid_index].data;
    if (data.GetNpadStatus().is_policy || data.GetNpadStatus().is_full_policy) {
        data.SetNpadJoyHoldType(default_hold_type);
    }

    active_data = data;
    if (data.GetNpadStatus().is_hold_type_set) {
        active_data.SetNpadJoyHoldType(default_hold_type);
    }
}

std::size_t NPadResource::GetIndexFromAruid(u64 aruid) const {
    for (std::size_t i = 0; i < AruidIndexMax; i++) {
        if (registration_list.flag[i] == RegistrationStatus::Initialized &&
            registration_list.aruid[i] == aruid) {
            return i;
        }
    }
    return AruidIndexMax;
}

Result NPadResource::ApplyNpadSystemCommonPolicy(u64 aruid, bool is_full_policy) {
    const u64 aruid_index = GetIndexFromAruid(aruid);
    if (aruid_index >= AruidIndexMax) {
        return ResultNpadNotConnected;
    }

    auto& data = state[aruid_index].data;
    data.SetNpadSystemCommonPolicy(is_full_policy);
    data.SetNpadJoyHoldType(default_hold_type);
    if (active_data_aruid == aruid) {
        active_data.SetNpadSystemCommonPolicy(is_full_policy);
        active_data.SetNpadJoyHoldType(default_hold_type);
    }
    return ResultSuccess;
}

Result NPadResource::ClearNpadSystemCommonPolicy(u64 aruid) {
    const u64 aruid_index = GetIndexFromAruid(aruid);
    if (aruid_index >= AruidIndexMax) {
        return ResultNpadNotConnected;
    }

    state[aruid_index].data.ClearNpadSystemCommonPolicy();
    if (active_data_aruid == aruid) {
        active_data.ClearNpadSystemCommonPolicy();
    }
    return ResultSuccess;
}

Result NPadResource::SetSupportedNpadStyleSet(u64 aruid, Core::HID::NpadStyleSet style_set) {
    const u64 aruid_index = GetIndexFromAruid(aruid);
    if (aruid_index >= AruidIndexMax) {
        return ResultNpadNotConnected;
    }

    auto& data = state[aruid_index].data;
    data.SetSupportedNpadStyleSet(style_set);
    if (active_data_aruid == aruid) {
        active_data.SetSupportedNpadStyleSet(style_set);
        active_data.SetNpadJoyHoldType(data.GetNpadJoyHoldType());
    }
    return ResultSuccess;
}

Result NPadResource::GetSupportedNpadStyleSet(Core::HID::NpadStyleSet& out_style_Set,
                                              u64 aruid) const {
    const u64 aruid_index = GetIndexFromAruid(aruid);
    if (aruid_index >= AruidIndexMax) {
        return ResultNpadNotConnected;
    }

    auto& data = state[aruid_index].data;
    if (!data.GetNpadStatus().is_supported_styleset_set) {
        return ResultUndefinedStyleset;
    }

    out_style_Set = data.GetSupportedNpadStyleSet();
    return ResultSuccess;
}

Result NPadResource::GetMaskedSupportedNpadStyleSet(Core::HID::NpadStyleSet& out_style_set,
                                                    u64 aruid) const {
    if (aruid == SystemAruid) {
        out_style_set = Core::HID::NpadStyleSet::Fullkey | Core::HID::NpadStyleSet::Handheld |
                        Core::HID::NpadStyleSet::JoyDual | Core::HID::NpadStyleSet::JoyLeft |
                        Core::HID::NpadStyleSet::JoyRight | Core::HID::NpadStyleSet::Palma |
                        Core::HID::NpadStyleSet::SystemExt | Core::HID::NpadStyleSet::System;
        return ResultSuccess;
    }

    const u64 aruid_index = GetIndexFromAruid(aruid);
    if (aruid_index >= AruidIndexMax) {
        return ResultNpadNotConnected;
    }

    auto& data = state[aruid_index].data;
    if (!data.GetNpadStatus().is_supported_styleset_set) {
        return ResultUndefinedStyleset;
    }

    Core::HID::NpadStyleSet mask{Core::HID::NpadStyleSet::None};
    out_style_set = data.GetSupportedNpadStyleSet();

    switch (state[aruid_index].npad_revision) {
    case NpadRevision::Revision1:
        mask = Core::HID::NpadStyleSet::Fullkey | Core::HID::NpadStyleSet::Handheld |
               Core::HID::NpadStyleSet::JoyDual | Core::HID::NpadStyleSet::JoyLeft |
               Core::HID::NpadStyleSet::JoyRight | Core::HID::NpadStyleSet::Gc |
               Core::HID::NpadStyleSet::Palma | Core::HID::NpadStyleSet::SystemExt |
               Core::HID::NpadStyleSet::System;
        break;
    case NpadRevision::Revision2:
        mask = Core::HID::NpadStyleSet::Fullkey | Core::HID::NpadStyleSet::Handheld |
               Core::HID::NpadStyleSet::JoyDual | Core::HID::NpadStyleSet::JoyLeft |
               Core::HID::NpadStyleSet::JoyRight | Core::HID::NpadStyleSet::Gc |
               Core::HID::NpadStyleSet::Palma | Core::HID::NpadStyleSet::Lark |
               Core::HID::NpadStyleSet::SystemExt | Core::HID::NpadStyleSet::System;
        break;
    case NpadRevision::Revision3:
        mask = Core::HID::NpadStyleSet::Fullkey | Core::HID::NpadStyleSet::Handheld |
               Core::HID::NpadStyleSet::JoyDual | Core::HID::NpadStyleSet::JoyLeft |
               Core::HID::NpadStyleSet::JoyRight | Core::HID::NpadStyleSet::Gc |
               Core::HID::NpadStyleSet::Palma | Core::HID::NpadStyleSet::Lark |
               Core::HID::NpadStyleSet::HandheldLark | Core::HID::NpadStyleSet::Lucia |
               Core::HID::NpadStyleSet::Lagoon | Core::HID::NpadStyleSet::Lager |
               Core::HID::NpadStyleSet::SystemExt | Core::HID::NpadStyleSet::System;
        break;
    default:
        mask = Core::HID::NpadStyleSet::Fullkey | Core::HID::NpadStyleSet::Handheld |
               Core::HID::NpadStyleSet::JoyDual | Core::HID::NpadStyleSet::JoyLeft |
               Core::HID::NpadStyleSet::JoyRight | Core::HID::NpadStyleSet::SystemExt |
               Core::HID::NpadStyleSet::System;
        break;
    }

    out_style_set = out_style_set & mask;
    return ResultSuccess;
}

Result NPadResource::GetAvailableStyleset(Core::HID::NpadStyleSet& out_style_set, u64 aruid) const {
    const u64 aruid_index = GetIndexFromAruid(aruid);
    if (aruid_index >= AruidIndexMax) {
        return ResultNpadNotConnected;
    }

    auto& data = state[aruid_index].data;
    if (!data.GetNpadStatus().is_supported_styleset_set) {
        return ResultUndefinedStyleset;
    }

    Core::HID::NpadStyleSet mask{Core::HID::NpadStyleSet::None};
    out_style_set = data.GetSupportedNpadStyleSet();

    switch (state[aruid_index].npad_revision) {
    case NpadRevision::Revision1:
        mask = Core::HID::NpadStyleSet::Fullkey | Core::HID::NpadStyleSet::Handheld |
               Core::HID::NpadStyleSet::JoyDual | Core::HID::NpadStyleSet::JoyLeft |
               Core::HID::NpadStyleSet::JoyRight | Core::HID::NpadStyleSet::Gc |
               Core::HID::NpadStyleSet::Palma | Core::HID::NpadStyleSet::SystemExt |
               Core::HID::NpadStyleSet::System;
        break;
    case NpadRevision::Revision2:
        mask = Core::HID::NpadStyleSet::Fullkey | Core::HID::NpadStyleSet::Handheld |
               Core::HID::NpadStyleSet::JoyDual | Core::HID::NpadStyleSet::JoyLeft |
               Core::HID::NpadStyleSet::JoyRight | Core::HID::NpadStyleSet::Gc |
               Core::HID::NpadStyleSet::Palma | Core::HID::NpadStyleSet::Lark |
               Core::HID::NpadStyleSet::SystemExt | Core::HID::NpadStyleSet::System;
        break;
    case NpadRevision::Revision3:
        mask = Core::HID::NpadStyleSet::Fullkey | Core::HID::NpadStyleSet::Handheld |
               Core::HID::NpadStyleSet::JoyDual | Core::HID::NpadStyleSet::JoyLeft |
               Core::HID::NpadStyleSet::JoyRight | Core::HID::NpadStyleSet::Gc |
               Core::HID::NpadStyleSet::Palma | Core::HID::NpadStyleSet::Lark |
               Core::HID::NpadStyleSet::HandheldLark | Core::HID::NpadStyleSet::Lucia |
               Core::HID::NpadStyleSet::Lagoon | Core::HID::NpadStyleSet::Lager |
               Core::HID::NpadStyleSet::SystemExt | Core::HID::NpadStyleSet::System;
        break;
    default:
        mask = Core::HID::NpadStyleSet::Fullkey | Core::HID::NpadStyleSet::Handheld |
               Core::HID::NpadStyleSet::JoyDual | Core::HID::NpadStyleSet::JoyLeft |
               Core::HID::NpadStyleSet::JoyRight | Core::HID::NpadStyleSet::SystemExt |
               Core::HID::NpadStyleSet::System;
        break;
    }

    out_style_set = out_style_set & mask;
    return ResultSuccess;
}

NpadRevision NPadResource::GetNpadRevision(u64 aruid) const {
    const u64 aruid_index = GetIndexFromAruid(aruid);
    if (aruid_index >= AruidIndexMax) {
        return NpadRevision::Revision0;
    }

    return state[aruid_index].npad_revision;
}

Result NPadResource::IsSupportedNpadStyleSet(bool& is_set, u64 aruid) {
    const u64 aruid_index = GetIndexFromAruid(aruid);
    if (aruid_index >= AruidIndexMax) {
        return ResultNpadNotConnected;
    }

    is_set = state[aruid_index].data.GetNpadStatus().is_supported_styleset_set.Value() != 0;
    return ResultSuccess;
}

Result NPadResource::SetNpadJoyHoldType(u64 aruid, NpadJoyHoldType hold_type) {
    const u64 aruid_index = GetIndexFromAruid(aruid);
    if (aruid_index >= AruidIndexMax) {
        return ResultNpadNotConnected;
    }

    state[aruid_index].data.SetNpadJoyHoldType(hold_type);
    if (active_data_aruid == aruid) {
        active_data.SetNpadJoyHoldType(hold_type);
    }
    return ResultSuccess;
}

Result NPadResource::GetNpadJoyHoldType(NpadJoyHoldType& hold_type, u64 aruid) const {
    const u64 aruid_index = GetIndexFromAruid(aruid);
    if (aruid_index >= AruidIndexMax) {
        return ResultNpadNotConnected;
    }

    auto& data = state[aruid_index].data;
    if (data.GetNpadStatus().is_policy || data.GetNpadStatus().is_full_policy) {
        hold_type = active_data.GetNpadJoyHoldType();
        return ResultSuccess;
    }
    hold_type = data.GetNpadJoyHoldType();
    return ResultSuccess;
}

Result NPadResource::SetNpadHandheldActivationMode(u64 aruid,
                                                   NpadHandheldActivationMode activation_mode) {
    const u64 aruid_index = GetIndexFromAruid(aruid);
    if (aruid_index >= AruidIndexMax) {
        return ResultNpadNotConnected;
    }

    state[aruid_index].data.SetHandheldActivationMode(activation_mode);
    if (active_data_aruid == aruid) {
        active_data.SetHandheldActivationMode(activation_mode);
    }
    return ResultSuccess;
}

Result NPadResource::GetNpadHandheldActivationMode(NpadHandheldActivationMode& activation_mode,
                                                   u64 aruid) const {
    const u64 aruid_index = GetIndexFromAruid(aruid);
    if (aruid_index >= AruidIndexMax) {
        return ResultNpadNotConnected;
    }

    activation_mode = state[aruid_index].data.GetHandheldActivationMode();
    return ResultSuccess;
}

Result NPadResource::SetSupportedNpadIdType(
    u64 aruid, std::span<const Core::HID::NpadIdType> supported_npad_list) {
    const u64 aruid_index = GetIndexFromAruid(aruid);
    if (aruid_index >= AruidIndexMax) {
        return ResultNpadNotConnected;
    }
    if (supported_npad_list.size() > MaxSupportedNpadIdTypes) {
        return ResultInvalidArraySize;
    }

    Result result = state[aruid_index].data.SetSupportedNpadIdType(supported_npad_list);
    if (result.IsSuccess() && active_data_aruid == aruid) {
        result = active_data.SetSupportedNpadIdType(supported_npad_list);
    }

    return result;
}

bool NPadResource::IsControllerSupported(u64 aruid, Core::HID::NpadStyleIndex style_index) const {
    const u64 aruid_index = GetIndexFromAruid(aruid);
    if (aruid_index >= AruidIndexMax) {
        return false;
    }
    return state[aruid_index].data.IsNpadStyleIndexSupported(style_index);
}

Result NPadResource::SetLrAssignmentMode(u64 aruid, bool is_enabled) {
    const u64 aruid_index = GetIndexFromAruid(aruid);
    if (aruid_index >= AruidIndexMax) {
        return ResultNpadNotConnected;
    }

    state[aruid_index].data.SetLrAssignmentMode(is_enabled);
    if (active_data_aruid == aruid) {
        active_data.SetLrAssignmentMode(is_enabled);
    }
    return ResultSuccess;
}

Result NPadResource::GetLrAssignmentMode(bool& is_enabled, u64 aruid) const {
    const u64 aruid_index = GetIndexFromAruid(aruid);
    if (aruid_index >= AruidIndexMax) {
        return ResultNpadNotConnected;
    }

    is_enabled = state[aruid_index].data.GetLrAssignmentMode();
    return ResultSuccess;
}

Result NPadResource::SetAssigningSingleOnSlSrPress(u64 aruid, bool is_enabled) {
    const u64 aruid_index = GetIndexFromAruid(aruid);
    if (aruid_index >= AruidIndexMax) {
        return ResultNpadNotConnected;
    }

    state[aruid_index].data.SetAssigningSingleOnSlSrPress(is_enabled);
    if (active_data_aruid == aruid) {
        active_data.SetAssigningSingleOnSlSrPress(is_enabled);
    }
    return ResultSuccess;
}

Result NPadResource::IsAssigningSingleOnSlSrPressEnabled(bool& is_enabled, u64 aruid) const {
    const u64 aruid_index = GetIndexFromAruid(aruid);
    if (aruid_index >= AruidIndexMax) {
        return ResultNpadNotConnected;
    }

    is_enabled = state[aruid_index].data.GetAssigningSingleOnSlSrPress();
    return ResultSuccess;
}

Result NPadResource::AcquireNpadStyleSetUpdateEventHandle(u64 aruid,
                                                          Kernel::KReadableEvent** out_event,
                                                          Core::HID::NpadIdType npad_id) {
    const u64 aruid_index = GetIndexFromAruid(aruid);
    if (aruid_index >= AruidIndexMax) {
        return ResultNpadNotConnected;
    }

    auto& controller_state = state[aruid_index].controller_state[NpadIdTypeToIndex(npad_id)];
    if (!controller_state.is_styleset_update_event_initialized) {
        // Auto clear = true
        controller_state.style_set_update_event =
            service_context.CreateEvent("NpadResource:StylesetUpdateEvent");

        // Assume creating the event succeeds otherwise crash the system here
        controller_state.is_styleset_update_event_initialized = true;
    }

    *out_event = &controller_state.style_set_update_event->GetReadableEvent();

    if (controller_state.is_styleset_update_event_initialized) {
        controller_state.style_set_update_event->Signal();
    }

    return ResultSuccess;
}

Result NPadResource::SignalStyleSetUpdateEvent(u64 aruid, Core::HID::NpadIdType npad_id) {
    const u64 aruid_index = GetIndexFromAruid(aruid);
    if (aruid_index >= AruidIndexMax) {
        return ResultNpadNotConnected;
    }
    auto controller = state[aruid_index].controller_state[NpadIdTypeToIndex(npad_id)];
    if (controller.is_styleset_update_event_initialized) {
        controller.style_set_update_event->Signal();
    }
    return ResultSuccess;
}

Result NPadResource::GetHomeProtectionEnabled(bool& is_enabled, u64 aruid,
                                              Core::HID::NpadIdType npad_id) const {
    const u64 aruid_index = GetIndexFromAruid(aruid);
    if (aruid_index >= AruidIndexMax) {
        return ResultNpadNotConnected;
    }

    is_enabled = state[aruid_index].data.GetHomeProtectionEnabled(npad_id);
    return ResultSuccess;
}

Result NPadResource::SetHomeProtectionEnabled(u64 aruid, Core::HID::NpadIdType npad_id,
                                              bool is_enabled) {
    const u64 aruid_index = GetIndexFromAruid(aruid);
    if (aruid_index >= AruidIndexMax) {
        return ResultNpadNotConnected;
    }

    state[aruid_index].data.SetHomeProtectionEnabled(is_enabled, npad_id);
    if (active_data_aruid == aruid) {
        active_data.SetHomeProtectionEnabled(is_enabled, npad_id);
    }
    return ResultSuccess;
}

Result NPadResource::SetNpadAnalogStickUseCenterClamp(u64 aruid, bool is_enabled) {
    const u64 aruid_index = GetIndexFromAruid(aruid);
    if (aruid_index >= AruidIndexMax) {
        return ResultNpadNotConnected;
    }

    state[aruid_index].data.SetNpadAnalogStickUseCenterClamp(is_enabled);
    if (active_data_aruid == aruid) {
        active_data.SetNpadAnalogStickUseCenterClamp(is_enabled);
    }
    return ResultSuccess;
}

Result NPadResource::SetButtonConfig(u64 aruid, Core::HID::NpadIdType npad_id, std::size_t index,
                                     Core::HID::NpadButton button_config) {
    const u64 aruid_index = GetIndexFromAruid(aruid);
    if (aruid_index >= AruidIndexMax) {
        return ResultNpadNotConnected;
    }

    state[aruid_index].button_config[NpadIdTypeToIndex(npad_id)][index] = button_config;
    return ResultSuccess;
}

Core::HID::NpadButton NPadResource::GetButtonConfig(u64 aruid, Core::HID::NpadIdType npad_id,
                                                    std::size_t index, Core::HID::NpadButton mask,
                                                    bool is_enabled) {
    const u64 aruid_index = GetIndexFromAruid(aruid);
    if (aruid_index >= AruidIndexMax) {
        return Core::HID::NpadButton::None;
    }

    auto& button_config = state[aruid_index].button_config[NpadIdTypeToIndex(npad_id)][index];
    if (is_enabled) {
        button_config = button_config | mask;
        return button_config;
    }

    button_config = Core::HID::NpadButton::None;
    return Core::HID::NpadButton::None;
}

void NPadResource::ResetButtonConfig() {
    for (auto& selected_state : state) {
        selected_state.button_config = {};
    }
}

Result NPadResource::SetNpadCaptureButtonAssignment(u64 aruid,
                                                    Core::HID::NpadStyleSet npad_style_set,
                                                    Core::HID::NpadButton button_assignment) {
    const u64 aruid_index = GetIndexFromAruid(aruid);
    if (aruid_index >= AruidIndexMax) {
        return ResultNpadNotConnected;
    }

    // Must be a power of two
    const auto raw_styleset = static_cast<u32>(npad_style_set);
    if (raw_styleset == 0 && (raw_styleset & (raw_styleset - 1)) != 0) {
        return ResultMultipleStyleSetSelected;
    }

    std::size_t style_index{};
    Core::HID::NpadStyleSet style_selected{};
    for (style_index = 0; style_index < StyleIndexCount; ++style_index) {
        style_selected = GetStylesetByIndex(style_index);
        if (npad_style_set == style_selected) {
            break;
        }
    }

    if (style_selected == Core::HID::NpadStyleSet::None) {
        return ResultMultipleStyleSetSelected;
    }

    state[aruid_index].data.SetCaptureButtonAssignment(button_assignment, style_index);
    if (active_data_aruid == aruid) {
        active_data.SetCaptureButtonAssignment(button_assignment, style_index);
    }
    return ResultSuccess;
}

Result NPadResource::ClearNpadCaptureButtonAssignment(u64 aruid) {
    const u64 aruid_index = GetIndexFromAruid(aruid);
    if (aruid_index >= AruidIndexMax) {
        return ResultNpadNotConnected;
    }

    for (std::size_t i = 0; i < StyleIndexCount; i++) {
        state[aruid_index].data.SetCaptureButtonAssignment(Core::HID::NpadButton::None, i);
        if (active_data_aruid == aruid) {
            active_data.SetCaptureButtonAssignment(Core::HID::NpadButton::None, i);
        }
    }
    return ResultSuccess;
}

std::size_t NPadResource::GetNpadCaptureButtonAssignment(std::span<Core::HID::NpadButton> out_list,
                                                         u64 aruid) const {
    const u64 aruid_index = GetIndexFromAruid(aruid);
    if (aruid_index >= AruidIndexMax) {
        return 0;
    }
    return state[aruid_index].data.GetNpadCaptureButtonAssignmentList(out_list);
}

void NPadResource::SetNpadRevision(u64 aruid, NpadRevision revision) {
    const u64 aruid_index = GetIndexFromAruid(aruid);
    if (aruid_index >= AruidIndexMax) {
        return;
    }

    state[aruid_index].npad_revision = revision;
}

Result NPadResource::SetNpadSystemExtStateEnabled(u64 aruid, bool is_enabled) {
    const u64 aruid_index = GetIndexFromAruid(aruid);
    if (aruid_index >= AruidIndexMax) {
        return ResultNpadNotConnected;
    }

    state[aruid_index].data.SetNpadAnalogStickUseCenterClamp(is_enabled);
    if (active_data_aruid == aruid) {
        active_data.SetNpadAnalogStickUseCenterClamp(is_enabled);
    }
    return ResultSuccess;
}

} // namespace Service::HID
