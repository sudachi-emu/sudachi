// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/core.h"
#include "core/hle/kernel/k_shared_memory.h"
#include "hid_core/hid_result.h"
#include "hid_core/resources/applet_resource.h"
#include "hid_core/resources/shared_memory_format.h"

namespace Service::HID {

AppletResource::AppletResource(Core::System& system_) : system{system_} {}

AppletResource::~AppletResource() = default;

Result AppletResource::CreateAppletResource(u64 aruid) {
    const u64 index = GetIndexFromAruid(aruid);

    if (index >= AruidIndexMax) {
        return ResultAruidNotRegistered;
    }

    if (data[index].flag.is_assigned) {
        return ResultAruidAlreadyRegistered;
    }

    auto& shared_memory = shared_memory_holder[index];
    if (!shared_memory.IsMapped()) {
        const Result result = shared_memory.Initialize(system);
        if (result.IsError()) {
            return result;
        }
        if (shared_memory.GetAddress() == nullptr) {
            shared_memory.Finalize();
            return ResultSharedMemoryNotInitialized;
        }
    }

    auto* shared_memory_format = shared_memory.GetAddress();
    if (shared_memory_format != nullptr) {
        shared_memory_format->Initialize();
    }

    data[index].shared_memory_format = shared_memory_format;
    data[index].flag.is_assigned.Assign(true);
    // TODO: InitializeSixAxisControllerConfig(false);
    active_aruid = aruid;
    return ResultSuccess;
}

Result AppletResource::RegisterAppletResourceUserId(u64 aruid, bool enable_input) {
    const u64 index = GetIndexFromAruid(aruid);

    if (index < AruidIndexMax) {
        return ResultAruidAlreadyRegistered;
    }

    std::size_t data_index = AruidIndexMax;
    for (std::size_t i = 0; i < AruidIndexMax; i++) {
        if (!data[i].flag.is_initialized) {
            data_index = i;
            break;
        }
    }

    if (data_index == AruidIndexMax) {
        return ResultAruidNoAvailableEntries;
    }

    AruidData& aruid_data = data[data_index];

    aruid_data.aruid = aruid;
    aruid_data.flag.is_initialized.Assign(true);
    if (enable_input) {
        aruid_data.flag.enable_pad_input.Assign(true);
        aruid_data.flag.enable_six_axis_sensor.Assign(true);
        aruid_data.flag.bit_18.Assign(true);
        aruid_data.flag.enable_touchscreen.Assign(true);
    }

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

void AppletResource::UnregisterAppletResourceUserId(u64 aruid) {
    const u64 index = GetIndexFromAruid(aruid);

    if (index >= AruidIndexMax) {
        return;
    }

    FreeAppletResourceId(aruid);
    DestroySevenSixAxisTransferMemory();
    data[index].flag.raw = 0;
    data[index].aruid = 0;

    registration_list.flag[index] = RegistrationStatus::PendingDelete;

    for (std::size_t i = 0; i < AruidIndexMax; i++) {
        if (registration_list.flag[i] == RegistrationStatus::Initialized) {
            active_aruid = registration_list.aruid[i];
        }
    }
}

void AppletResource::FreeAppletResourceId(u64 aruid) {
    const u64 index = GetIndexFromAruid(aruid);
    if (index >= AruidIndexMax) {
        return;
    }

    auto& aruid_data = data[index];
    if (aruid_data.flag.is_assigned) {
        aruid_data.shared_memory_format = nullptr;
        aruid_data.flag.is_assigned.Assign(false);
        shared_memory_holder[index].Finalize();
    }
}

u64 AppletResource::GetActiveAruid() {
    return active_aruid;
}

Result AppletResource::GetSharedMemoryHandle(Kernel::KSharedMemory** out_handle, u64 aruid) {
    const u64 index = GetIndexFromAruid(aruid);
    if (index >= AruidIndexMax) {
        return ResultAruidNotRegistered;
    }

    *out_handle = shared_memory_holder[index].GetHandle();
    return ResultSuccess;
}

Result AppletResource::GetSharedMemoryFormat(SharedMemoryFormat** out_shared_memory_format,
                                             u64 aruid) {
    const u64 index = GetIndexFromAruid(aruid);
    if (index >= AruidIndexMax) {
        return ResultAruidNotRegistered;
    }

    *out_shared_memory_format = data[index].shared_memory_format;
    return ResultSuccess;
}

AruidData* AppletResource::GetAruidData(u64 aruid) {
    const u64 aruid_index = GetIndexFromAruid(aruid);
    if (aruid_index == AruidIndexMax) {
        return nullptr;
    }
    return &data[aruid_index];
}

AruidData* AppletResource::GetAruidDataByIndex(std::size_t aruid_index) {
    return &data[aruid_index];
}

bool AppletResource::IsVibrationAruidActive(u64 aruid) const {
    return aruid == 0 || aruid == active_vibration_aruid;
}

u64 AppletResource::GetIndexFromAruid(u64 aruid) {
    for (std::size_t i = 0; i < AruidIndexMax; i++) {
        if (registration_list.flag[i] == RegistrationStatus::Initialized &&
            registration_list.aruid[i] == aruid) {
            return i;
        }
    }
    return AruidIndexMax;
}

Result AppletResource::DestroySevenSixAxisTransferMemory() {
    // TODO
    return ResultSuccess;
}

void AppletResource::EnableInput(u64 aruid, bool is_enabled) {
    const u64 index = GetIndexFromAruid(aruid);
    if (index >= AruidIndexMax) {
        return;
    }

    data[index].flag.enable_pad_input.Assign(is_enabled);
    data[index].flag.enable_touchscreen.Assign(is_enabled);
}

bool AppletResource::SetAruidValidForVibration(u64 aruid, bool is_enabled) {
    const u64 index = GetIndexFromAruid(aruid);
    if (index >= AruidIndexMax) {
        return false;
    }

    if (!is_enabled && aruid == active_vibration_aruid) {
        active_vibration_aruid = SystemAruid;
        return true;
    }

    if (is_enabled && aruid != active_vibration_aruid) {
        active_vibration_aruid = aruid;
        return true;
    }

    return false;
}

void AppletResource::EnableSixAxisSensor(u64 aruid, bool is_enabled) {
    const u64 index = GetIndexFromAruid(aruid);
    if (index >= AruidIndexMax) {
        return;
    }

    data[index].flag.enable_six_axis_sensor.Assign(is_enabled);
}

void AppletResource::EnablePadInput(u64 aruid, bool is_enabled) {
    const u64 index = GetIndexFromAruid(aruid);
    if (index >= AruidIndexMax) {
        return;
    }

    data[index].flag.enable_pad_input.Assign(is_enabled);
}

void AppletResource::EnableTouchScreen(u64 aruid, bool is_enabled) {
    const u64 index = GetIndexFromAruid(aruid);
    if (index >= AruidIndexMax) {
        return;
    }

    data[index].flag.enable_touchscreen.Assign(is_enabled);
}

void AppletResource::SetIsPalmaConnectable(u64 aruid, bool is_connectable) {
    const u64 index = GetIndexFromAruid(aruid);
    if (index >= AruidIndexMax) {
        return;
    }

    data[index].flag.is_palma_connectable.Assign(is_connectable);
}

void AppletResource::EnablePalmaBoostMode(u64 aruid, bool is_enabled) {
    const u64 index = GetIndexFromAruid(aruid);
    if (index >= AruidIndexMax) {
        return;
    }

    data[index].flag.enable_palma_boost_mode.Assign(is_enabled);
}

Result AppletResource::RegisterCoreAppletResource() {
    if (ref_counter == std::numeric_limits<s32>::max() - 1) {
        return ResultAppletResourceOverflow;
    }
    if (ref_counter == 0) {
        const u64 index = GetIndexFromAruid(0);
        if (index < AruidIndexMax) {
            return ResultAruidAlreadyRegistered;
        }

        std::size_t data_index = AruidIndexMax;
        for (std::size_t i = 0; i < AruidIndexMax; i++) {
            if (!data[i].flag.is_initialized) {
                data_index = i;
                break;
            }
        }

        if (data_index == AruidIndexMax) {
            return ResultAruidNoAvailableEntries;
        }

        AruidData& aruid_data = data[data_index];

        aruid_data.aruid = 0;
        aruid_data.flag.is_initialized.Assign(true);
        aruid_data.flag.enable_pad_input.Assign(true);
        aruid_data.flag.enable_six_axis_sensor.Assign(true);
        aruid_data.flag.bit_18.Assign(true);
        aruid_data.flag.enable_touchscreen.Assign(true);

        data_index = AruidIndexMax;
        for (std::size_t i = 0; i < AruidIndexMax; i++) {
            if (registration_list.flag[i] == RegistrationStatus::Initialized) {
                if (registration_list.aruid[i] != 0) {
                    continue;
                }
                data_index = i;
                break;
            }
            if (registration_list.flag[i] == RegistrationStatus::None) {
                data_index = i;
                break;
            }
        }

        Result result = ResultSuccess;

        if (data_index == AruidIndexMax) {
            result = CreateAppletResource(0);
        } else {
            registration_list.flag[data_index] = RegistrationStatus::Initialized;
            registration_list.aruid[data_index] = 0;
        }

        if (result.IsError()) {
            UnregisterAppletResourceUserId(0);
            return result;
        }
    }
    ref_counter++;
    return ResultSuccess;
}

Result AppletResource::UnregisterCoreAppletResource() {
    if (ref_counter == 0) {
        return ResultAppletResourceNotInitialized;
    }

    if (--ref_counter == 0) {
        UnregisterAppletResourceUserId(0);
    }

    return ResultSuccess;
}

} // namespace Service::HID
