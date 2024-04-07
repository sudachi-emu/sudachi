// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <atomic>
#include <deque>
#include <utility>

#include "common/common_types.h"

namespace VideoCommon {

class BankBase {
protected:
    const size_t base_bank_size{};
    size_t bank_size{};
    std::atomic<size_t> references{};
    size_t current_slot{};

public:
    explicit BankBase(size_t bank_size_) : base_bank_size{bank_size_}, bank_size(bank_size_) {}

    virtual ~BankBase() = default;

    virtual std::pair<bool, size_t> Reserve() {
        if (IsClosed()) {
            return {false, bank_size};
        }
        const size_t result = current_slot++;
        return {true, result};
    }

    virtual void Reset() {
        current_slot = 0;
        references = 0;
        bank_size = base_bank_size;
    }

    size_t Size() const {
        return bank_size;
    }

    void AddReference(size_t how_many = 1) {
        references.fetch_add(how_many, std::memory_order_relaxed);
    }

    void CloseReference(size_t how_many = 1) {
        if (how_many > references.load(std::memory_order_relaxed)) {
            UNREACHABLE();
        }
        references.fetch_sub(how_many, std::memory_order_relaxed);
    }

    void Close() {
        bank_size = current_slot;
    }

    bool IsClosed() const {
        return current_slot >= bank_size;
    }

    bool IsDead() const {
        return IsClosed() && references == 0;
    }
};

template <typename BankType>
class BankPool {
private:
    std::deque<BankType> bank_pool;
    std::deque<size_t> bank_indices;

public:
    BankPool() = default;
    ~BankPool() = default;

    // Reserve a bank from the pool and return its index
    template <typename Func>
    size_t ReserveBank(Func&& builder) {
        if (!bank_indices.empty() && bank_pool[bank_indices.front()].IsDead()) {
            size_t new_index = bank_indices.front();
            bank_indices.pop_front();
            bank_pool[new_index].Reset();
            bank_indices.push_back(new_index);
            return new_index;
        }
        size_t new_index = bank_pool.size();
        builder(bank_pool, new_index);
        bank_indices.push_back(new_index);
        return new_index;
    }

    // Get a reference to a bank using its index
    BankType& GetBank(size_t index) {
        return bank_pool[index];
    }

    // Get the total number of banks in the pool
    size_t BankCount() const {
        return bank_pool.size();
    }
};

} // namespace VideoCommon
