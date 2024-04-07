// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <span>
#include <vector>

#include "common/concepts.h"
#include "core/hle/service/nvdrv/devices/nvdevice.h"

namespace Service::Nvidia::Devices {

struct IoctlOneArgTraits {
    template <typename T, typename R, typename A, typename... B>
    static A GetFirstArgImpl(R (T::*)(A, B...));
};

struct IoctlTwoArgTraits {
    template <typename T, typename R, typename A, typename B, typename... C>
    static A GetFirstArgImpl(R (T::*)(A, B, C...));

    template <typename T, typename R, typename A, typename B, typename... C>
    static B GetSecondArgImpl(R (T::*)(A, B, C...));
};

struct Null {};

// clang-format off

template <typename FixedArg, typename VarArg, typename InlInVarArg, typename InlOutVarArg, typename F>
NvResult WrapGeneric(F&& callable, std::span<const u8> input, std::span<const u8> inline_input, std::span<u8> output, std::span<u8> inline_output) {
    constexpr bool HasFixedArg     = !std::is_same_v<FixedArg, Null>;
    constexpr bool HasVarArg       = !std::is_same_v<VarArg, Null>;
    constexpr bool HasInlInVarArg  = !std::is_same_v<InlInVarArg, Null>;
    constexpr bool HasInlOutVarArg = !std::is_same_v<InlOutVarArg, Null>;

    // Declare the fixed-size input value.
    FixedArg fixed{};
    size_t var_offset = 0;

    if constexpr (HasFixedArg) {
        // Read the fixed-size input value.
        var_offset = std::min(sizeof(FixedArg), input.size());
        if (var_offset > 0) {
            std::memcpy(&fixed, input.data(), var_offset);
        }
    }

    // Read the variable-sized inputs.
    const size_t num_var_args = HasVarArg ? ((input.size() - var_offset) / sizeof(VarArg)) : 0;
    std::vector<VarArg> var_args(num_var_args);
    if constexpr (HasVarArg) {
        if (num_var_args > 0) {
            std::memcpy(var_args.data(), input.data() + var_offset, num_var_args * sizeof(VarArg));
        }
    }

    const size_t num_inl_in_var_args = HasInlInVarArg ? (inline_input.size() / sizeof(InlInVarArg)) : 0;
    std::vector<InlInVarArg> inl_in_var_args(num_inl_in_var_args);
    if constexpr (HasInlInVarArg) {
        if (num_inl_in_var_args > 0) {
            std::memcpy(inl_in_var_args.data(), inline_input.data(), num_inl_in_var_args * sizeof(InlInVarArg));
        }
    }

    // Construct inline output data.
    const size_t num_inl_out_var_args = HasInlOutVarArg ? (inline_output.size() / sizeof(InlOutVarArg)) : 0;
    std::vector<InlOutVarArg> inl_out_var_args(num_inl_out_var_args);

    // Perform the call.
    NvResult result = callable(fixed, var_args, inl_in_var_args, inl_out_var_args);

    // Copy outputs.
    if constexpr (HasFixedArg) {
        if (output.size() > 0) {
            std::memcpy(output.data(), &fixed, std::min(output.size(), sizeof(FixedArg)));
        }
    }

    if constexpr (HasVarArg) {
        if (num_var_args > 0 && output.size() > var_offset) {
            const size_t max_var_size = output.size() - var_offset;
            std::memcpy(output.data() + var_offset, var_args.data(), std::min(max_var_size, num_var_args * sizeof(VarArg)));
        }
    }

    // Copy inline outputs.
    if constexpr (HasInlOutVarArg) {
        if (num_inl_out_var_args > 0) {
            std::memcpy(inline_output.data(), inl_out_var_args.data(), num_inl_out_var_args * sizeof(InlOutVarArg));
        }
    }

    // We're done.
    return result;
}

template <typename Self, typename F, typename... Rest>
NvResult WrapFixed(Self* self, F&& callable, std::span<const u8> input, std::span<u8> output, Rest&&... rest) {
    using FixedArg = typename std::remove_reference_t<decltype(IoctlOneArgTraits::GetFirstArgImpl(callable))>;

    const auto Callable = [&](auto& fixed, auto& var, auto& inl_in, auto& inl_out) -> NvResult {
        return (self->*callable)(fixed, std::forward<Rest>(rest)...);
    };

    return WrapGeneric<FixedArg, Null, Null, Null>(std::move(Callable), input, {}, output, {});
}

template <typename Self, typename F, typename... Rest>
NvResult WrapFixedInlOut(Self* self, F&& callable, std::span<const u8> input, std::span<u8> output, std::span<u8> inline_output, Rest&&... rest) {
    using FixedArg     = typename std::remove_reference_t<decltype(IoctlTwoArgTraits::GetFirstArgImpl(callable))>;
    using InlOutVarArg = typename std::remove_reference_t<decltype(IoctlTwoArgTraits::GetSecondArgImpl(callable))>::value_type;

    const auto Callable = [&](auto& fixed, auto& var, auto& inl_in, auto& inl_out) -> NvResult {
        return (self->*callable)(fixed, inl_out, std::forward<Rest>(rest)...);
    };

    return WrapGeneric<FixedArg, Null, Null, InlOutVarArg>(std::move(Callable), input, {}, output, inline_output);
}

template <typename Self, typename F, typename... Rest>
NvResult WrapVariable(Self* self, F&& callable, std::span<const u8> input, std::span<u8> output, Rest&&... rest) {
    using VarArg = typename std::remove_reference_t<decltype(IoctlOneArgTraits::GetFirstArgImpl(callable))>::value_type;

    const auto Callable = [&](auto& fixed, auto& var, auto& inl_in, auto& inl_out) -> NvResult {
        return (self->*callable)(var, std::forward<Rest>(rest)...);
    };

    return WrapGeneric<Null, VarArg, Null, Null>(std::move(Callable), input, {}, output, {});
}

template <typename Self, typename F, typename... Rest>
NvResult WrapFixedVariable(Self* self, F&& callable, std::span<const u8> input, std::span<u8> output, Rest&&... rest) {
    using FixedArg = typename std::remove_reference_t<decltype(IoctlTwoArgTraits::GetFirstArgImpl(callable))>;
    using VarArg   = typename std::remove_reference_t<decltype(IoctlTwoArgTraits::GetSecondArgImpl(callable))>::value_type;

    const auto Callable = [&](auto& fixed, auto& var, auto& inl_in, auto& inl_out) -> NvResult {
        return (self->*callable)(fixed, var, std::forward<Rest>(rest)...);
    };

    return WrapGeneric<FixedArg, VarArg, Null, Null>(std::move(Callable), input, {}, output, {});
}

template <typename Self, typename F, typename... Rest>
NvResult WrapFixedInlIn(Self* self, F&& callable, std::span<const u8> input, std::span<const u8> inline_input, std::span<u8> output, Rest&&... rest) {
    using FixedArg    = typename std::remove_reference_t<decltype(IoctlTwoArgTraits::GetFirstArgImpl(callable))>;
    using InlInVarArg = typename std::remove_reference_t<decltype(IoctlTwoArgTraits::GetSecondArgImpl(callable))>::value_type;

    const auto Callable = [&](auto& fixed, auto& var, auto& inl_in, auto& inl_out) -> NvResult {
        return (self->*callable)(fixed, inl_in, std::forward<Rest>(rest)...);
    };

    return WrapGeneric<FixedArg, Null, InlInVarArg, Null>(std::move(Callable), input, inline_input, output, {});
}

// clang-format on

} // namespace Service::Nvidia::Devices
