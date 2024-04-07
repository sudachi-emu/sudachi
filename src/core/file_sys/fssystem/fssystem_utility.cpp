// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/file_sys/fssystem/fssystem_utility.h"

namespace FileSys {

void AddCounter(void* counter_, size_t counter_size, u64 value) {
    u8* counter = static_cast<u8*>(counter_);
    u64 remaining = value;
    u8 carry = 0;

    for (size_t i = 0; i < counter_size; i++) {
        auto sum = counter[counter_size - 1 - i] + (remaining & 0xFF) + carry;
        carry = static_cast<u8>(sum >> (sizeof(u8) * 8));
        auto sum8 = static_cast<u8>(sum & 0xFF);

        counter[counter_size - 1 - i] = sum8;

        remaining >>= (sizeof(u8) * 8);
        if (carry == 0 && remaining == 0) {
            break;
        }
    }
}

} // namespace FileSys
