// SPDX-FileCopyrightText: Copyright 2018 sudachi Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <locale>
#include "common/hex_util.h"
#include "common/profiling.h"
#include "common/swap.h"
#include "core/arm/debug.h"
#include "core/core.h"
#include "core/core_timing.h"
#include "core/hle/kernel/k_page_table.h"
#include "core/hle/kernel/k_process.h"
#include "core/hle/kernel/k_process_page_table.h"
#include "core/hle/kernel/svc_types.h"
#include "core/hle/service/hid/hid_server.h"
#include "core/hle/service/sm/sm.h"
#include "core/memory.h"
#include "core/memory/cheat_engine.h"
#include "hid_core/resource_manager.h"
#include "hid_core/resources/npad/npad.h"

namespace Core::Memory {
namespace {
constexpr auto CHEAT_ENGINE_NS = std::chrono::nanoseconds{1000000000 / 12};

std::string_view ExtractName(std::size_t& out_name_size, std::string_view data,
                             std::size_t start_index, char match) {
    auto end_index = start_index;
    while (data[end_index] != match) {
        ++end_index;
        if (end_index > data.size()) {
            return {};
        }
    }

    out_name_size = end_index - start_index;

    // Clamp name if it's too big
    if (out_name_size > sizeof(CheatDefinition::readable_name)) {
        end_index = start_index + sizeof(CheatDefinition::readable_name);
    }

    return data.substr(start_index, end_index - start_index);
}
} // Anonymous namespace

StandardVmCallbacks::StandardVmCallbacks(System& system_, const CheatProcessMetadata& metadata_)
    : metadata{metadata_}, system{system_} {}

StandardVmCallbacks::~StandardVmCallbacks() = default;

void StandardVmCallbacks::MemoryReadUnsafe(VAddr address, void* data, u64 size) {
    // Return zero on invalid address
    if (!IsAddressInRange(address) || !system.ApplicationMemory().IsValidVirtualAddress(address)) {
        std::memset(data, 0, size);
        return;
    }

    system.ApplicationMemory().ReadBlock(address, data, size);
}

void StandardVmCallbacks::MemoryWriteUnsafe(VAddr address, const void* data, u64 size) {
    // Skip invalid memory write address
    if (!IsAddressInRange(address) || !system.ApplicationMemory().IsValidVirtualAddress(address)) {
        return;
    }

    if (system.ApplicationMemory().WriteBlock(address, data, size)) {
        Core::InvalidateInstructionCacheRange(system.ApplicationProcess(), address, size);
    }
}

u64 StandardVmCallbacks::HidKeysDown() {
    const auto hid = system.ServiceManager().GetService<Service::HID::IHidServer>("hid");
    if (hid == nullptr) {
        LOG_WARNING(CheatEngine, "Attempted to read input state, but hid is not initialized!");
        return 0;
    }

    const auto applet_resource = hid->GetResourceManager();
    if (applet_resource == nullptr || applet_resource->GetNpad() == nullptr) {
        LOG_WARNING(CheatEngine,
                    "Attempted to read input state, but applet resource is not initialized!");
        return 0;
    }

    const auto press_state = applet_resource->GetNpad()->GetAndResetPressState();
    return static_cast<u64>(press_state & HID::NpadButton::All);
}

void StandardVmCallbacks::PauseProcess() {
    if (system.ApplicationProcess()->IsSuspended()) {
        return;
    }
    system.ApplicationProcess()->SetActivity(Kernel::Svc::ProcessActivity::Paused);
}

void StandardVmCallbacks::ResumeProcess() {
    if (!system.ApplicationProcess()->IsSuspended()) {
        return;
    }
    system.ApplicationProcess()->SetActivity(Kernel::Svc::ProcessActivity::Runnable);
}

void StandardVmCallbacks::DebugLog(u8 id, u64 value) {
    LOG_INFO(CheatEngine, "Cheat triggered DebugLog: ID '{:01X}' Value '{:016X}'", id, value);
}

void StandardVmCallbacks::CommandLog(std::string_view data) {
    LOG_DEBUG(CheatEngine, "[DmntCheatVm]: {}",
              data.back() == '\n' ? data.substr(0, data.size() - 1) : data);
}

bool StandardVmCallbacks::IsAddressInRange(VAddr in) const {
    if ((in < metadata.main_nso_extents.base ||
         in >= metadata.main_nso_extents.base + metadata.main_nso_extents.size) &&
        (in < metadata.heap_extents.base ||
         in >= metadata.heap_extents.base + metadata.heap_extents.size) &&
        (in < metadata.alias_extents.base ||
         in >= metadata.alias_extents.base + metadata.alias_extents.size) &&
        (in < metadata.aslr_extents.base ||
         in >= metadata.aslr_extents.base + metadata.aslr_extents.size)) {
        LOG_DEBUG(CheatEngine,
                  "Cheat attempting to access memory at invalid address={:016X}, if this "
                  "persists, "
                  "the cheat may be incorrect. However, this may be normal early in execution if "
                  "the game has not properly set up yet.",
                  in);
        return false; ///< Invalid addresses will hard crash
    }

    return true;
}

CheatParser::~CheatParser() = default;

TextCheatParser::~TextCheatParser() = default;

std::vector<CheatEntry> TextCheatParser::Parse(std::string_view data) const {
    std::vector<CheatEntry> out(1);
    std::optional<u64> current_entry;

    for (std::size_t i = 0; i < data.size(); ++i) {
        if (::isspace(data[i])) {
            continue;
        }

        if (data[i] == '{') {
            current_entry = 0;

            if (out[*current_entry].definition.num_opcodes > 0) {
                return {};
            }

            std::size_t name_size{};
            const auto name = ExtractName(name_size, data, i + 1, '}');
            if (name.empty()) {
                return {};
            }

            std::memcpy(out[*current_entry].definition.readable_name.data(), name.data(),
                        std::min<std::size_t>(out[*current_entry].definition.readable_name.size(),
                                              name.size()));
            out[*current_entry]
                .definition.readable_name[out[*current_entry].definition.readable_name.size() - 1] =
                '\0';

            i += name_size + 1;
        } else if (data[i] == '[') {
            current_entry = out.size();
            out.emplace_back();

            std::size_t name_size{};
            const auto name = ExtractName(name_size, data, i + 1, ']');
            if (name.empty()) {
                return {};
            }

            std::memcpy(out[*current_entry].definition.readable_name.data(), name.data(),
                        std::min<std::size_t>(out[*current_entry].definition.readable_name.size(),
                                              name.size()));
            out[*current_entry]
                .definition.readable_name[out[*current_entry].definition.readable_name.size() - 1] =
                '\0';

            i += name_size + 1;
        } else if (::isxdigit(data[i])) {
            if (!current_entry || out[*current_entry].definition.num_opcodes >=
                                      out[*current_entry].definition.opcodes.size()) {
                return {};
            }

            const auto hex = std::string(data.substr(i, 8));
            if (!std::all_of(hex.begin(), hex.end(), ::isxdigit)) {
                return {};
            }

            const auto value = static_cast<u32>(std::strtoul(hex.c_str(), nullptr, 0x10));
            out[*current_entry].definition.opcodes[out[*current_entry].definition.num_opcodes++] =
                value;

            i += 8;
        } else {
            return {};
        }
    }

    out[0].enabled = out[0].definition.num_opcodes > 0;
    out[0].cheat_id = 0;

    for (u32 i = 1; i < out.size(); ++i) {
        out[i].enabled = out[i].definition.num_opcodes > 0;
        out[i].cheat_id = i;
    }

    return out;
}

CheatEngine::CheatEngine(System& system_, std::vector<CheatEntry> cheats_,
                         const std::array<u8, 0x20>& build_id_)
    : vm{std::make_unique<StandardVmCallbacks>(system_, metadata)},
      cheats(std::move(cheats_)), core_timing{system_.CoreTiming()}, system{system_} {
    metadata.main_nso_build_id = build_id_;
}

CheatEngine::~CheatEngine() {
    core_timing.UnscheduleEvent(event);
}

void CheatEngine::Initialize() {
    event = Core::Timing::CreateEvent(
        "CheatEngine::FrameCallback::" + Common::HexToString(metadata.main_nso_build_id),
        [this](s64 time,
               std::chrono::nanoseconds ns_late) -> std::optional<std::chrono::nanoseconds> {
            FrameCallback(ns_late);
            return std::nullopt;
        });
    core_timing.ScheduleLoopingEvent(CHEAT_ENGINE_NS, CHEAT_ENGINE_NS, event);

    metadata.process_id = system.ApplicationProcess()->GetProcessId();
    metadata.title_id = system.GetApplicationProcessProgramID();

    const auto& page_table = system.ApplicationProcess()->GetPageTable();
    metadata.heap_extents = {
        .base = GetInteger(page_table.GetHeapRegionStart()),
        .size = page_table.GetHeapRegionSize(),
    };
    metadata.aslr_extents = {
        .base = GetInteger(page_table.GetAliasCodeRegionStart()),
        .size = page_table.GetAliasCodeRegionSize(),
    };
    metadata.alias_extents = {
        .base = GetInteger(page_table.GetAliasRegionStart()),
        .size = page_table.GetAliasRegionSize(),
    };

    is_pending_reload.exchange(true);
}

void CheatEngine::SetMainMemoryParameters(VAddr main_region_begin, u64 main_region_size) {
    metadata.main_nso_extents = {
        .base = main_region_begin,
        .size = main_region_size,
    };
}

void CheatEngine::Reload(std::vector<CheatEntry> reload_cheats) {
    cheats = std::move(reload_cheats);
    is_pending_reload.exchange(true);
}

void CheatEngine::FrameCallback(std::chrono::nanoseconds ns_late) {
    if (is_pending_reload.exchange(false)) {
        vm.LoadProgram(cheats);
    }

    if (vm.GetProgramSize() == 0) {
        return;
    }

    SUDACHI_PROFILE("Cheat", "Engine");

    vm.Execute(metadata);
}

} // namespace Core::Memory
