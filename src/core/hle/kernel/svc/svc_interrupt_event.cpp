// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/hle/kernel/svc.h"
#include "core/hle/kernel/svc_results.h"

namespace Kernel::Svc {

Result CreateInterruptEvent(Core::System& system, Handle* out, int32_t interrupt_id,
                            InterruptType type) {
    UNIMPLEMENTED();
    R_THROW(ResultNotImplemented);
}

Result CreateInterruptEvent64(Core::System& system, Handle* out_read_handle, int32_t interrupt_id,
                              InterruptType interrupt_type) {
    R_RETURN(CreateInterruptEvent(system, out_read_handle, interrupt_id, interrupt_type));
}

Result CreateInterruptEvent64From32(Core::System& system, Handle* out_read_handle,
                                    int32_t interrupt_id, InterruptType interrupt_type) {
    R_RETURN(CreateInterruptEvent(system, out_read_handle, interrupt_id, interrupt_type));
}

} // namespace Kernel::Svc
