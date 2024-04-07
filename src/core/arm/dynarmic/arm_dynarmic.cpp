// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#ifdef __linux__

#include "common/signal_chain.h"

#include "core/arm/dynarmic/arm_dynarmic.h"
#include "core/hle/kernel/k_process.h"
#include "core/memory.h"

namespace Core {

namespace {

thread_local Core::Memory::Memory* g_current_memory{};
std::once_flag g_registered{};
struct sigaction g_old_segv {};

void HandleSigSegv(int sig, siginfo_t* info, void* ctx) {
    if (g_current_memory && g_current_memory->InvalidateSeparateHeap(info->si_addr)) {
        return;
    }

    return g_old_segv.sa_sigaction(sig, info, ctx);
}

} // namespace

ScopedJitExecution::ScopedJitExecution(Kernel::KProcess* process) {
    g_current_memory = std::addressof(process->GetMemory());
}

ScopedJitExecution::~ScopedJitExecution() {
    g_current_memory = nullptr;
}

void ScopedJitExecution::RegisterHandler() {
    std::call_once(g_registered, [] {
        struct sigaction sa {};
        sa.sa_sigaction = &HandleSigSegv;
        sa.sa_flags = SA_SIGINFO | SA_ONSTACK;
        Common::SigAction(SIGSEGV, std::addressof(sa), std::addressof(g_old_segv));
    });
}

} // namespace Core

#endif
