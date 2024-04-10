// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/hle/kernel/svc.h"
#include "core/hle/kernel/svc_results.h"

namespace Kernel::Svc {

Result ReadWriteRegister(Core::System& system, uint32_t* out, uint64_t address, uint32_t mask,
                         uint32_t value) {
    *out = 0;

    UNIMPLEMENTED();
    R_THROW(ResultNotImplemented);
}

Result ReadWriteRegister64(Core::System& system, uint32_t* out_value, uint64_t address,
                           uint32_t mask, uint32_t value) {
    R_RETURN(ReadWriteRegister(system, out_value, address, mask, value));
}

Result ReadWriteRegister64From32(Core::System& system, uint32_t* out_value, uint64_t address,
                                 uint32_t mask, uint32_t value) {
    R_RETURN(ReadWriteRegister(system, out_value, address, mask, value));
}

} // namespace Kernel::Svc
