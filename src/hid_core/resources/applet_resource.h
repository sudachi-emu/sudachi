// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <array>
#include <mutex>

#include "common/bit_field.h"
#include "common/common_types.h"
#include "core/hle/result.h"
#include "hid_core/resources/shared_memory_holder.h"

namespace Core {
class System;
} // namespace Core

namespace Kernel {
class KEvent;
class KSharedMemory;
} // namespace Kernel

namespace Service::HID {
struct SharedMemoryFormat;
class AppletResource;
class NPadResource;

static constexpr std::size_t AruidIndexMax = 0x20;
static constexpr u64 SystemAruid = 0;

enum class RegistrationStatus : u32 {
    None,
    Initialized,
    PendingDelete,
};

struct DataStatusFlag {
    union {
        u32 raw{};

        BitField<0, 1, u32> is_initialized;
        BitField<1, 1, u32> is_assigned;
        BitField<16, 1, u32> enable_pad_input;
        BitField<17, 1, u32> enable_six_axis_sensor;
        BitField<18, 1, u32> bit_18;
        BitField<19, 1, u32> is_palma_connectable;
        BitField<20, 1, u32> enable_palma_boost_mode;
        BitField<21, 1, u32> enable_touchscreen;
    };
};

struct AruidRegisterList {
    std::array<RegistrationStatus, AruidIndexMax> flag{};
    std::array<u64, AruidIndexMax> aruid{};
};
static_assert(sizeof(AruidRegisterList) == 0x180, "AruidRegisterList is an invalid size");

struct AruidData {
    DataStatusFlag flag{};
    u64 aruid{};
    SharedMemoryFormat* shared_memory_format{nullptr};
};

struct HandheldConfig {
    bool is_handheld_hid_enabled;
    bool is_force_handheld;
    bool is_joycon_rail_enabled;
    bool is_force_handheld_style_vibration;
};
static_assert(sizeof(HandheldConfig) == 0x4, "HandheldConfig is an invalid size");

struct AppletResourceHolder {
    std::shared_ptr<AppletResource> applet_resource{nullptr};
    std::recursive_mutex* shared_mutex{nullptr};
    NPadResource* shared_npad_resource{nullptr};
    std::shared_ptr<HandheldConfig> handheld_config{nullptr};
    Kernel::KEvent* input_event{nullptr};
    std::mutex* input_mutex{nullptr};
};

class AppletResource {
public:
    explicit AppletResource(Core::System& system_);
    ~AppletResource();

    Result CreateAppletResource(u64 aruid);

    Result RegisterAppletResourceUserId(u64 aruid, bool enable_input);
    void UnregisterAppletResourceUserId(u64 aruid);

    void FreeAppletResourceId(u64 aruid);

    u64 GetActiveAruid();
    Result GetSharedMemoryHandle(Kernel::KSharedMemory** out_handle, u64 aruid);
    Result GetSharedMemoryFormat(SharedMemoryFormat** out_shared_memory_format, u64 aruid);
    AruidData* GetAruidData(u64 aruid);
    AruidData* GetAruidDataByIndex(std::size_t aruid_index);

    bool IsVibrationAruidActive(u64 aruid) const;

    u64 GetIndexFromAruid(u64 aruid);

    Result DestroySevenSixAxisTransferMemory();

    void EnableInput(u64 aruid, bool is_enabled);
    bool SetAruidValidForVibration(u64 aruid, bool is_enabled);
    void EnableSixAxisSensor(u64 aruid, bool is_enabled);
    void EnablePadInput(u64 aruid, bool is_enabled);
    void EnableTouchScreen(u64 aruid, bool is_enabled);
    void SetIsPalmaConnectable(u64 aruid, bool is_connectable);
    void EnablePalmaBoostMode(u64 aruid, bool is_enabled);

    Result RegisterCoreAppletResource();
    Result UnregisterCoreAppletResource();

private:
    u64 active_aruid{};
    AruidRegisterList registration_list{};
    std::array<AruidData, AruidIndexMax> data{};
    std::array<SharedMemoryHolder, AruidIndexMax> shared_memory_holder{};
    s32 ref_counter{};
    u64 active_vibration_aruid;

    Core::System& system;
};
} // namespace Service::HID
