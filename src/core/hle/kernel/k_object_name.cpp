// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/hle/kernel/k_object_name.h"

namespace Kernel {

KObjectNameGlobalData::KObjectNameGlobalData(KernelCore& kernel) : m_object_list_lock{kernel} {}
KObjectNameGlobalData::~KObjectNameGlobalData() = default;

void KObjectName::Initialize(KAutoObject* obj, const char* name) {
    // Set member variables.
    m_object = obj;
    std::strncpy(m_name.data(), name, sizeof(m_name) - 1);
    m_name[sizeof(m_name) - 1] = '\x00';

    // Open a reference to the object we hold.
    m_object->Open();
}

bool KObjectName::MatchesName(const char* name) const {
    return std::strncmp(m_name.data(), name, sizeof(m_name)) == 0;
}

Result KObjectName::NewFromName(KernelCore& kernel, KAutoObject* obj, const char* name) {
    // Create a new object name.
    KObjectName* new_name = KObjectName::Allocate(kernel);
    R_UNLESS(new_name != nullptr, ResultOutOfResource);

    // Initialize the new name.
    new_name->Initialize(obj, name);

    // Check if there's an existing name.
    {
        // Get the global data.
        KObjectNameGlobalData& gd{kernel.ObjectNameGlobalData()};

        // Ensure we have exclusive access to the global list.
        KScopedLightLock lk{gd.GetObjectListLock()};

        // If the object doesn't exist, put it into the list.
        KScopedAutoObject existing_object = FindImpl(kernel, name);
        if (existing_object.IsNull()) {
            gd.GetObjectList().push_back(*new_name);
            R_SUCCEED();
        }
    }

    // The object already exists, which is an error condition. Perform cleanup.
    obj->Close();
    KObjectName::Free(kernel, new_name);
    R_THROW(ResultInvalidState);
}

Result KObjectName::Delete(KernelCore& kernel, KAutoObject* obj, const char* compare_name) {
    // Get the global data.
    KObjectNameGlobalData& gd{kernel.ObjectNameGlobalData()};

    // Ensure we have exclusive access to the global list.
    KScopedLightLock lk{gd.GetObjectListLock()};

    // Find a matching entry in the list, and delete it.
    for (auto& name : gd.GetObjectList()) {
        if (name.MatchesName(compare_name) && obj == name.GetObject()) {
            // We found a match, clean up its resources.
            obj->Close();
            gd.GetObjectList().erase(gd.GetObjectList().iterator_to(name));
            KObjectName::Free(kernel, std::addressof(name));
            R_SUCCEED();
        }
    }

    // We didn't find the object in the list.
    R_THROW(ResultNotFound);
}

KScopedAutoObject<KAutoObject> KObjectName::Find(KernelCore& kernel, const char* name) {
    // Get the global data.
    KObjectNameGlobalData& gd{kernel.ObjectNameGlobalData()};

    // Ensure we have exclusive access to the global list.
    KScopedLightLock lk{gd.GetObjectListLock()};

    return FindImpl(kernel, name);
}

KScopedAutoObject<KAutoObject> KObjectName::FindImpl(KernelCore& kernel, const char* compare_name) {
    // Get the global data.
    KObjectNameGlobalData& gd{kernel.ObjectNameGlobalData()};

    // Try to find a matching object in the global list.
    for (const auto& name : gd.GetObjectList()) {
        if (name.MatchesName(compare_name)) {
            return name.GetObject();
        }
    }

    // There's no matching entry in the list.
    return nullptr;
}

} // namespace Kernel
