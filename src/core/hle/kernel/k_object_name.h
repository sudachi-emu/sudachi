// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <memory>

#include "common/intrusive_list.h"

#include "core/hle/kernel/k_light_lock.h"
#include "core/hle/kernel/slab_helpers.h"
#include "core/hle/kernel/svc_results.h"

namespace Kernel {

class KObjectNameGlobalData;

class KObjectName : public KSlabAllocated<KObjectName>,
                    public Common::IntrusiveListBaseNode<KObjectName> {
public:
    explicit KObjectName(KernelCore&) {}
    virtual ~KObjectName() = default;

    static constexpr size_t NameLengthMax = 12;
    using List = Common::IntrusiveListBaseTraits<KObjectName>::ListType;

    static Result NewFromName(KernelCore& kernel, KAutoObject* obj, const char* name);
    static Result Delete(KernelCore& kernel, KAutoObject* obj, const char* name);

    static KScopedAutoObject<KAutoObject> Find(KernelCore& kernel, const char* name);

    template <typename Derived>
    static Result Delete(KernelCore& kernel, const char* name) {
        // Find the object.
        KScopedAutoObject obj = Find(kernel, name);
        R_UNLESS(obj.IsNotNull(), ResultNotFound);

        // Cast the object to the desired type.
        Derived* derived = obj->DynamicCast<Derived*>();
        R_UNLESS(derived != nullptr, ResultNotFound);

        // Check that the object is closed.
        R_UNLESS(derived->IsServerClosed(), ResultInvalidState);

        R_RETURN(Delete(kernel, obj.GetPointerUnsafe(), name));
    }

    template <typename Derived>
        requires(std::derived_from<Derived, KAutoObject>)
    static KScopedAutoObject<Derived> Find(KernelCore& kernel, const char* name) {
        return Find(kernel, name);
    }

private:
    static KScopedAutoObject<KAutoObject> FindImpl(KernelCore& kernel, const char* name);

    void Initialize(KAutoObject* obj, const char* name);

    bool MatchesName(const char* name) const;
    KAutoObject* GetObject() const {
        return m_object;
    }

private:
    std::array<char, NameLengthMax> m_name{};
    KAutoObject* m_object{};
};

class KObjectNameGlobalData {
public:
    explicit KObjectNameGlobalData(KernelCore& kernel);
    ~KObjectNameGlobalData();

    KLightLock& GetObjectListLock() {
        return m_object_list_lock;
    }

    KObjectName::List& GetObjectList() {
        return m_object_list;
    }

private:
    KLightLock m_object_list_lock;
    KObjectName::List m_object_list;
};

} // namespace Kernel
