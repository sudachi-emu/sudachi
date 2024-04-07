// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <algorithm>
#include <type_traits>
#include "bit_cast.h"

namespace Common {

template <typename T>
    requires(std::is_integral_v<T> && std::is_signed_v<T>)
inline T WrappingAdd(T lhs, T rhs) {
    using U = std::make_unsigned_t<T>;

    U lhs_u = BitCast<U>(lhs);
    U rhs_u = BitCast<U>(rhs);

    return BitCast<T>(lhs_u + rhs_u);
}

template <typename T>
    requires(std::is_integral_v<T> && std::is_signed_v<T>)
inline bool CanAddWithoutOverflow(T lhs, T rhs) {
#ifdef _MSC_VER
    if (lhs >= 0 && rhs >= 0) {
        return WrappingAdd(lhs, rhs) >= std::max(lhs, rhs);
    } else if (lhs < 0 && rhs < 0) {
        return WrappingAdd(lhs, rhs) <= std::min(lhs, rhs);
    } else {
        return true;
    }
#else
    T res;
    return !__builtin_add_overflow(lhs, rhs, &res);
#endif
}

} // namespace Common
