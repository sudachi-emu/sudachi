// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <compare>
#include <type_traits>
#include <fmt/format.h>

#include "common/common_types.h"

namespace Common {

template <bool Virtual, typename T>
class TypedAddress {
public:
    // Constructors.
    constexpr inline TypedAddress() : m_address(0) {}
    constexpr inline TypedAddress(uint64_t a) : m_address(a) {}

    template <typename U>
    constexpr inline explicit TypedAddress(const U* ptr)
        : m_address(reinterpret_cast<uint64_t>(ptr)) {}

    // Copy constructor.
    constexpr inline TypedAddress(const TypedAddress& rhs) = default;

    // Assignment operator.
    constexpr inline TypedAddress& operator=(const TypedAddress& rhs) = default;

    // Arithmetic operators.
    template <typename I>
    constexpr inline TypedAddress operator+(I rhs) const {
        static_assert(std::is_integral_v<I>);
        return m_address + rhs;
    }

    constexpr inline TypedAddress operator+(TypedAddress rhs) const {
        return m_address + rhs.m_address;
    }

    constexpr inline TypedAddress operator++() {
        return ++m_address;
    }

    constexpr inline TypedAddress operator++(int) {
        return m_address++;
    }

    template <typename I>
    constexpr inline TypedAddress operator-(I rhs) const {
        static_assert(std::is_integral_v<I>);
        return m_address - rhs;
    }

    constexpr inline ptrdiff_t operator-(TypedAddress rhs) const {
        return m_address - rhs.m_address;
    }

    constexpr inline TypedAddress operator--() {
        return --m_address;
    }

    constexpr inline TypedAddress operator--(int) {
        return m_address--;
    }

    template <typename I>
    constexpr inline TypedAddress operator+=(I rhs) {
        static_assert(std::is_integral_v<I>);
        m_address += rhs;
        return *this;
    }

    template <typename I>
    constexpr inline TypedAddress operator-=(I rhs) {
        static_assert(std::is_integral_v<I>);
        m_address -= rhs;
        return *this;
    }

    // Logical operators.
    constexpr inline uint64_t operator&(uint64_t mask) const {
        return m_address & mask;
    }

    constexpr inline uint64_t operator|(uint64_t mask) const {
        return m_address | mask;
    }

    template <typename I>
    constexpr inline TypedAddress operator|=(I rhs) {
        static_assert(std::is_integral_v<I>);
        m_address |= rhs;
        return *this;
    }

    constexpr inline uint64_t operator<<(int shift) const {
        return m_address << shift;
    }

    constexpr inline uint64_t operator>>(int shift) const {
        return m_address >> shift;
    }

    template <typename U>
    constexpr inline size_t operator/(U size) const {
        return m_address / size;
    }

    constexpr explicit operator bool() const {
        return m_address != 0;
    }

    // constexpr inline uint64_t operator%(U align) const { return m_address % align; }

    // Comparison operators.
    constexpr bool operator==(const TypedAddress&) const = default;
    constexpr auto operator<=>(const TypedAddress&) const = default;

    // For convenience, also define comparison operators versus uint64_t.
    constexpr inline bool operator==(uint64_t rhs) const {
        return m_address == rhs;
    }

