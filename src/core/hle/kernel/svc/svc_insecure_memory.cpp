// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/hle/kernel/svc.h"
#include "core/hle/kernel/svc_results.h"

namespace Kernel::Svc {

Result MapInsecureMemory(Core::System& system, uint64_t address, uint64_t size) {
    UNIMPLEMENTED();
    R_THROW(ResultNotImplemented);
}

Result UnmapInsecureMemory(Core::System& system, uint64_t address, uint64_t size) {
    UNIMPLEMENTED();
    R_THROW(ResultNotImplemented);
}

Result MapInsecureMemory64(Core::System& system, uint64_t address, uint64_t size) {
    R_RETURN(MapInsecureMemory(system, address, size));
}

Result UnmapInsecureMemory64(Core::System& system, uint64_t address, uint64_t size) {
    R_RETURN(UnmapInsecureMemory(system, address, size));
}

Result MapInsecureMemory64From32(Core::System& system, uint32_t address, uint32_t size) {
    R_RETURN(MapInsecureMemory(system, address, size));
}

Result UnmapInsecureMemory64From32(Core::System& system, uint32_t address, uint32_t size) {
    R_RETURN(UnmapInsecureMemory(system, address, size));
}

} // namespace Kernel::Svc
