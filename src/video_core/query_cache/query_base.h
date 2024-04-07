// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "common/common_funcs.h"
#include "common/common_types.h"

namespace VideoCommon {

enum class QueryFlagBits : u32 {
    HasTimestamp = 1 << 0,       ///< Indicates if this query has a timestamp.
    IsFinalValueSynced = 1 << 1, ///< Indicates if the query has been synced in the host
    IsHostSynced = 1 << 2,       ///< Indicates if the query has been synced in the host
    IsGuestSynced = 1 << 3,      ///< Indicates if the query has been synced with the guest.
    IsHostManaged = 1 << 4,      ///< Indicates if this query points to a host query
    IsRewritten = 1 << 5,        ///< Indicates if this query was rewritten by another query
    IsInvalidated = 1 << 6,      ///< Indicates the value of th query has been nullified.
    IsOrphan = 1 << 7,           ///< Indicates the query has not been set by a guest query.
    IsFence = 1 << 8,            ///< Indicates the query is a fence.
};
DECLARE_ENUM_FLAG_OPERATORS(QueryFlagBits)

class QueryBase {
public:
    DAddr guest_address{};
    QueryFlagBits flags{};
    u64 value{};

protected:
    // Default constructor
    QueryBase() = default;

    // Parameterized constructor
    QueryBase(DAddr address, QueryFlagBits flags_, u64 value_)
        : guest_address(address), flags(flags_), value{value_} {}
};

class GuestQuery : public QueryBase {
public:
    // Parameterized constructor
    GuestQuery(bool isLong, VAddr address, u64 queryValue)
        : QueryBase(address, QueryFlagBits::IsFinalValueSynced, queryValue) {
        if (isLong) {
            flags |= QueryFlagBits::HasTimestamp;
        }
    }
};

class HostQueryBase : public QueryBase {
public:
    // Default constructor
    HostQueryBase() : QueryBase(0, QueryFlagBits::IsHostManaged | QueryFlagBits::IsOrphan, 0) {}

    // Parameterized constructor
    HostQueryBase(bool has_timestamp, VAddr address)
        : QueryBase(address, QueryFlagBits::IsHostManaged, 0), start_bank_id{}, size_banks{},
          start_slot{}, size_slots{} {
        if (has_timestamp) {
            flags |= QueryFlagBits::HasTimestamp;
        }
    }

    u32 start_bank_id{};
    u32 size_banks{};
    size_t start_slot{};
    size_t size_slots{};
};

} // namespace VideoCommon
