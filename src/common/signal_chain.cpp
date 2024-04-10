// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <dlfcn.h>

#include "common/assert.h"
#include "common/dynamic_library.h"
#include "common/scope_exit.h"
#include "common/signal_chain.h"

namespace Common {

template <typename T>
T* LookupLibcSymbol(const char* name) {
#if defined(__BIONIC__)
    Common::DynamicLibrary provider("libc.so");
    if (!provider.IsOpen()) {
        UNREACHABLE_MSG("Failed to open libc!");
    }
#else
    // For other operating environments, we assume the symbol is not overridden.
    const char* base = nullptr;
    Common::DynamicLibrary provider(base);
#endif

    void* sym = provider.GetSymbolAddress(name);
    if (sym == nullptr) {
        sym = dlsym(RTLD_DEFAULT, name);
    }
    if (sym == nullptr) {
        UNREACHABLE_MSG("Unable to find symbol {}!", name);
    }

    return reinterpret_cast<T*>(sym);
}

int SigAction(int signum, const struct sigaction* act, struct sigaction* oldact) {
    static auto libc_sigaction = LookupLibcSymbol<decltype(sigaction)>("sigaction");
    return libc_sigaction(signum, act, oldact);
}

} // namespace Common
