// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <optional>

#include "core/hle/kernel/k_thread.h"
#include "core/loader/loader.h"

namespace Core {

std::optional<std::string> GetThreadName(const Kernel::KThread* thread);
std::string_view GetThreadWaitReason(const Kernel::KThread* thread);
std::string GetThreadState(const Kernel::KThread* thread);

Loader::AppLoader::Modules FindModules(Kernel::KProcess* process);
Kernel::KProcessAddress GetModuleEnd(const Kernel::KProcess* process, Kernel::KProcessAddress base);
Kernel::KProcessAddress FindMainModuleEntrypoint(Kernel::KProcess* process);

void InvalidateInstructionCacheRange(const Kernel::KProcess* process, u64 address, u64 size);

struct BacktraceEntry {
    std::string module;
    u64 address;
    u64 original_address;
    u64 offset;
    std::string name;
};

std::vector<BacktraceEntry> GetBacktraceFromContext(Kernel::KProcess* process,
                                                    const Kernel::Svc::ThreadContext& ctx);
std::vector<BacktraceEntry> GetBacktrace(const Kernel::KThread* thread);

} // namespace Core
