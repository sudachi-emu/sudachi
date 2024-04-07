// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/scope_exit.h"
#include "core/core.h"
#include "core/hle/kernel/k_process.h"
#include "core/hle/kernel/k_resource_limit.h"
#include "core/hle/kernel/svc.h"

namespace Kernel::Svc {

Result CreateResourceLimit(Core::System& system, Handle* out_handle) {
    LOG_DEBUG(Kernel_SVC, "called");

    // Create a new resource limit.
    auto& kernel = system.Kernel();
    KResourceLimit* resource_limit = KResourceLimit::Create(kernel);
    R_UNLESS(resource_limit != nullptr, ResultOutOfResource);

    // Ensure we don't leak a reference to the limit.
    SCOPE_EXIT {
        resource_limit->Close();
    };

    // Initialize the resource limit.
    resource_limit->Initialize();

    // Register the limit.
    KResourceLimit::Register(kernel, resource_limit);

    // Add the limit to the handle table.
    R_RETURN(GetCurrentProcess(kernel).GetHandleTable().Add(out_handle, resource_limit));
}

Result GetResourceLimitLimitValue(Core::System& system, s64* out_limit_value,
                                  Handle resource_limit_handle, LimitableResource which) {
    LOG_DEBUG(Kernel_SVC, "called, resource_limit_handle={:08X}, which={}", resource_limit_handle,
              which);

    // Validate the resource.
    R_UNLESS(IsValidResourceType(which), ResultInvalidEnumValue);

    // Get the resource limit.
    KScopedAutoObject resource_limit = GetCurrentProcess(system.Kernel())
                                           .GetHandleTable()
                                           .GetObject<KResourceLimit>(resource_limit_handle);
    R_UNLESS(resource_limit.IsNotNull(), ResultInvalidHandle);

    // Get the limit value.
    *out_limit_value = resource_limit->GetLimitValue(which);

    R_SUCCEED();
}

Result GetResourceLimitCurrentValue(Core::System& system, s64* out_current_value,
                                    Handle resource_limit_handle, LimitableResource which) {
    LOG_DEBUG(Kernel_SVC, "called, resource_limit_handle={:08X}, which={}", resource_limit_handle,
              which);

    // Validate the resource.
    R_UNLESS(IsValidResourceType(which), ResultInvalidEnumValue);

    // Get the resource limit.
    KScopedAutoObject resource_limit = GetCurrentProcess(system.Kernel())
                                           .GetHandleTable()
                                           .GetObject<KResourceLimit>(resource_limit_handle);
    R_UNLESS(resource_limit.IsNotNull(), ResultInvalidHandle);

    // Get the current value.
    *out_current_value = resource_limit->GetCurrentValue(which);

    R_SUCCEED();
}

Result SetResourceLimitLimitValue(Core::System& system, Handle resource_limit_handle,
                                  LimitableResource which, s64 limit_value) {
    LOG_DEBUG(Kernel_SVC, "called, resource_limit_handle={:08X}, which={}, limit_value={}",
              resource_limit_handle, which, limit_value);

    // Validate the resource.
    R_UNLESS(IsValidResourceType(which), ResultInvalidEnumValue);

    // Get the resource limit.
    KScopedAutoObject resource_limit = GetCurrentProcess(system.Kernel())
                                           .GetHandleTable()
                                           .GetObject<KResourceLimit>(resource_limit_handle);
    R_UNLESS(resource_limit.IsNotNull(), ResultInvalidHandle);

    // Set the limit value.
    R_RETURN(resource_limit->SetLimitValue(which, limit_value));
}

Result GetResourceLimitPeakValue(Core::System& system, int64_t* out_peak_value,
                                 Handle resource_limit_handle, LimitableResource which) {
    UNIMPLEMENTED();
    R_THROW(ResultNotImplemented);
}

Result GetResourceLimitLimitValue64(Core::System& system, int64_t* out_limit_value,
                                    Handle resource_limit_handle, LimitableResource which) {
    R_RETURN(GetResourceLimitLimitValue(system, out_limit_value, resource_limit_handle, which));
}

Result GetResourceLimitCurrentValue64(Core::System& system, int64_t* out_current_value,
                                      Handle resource_limit_handle, LimitableResource which) {
    R_RETURN(GetResourceLimitCurrentValue(system, out_current_value, resource_limit_handle, which));
}

Result GetResourceLimitPeakValue64(Core::System& system, int64_t* out_peak_value,
                                   Handle resource_limit_handle, LimitableResource which) {
    R_RETURN(GetResourceLimitPeakValue(system, out_peak_value, resource_limit_handle, which));
}

Result CreateResourceLimit64(Core::System& system, Handle* out_handle) {
    R_RETURN(CreateResourceLimit(system, out_handle));
}

Result SetResourceLimitLimitValue64(Core::System& system, Handle resource_limit_handle,
                                    LimitableResource which, int64_t limit_value) {
    R_RETURN(SetResourceLimitLimitValue(system, resource_limit_handle, which, limit_value));
}

Result GetResourceLimitLimitValue64From32(Core::System& system, int64_t* out_limit_value,
                                          Handle resource_limit_handle, LimitableResource which) {
    R_RETURN(GetResourceLimitLimitValue(system, out_limit_value, resource_limit_handle, which));
}

Result GetResourceLimitCurrentValue64From32(Core::System& system, int64_t* out_current_value,
                                            Handle resource_limit_handle, LimitableResource which) {
    R_RETURN(GetResourceLimitCurrentValue(system, out_current_value, resource_limit_handle, which));
}

Result GetResourceLimitPeakValue64From32(Core::System& system, int64_t* out_peak_value,
                                         Handle resource_limit_handle, LimitableResource which) {
    R_RETURN(GetResourceLimitPeakValue(system, out_peak_value, resource_limit_handle, which));
}

Result CreateResourceLimit64From32(Core::System& system, Handle* out_handle) {
    R_RETURN(CreateResourceLimit(system, out_handle));
}

Result SetResourceLimitLimitValue64From32(Core::System& system, Handle resource_limit_handle,
                                          LimitableResource which, int64_t limit_value) {
    R_RETURN(SetResourceLimitLimitValue(system, resource_limit_handle, which, limit_value));
}

} // namespace Kernel::Svc
