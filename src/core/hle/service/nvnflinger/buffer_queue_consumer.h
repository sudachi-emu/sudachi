// SPDX-FileCopyrightText: Copyright 2021 sudachi Emulator Project
// SPDX-FileCopyrightText: Copyright 2014 The Android Open Source Project
// SPDX-License-Identifier: GPL-3.0-or-later
// Parts of this implementation were based on:
// https://cs.android.com/android/platform/superproject/+/android-5.1.1_r38:frameworks/native/include/gui/BufferQueueConsumer.h

#pragma once

#include <chrono>
#include <memory>

#include "common/common_types.h"
#include "core/hle/service/nvnflinger/binder.h"
#include "core/hle/service/nvnflinger/buffer_queue_defs.h"
#include "core/hle/service/nvnflinger/status.h"

namespace Service {
namespace Time {
namespace Clock {
struct TimeSpanType;
} // namespace Clock
} // namespace Time
} // namespace Service

namespace Service::android {

class BufferItem;
class BufferQueueCore;
class IConsumerListener;

class BufferQueueConsumer final : public IBinder {
public:
    explicit BufferQueueConsumer(std::shared_ptr<BufferQueueCore> core_);
    ~BufferQueueConsumer() override;

    Status AcquireBuffer(BufferItem* out_buffer, std::chrono::nanoseconds expected_present);
    Status ReleaseBuffer(s32 slot, u64 frame_number, const Fence& release_fence);
    Status Connect(std::shared_ptr<IConsumerListener> consumer_listener, bool controlled_by_app);
    Status Disconnect();
    Status GetReleasedBuffers(u64* out_slot_mask);
    Status SetPresentTime(s32 slot, u64 frame_number, Time::Clock::TimeSpanType presentation_time);

    void Transact(u32 code, std::span<const u8> parcel_data, std::span<u8> parcel_reply,
                  u32 flags) override;

    Kernel::KReadableEvent* GetNativeHandle(u32 type_id) override;

private:
    std::shared_ptr<BufferQueueCore> core;
    BufferQueueDefs::SlotsType& slots;
};

} // namespace Service::android
