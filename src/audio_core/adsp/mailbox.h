// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <span>

#include "common/bounded_threadsafe_queue.h"
#include "common/common_types.h"

namespace AudioCore::ADSP {

enum class AppMailboxId : u32 {
    Invalid = 0,
    AudioRenderer = 50,
    AudioRendererMemoryMapUnmap = 51,
};

enum class Direction : u32 {
    Host,
    DSP,
};

class Mailbox {
public:
    void Initialize(AppMailboxId id_) {
        Reset();
        id = id_;
    }

    AppMailboxId Id() const noexcept {
        return id;
    }

    void Send(Direction dir, u32 message) {
        auto& queue = dir == Direction::Host ? host_queue : adsp_queue;
        queue.EmplaceWait(message);
    }

    u32 Receive(Direction dir, std::stop_token stop_token = {}) {
        auto& queue = dir == Direction::Host ? host_queue : adsp_queue;
        return queue.PopWait(stop_token);
    }

    void Reset() {
        id = AppMailboxId::Invalid;
        u32 t{};
        while (host_queue.TryPop(t)) {
        }
        while (adsp_queue.TryPop(t)) {
        }
    }

private:
    AppMailboxId id{0};
    Common::SPSCQueue<u32> host_queue;
    Common::SPSCQueue<u32> adsp_queue;
};

} // namespace AudioCore::ADSP