    // Allow getting the address explicitly, for use in accessors.
    constexpr inline uint64_t GetValue() const {
        return m_address;
    }

private:
    uint64_t m_address{};
};

struct PhysicalAddressTag {};
struct VirtualAddressTag {};
struct ProcessAddressTag {};

using PhysicalAddress = TypedAddress<false, PhysicalAddressTag>;
using VirtualAddress = TypedAddress<true, VirtualAddressTag>;
using ProcessAddress = TypedAddress<true, ProcessAddressTag>;

// Define accessors.
template <typename T>
concept IsTypedAddress = std::same_as<T, PhysicalAddress> || std::same_as<T, VirtualAddress> ||
                         std::same_as<T, ProcessAddress>;

template <typename T>
constexpr inline T Null = [] {
    if constexpr (std::is_same<T, uint64_t>::value) {
        return 0;
    } else {
        static_assert(std::is_same<T, PhysicalAddress>::value ||
                      std::is_same<T, VirtualAddress>::value ||
                      std::is_same<T, ProcessAddress>::value);
        return T(0);
    }
}();

// Basic type validations.
static_assert(sizeof(PhysicalAddress) == sizeof(uint64_t));
static_assert(sizeof(VirtualAddress) == sizeof(uint64_t));
static_assert(sizeof(ProcessAddress) == sizeof(uint64_t));

static_assert(std::is_trivially_copyable_v<PhysicalAddress>);
static_assert(std::is_trivially_copyable_v<VirtualAddress>);
static_assert(std::is_trivially_copyable_v<ProcessAddress>);

static_assert(std::is_trivially_copy_constructible_v<PhysicalAddress>);
static_assert(std::is_trivially_copy_constructible_v<VirtualAddress>);
static_assert(std::is_trivially_copy_constructible_v<ProcessAddress>);

static_assert(std::is_trivially_move_constructible_v<PhysicalAddress>);
static_assert(std::is_trivially_move_constructible_v<VirtualAddress>);
static_assert(std::is_trivially_move_constructible_v<ProcessAddress>);

static_assert(std::is_trivially_copy_assignable_v<PhysicalAddress>);
static_assert(std::is_trivially_copy_assignable_v<VirtualAddress>);
static_assert(std::is_trivially_copy_assignable_v<ProcessAddress>);

static_assert(std::is_trivially_move_assignable_v<PhysicalAddress>);
static_assert(std::is_trivially_move_assignable_v<VirtualAddress>);
static_assert(std::is_trivially_move_assignable_v<ProcessAddress>);

static_assert(std::is_trivially_destructible_v<PhysicalAddress>);
static_assert(std::is_trivially_destructible_v<VirtualAddress>);
static_assert(std::is_trivially_destructible_v<ProcessAddress>);

static_assert(Null<uint64_t> == 0U);
static_assert(Null<PhysicalAddress> == Null<uint64_t>);
static_assert(Null<VirtualAddress> == Null<uint64_t>);
static_assert(Null<ProcessAddress> == Null<uint64_t>);

// Constructor/assignment validations.
static_assert([] {
    const PhysicalAddress a(5U);
    PhysicalAddress b(a);
    return b;
}() == PhysicalAddress(5U));
static_assert([] {
    const PhysicalAddress a(5U);
    PhysicalAddress b(10U);
    b = a;
    return b;
}() == PhysicalAddress(5U));

// Arithmetic validations.
static_assert(PhysicalAddress(10U) + 5U == PhysicalAddress(15U));
static_assert(PhysicalAddress(10U) - 5U == PhysicalAddress(5U));
static_assert([] {
    PhysicalAddress v(10U);
    v += 5U;
    return v;
}() == PhysicalAddress(15U));
static_assert([] {
    PhysicalAddress v(10U);
    v -= 5U;
    return v;
}() == PhysicalAddress(5U));
static_assert(PhysicalAddress(10U)++ == PhysicalAddress(10U));
static_assert(++PhysicalAddress(10U) == PhysicalAddress(11U));
static_assert(PhysicalAddress(10U)-- == PhysicalAddress(10U));
static_assert(--PhysicalAddress(10U) == PhysicalAddress(9U));

// Logical validations.
static_assert((PhysicalAddress(0b11111111U) >> 1) == 0b01111111U);
static_assert((PhysicalAddress(0b10101010U) >> 1) == 0b01010101U);
static_assert((PhysicalAddress(0b11111111U) << 1) == 0b111111110U);
static_assert((PhysicalAddress(0b01010101U) << 1) == 0b10101010U);
static_assert((PhysicalAddress(0b11111111U) & 0b01010101U) == 0b01010101U);
static_assert((PhysicalAddress(0b11111111U) & 0b10101010U) == 0b10101010U);
static_assert((PhysicalAddress(0b01010101U) & 0b10101010U) == 0b00000000U);
static_assert((PhysicalAddress(0b00000000U) | 0b01010101U) == 0b01010101U);
static_assert((PhysicalAddress(0b11111111U) | 0b01010101U) == 0b11111111U);
static_assert((PhysicalAddress(0b10101010U) | 0b01010101U) == 0b11111111U);

// Comparisons.
static_assert(PhysicalAddress(0U) == PhysicalAddress(0U));
static_assert(PhysicalAddress(0U) != PhysicalAddress(1U));
static_assert(PhysicalAddress(0U) < PhysicalAddress(1U));
static_assert(PhysicalAddress(0U) <= PhysicalAddress(1U));
static_assert(PhysicalAddress(1U) > PhysicalAddress(0U));
static_assert(PhysicalAddress(1U) >= PhysicalAddress(0U));

static_assert(!(PhysicalAddress(0U) == PhysicalAddress(1U)));
static_assert(!(PhysicalAddress(0U) != PhysicalAddress(0U)));
static_assert(!(PhysicalAddress(1U) < PhysicalAddress(0U)));
static_assert(!(PhysicalAddress(1U) <= PhysicalAddress(0U)));
static_assert(!(PhysicalAddress(0U) > PhysicalAddress(1U)));
static_assert(!(PhysicalAddress(0U) >= PhysicalAddress(1U)));

} // namespace Common

template <bool Virtual, typename T>
constexpr inline uint64_t GetInteger(Common::TypedAddress<Virtual, T> address) {
    return address.GetValue();
}

template <>
struct fmt::formatter<Common::PhysicalAddress> {
    constexpr auto parse(fmt::format_parse_context& ctx) {
        return ctx.begin();
    }
    template <typename FormatContext>
    auto format(const Common::PhysicalAddress& addr, FormatContext& ctx) {
        return fmt::format_to(ctx.out(), "{:#x}", static_cast<u64>(addr.GetValue()));
    }
};

template <>
struct fmt::formatter<Common::ProcessAddress> {
    constexpr auto parse(fmt::format_parse_context& ctx) {
        return ctx.begin();
    }
    template <typename FormatContext>
    auto format(const Common::ProcessAddress& addr, FormatContext& ctx) {
        return fmt::format_to(ctx.out(), "{:#x}", static_cast<u64>(addr.GetValue()));
    }
};

template <>
struct fmt::formatter<Common::VirtualAddress> {
    constexpr auto parse(fmt::format_parse_context& ctx) {
        return ctx.begin();
    }
    template <typename FormatContext>
    auto format(const Common::VirtualAddress& addr, FormatContext& ctx) {
        return fmt::format_to(ctx.out(), "{:#x}", static_cast<u64>(addr.GetValue()));
    }
};

namespace std {

template <>
struct hash<Common::PhysicalAddress> {
    size_t operator()(const Common::PhysicalAddress& k) const noexcept {
        return k.GetValue();
    }
};

template <>
struct hash<Common::ProcessAddress> {
    size_t operator()(const Common::ProcessAddress& k) const noexcept {
        return k.GetValue();
    }
};

template <>
struct hash<Common::VirtualAddress> {
    size_t operator()(const Common::VirtualAddress& k) const noexcept {
        return k.GetValue();
    }
};

} // namespace std
