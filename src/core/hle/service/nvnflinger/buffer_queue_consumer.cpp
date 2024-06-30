// SPDX-FileCopyrightText: Copyright 2021 sudachi Emulator Project
// SPDX-FileCopyrightText: Copyright 2014 The Android Open Source Project
// SPDX-License-Identifier: GPL-3.0-or-later
// Parts of this implementation were based on:
// https://cs.android.com/android/platform/superproject/+/android-5.1.1_r38:frameworks/native/libs/gui/BufferQueueConsumer.cpp

#include "common/assert.h"
#include "common/logging/log.h"
#include "core/hle/service/nvnflinger/buffer_item.h"
#include "core/hle/service/nvnflinger/buffer_queue_consumer.h"
#include "core/hle/service/nvnflinger/buffer_queue_core.h"
#include "core/hle/service/nvnflinger/parcel.h"
#include "core/hle/service/nvnflinger/producer_listener.h"

namespace Service::android {

BufferQueueConsumer::BufferQueueConsumer(std::shared_ptr<BufferQueueCore> core_)
    : core{std::move(core_)}, slots{core->slots} {}

BufferQueueConsumer::~BufferQueueConsumer() = default;

Status BufferQueueConsumer::AcquireBuffer(BufferItem* out_buffer,
                                          std::chrono::nanoseconds expected_present) {
    std::scoped_lock lock{core->mutex};

    // Check that the consumer doesn't currently have the maximum number of buffers acquired.
    const s32 num_acquired_buffers{
        static_cast<s32>(std::count_if(slots.begin(), slots.end(), [](const auto& slot) {
            return slot.buffer_state == BufferState::Acquired;
        }))};

    if (num_acquired_buffers >= core->max_acquired_buffer_count + 1) {
        LOG_ERROR(Service_Nvnflinger, "max acquired buffer count reached: {} (max {})",
                  num_acquired_buffers, core->max_acquired_buffer_count);
        return Status::InvalidOperation;
    }

    // Check if the queue is empty.
    if (core->queue.empty()) {
        return Status::NoBufferAvailable;
    }

    auto front(core->queue.begin());

    // If expected_present is specified, we may not want to return a buffer yet.
    if (expected_present.count() != 0) {
        constexpr auto MAX_REASONABLE_NSEC = 1000000000LL; // 1 second

        // The expected_present argument indicates when the buffer is expected to be presented
        // on-screen.
        while (core->queue.size() > 1 && !core->queue[0].is_auto_timestamp) {
            const auto& buffer_item{core->queue[1]};

            // If entry[1] is timely, drop entry[0] (and repeat).
            const auto desired_present = buffer_item.timestamp;
            if (desired_present < expected_present.count() - MAX_REASONABLE_NSEC ||
                desired_present > expected_present.count()) {
                // This buffer is set to display in the near future, or desired_present is garbage.
                LOG_DEBUG(Service_Nvnflinger, "nodrop desire={} expect={}", desired_present,
                          expected_present.count());
                break;
            }

            LOG_DEBUG(Service_Nvnflinger, "drop desire={} expect={} size={}", desired_present,
                      expected_present.count(), core->queue.size());

            if (core->StillTracking(*front)) {
                // Front buffer is still in mSlots, so mark the slot as free
                slots[front->slot].buffer_state = BufferState::Free;
            }

            core->queue.erase(front);
            front = core->queue.begin();
        }

        // See if the front buffer is ready to be acquired.
        const auto desired_present = front->timestamp;
        if (desired_present > expected_present.count() &&
            desired_present < expected_present.count() + MAX_REASONABLE_NSEC) {
            LOG_DEBUG(Service_Nvnflinger, "defer desire={} expect={}", desired_present,
                      expected_present.count());
            return Status::PresentLater;
        }

        LOG_DEBUG(Service_Nvnflinger, "accept desire={} expect={}", desired_present,
                  expected_present.count());
    }

    const auto slot = front->slot;
    *out_buffer = *front;

    LOG_DEBUG(Service_Nvnflinger, "acquiring slot={}", slot);

    // If the front buffer is still being tracked, update its slot state
    if (core->StillTracking(*front)) {
        slots[slot].acquire_called = true;
        slots[slot].needs_cleanup_on_release = false;
        slots[slot].buffer_state = BufferState::Acquired;

        // TODO: for now, avoid resetting the fence, so that when we next return this
        // slot to the producer, it will wait for the fence to pass. We should fix this
        // by properly waiting for the fence in the BufferItemConsumer.
        // slots[slot].fence = Fence::NoFence();

        const auto target_frame_number = slots[slot].frame_number;
        for (int i = 0; i < core->history.size(); i++) {
            if (core->history[i].frame_number = target_frame_number) {
                core->history[i].state = BufferState::Acquired;
                break;
            }
        }
    }

    // If the buffer has previously been acquired by the consumer, set graphic_buffer to nullptr to
    // avoid unnecessarily remapping this buffer on the consumer side.
    if (out_buffer->acquire_called) {
        out_buffer->graphic_buffer = nullptr;
    }

    core->queue.erase(front);

    // We might have freed a slot while dropping old buffers, or the producer  may be blocked
    // waiting for the number of buffers in the queue to decrease.
    core->SignalDequeueCondition();

    return Status::NoError;
}

Status BufferQueueConsumer::ReleaseBuffer(s32 slot, u64 frame_number, const Fence& release_fence) {
    if (slot < 0 || slot >= BufferQueueDefs::NUM_BUFFER_SLOTS) {
        LOG_ERROR(Service_Nvnflinger, "slot {} out of range", slot);
        return Status::BadValue;
    }

    std::shared_ptr<IProducerListener> listener;
    {
        std::scoped_lock lock{core->mutex};

        // If the frame number has changed because the buffer has been reallocated, we can ignore
        // this ReleaseBuffer for the old buffer.
        if (frame_number != slots[slot].frame_number) {
            return Status::StaleBufferSlot;
        }

        // Make sure this buffer hasn't been queued while acquired by the consumer.
        auto current(core->queue.begin());
        while (current != core->queue.end()) {
            if (current->slot == slot) {
                LOG_ERROR(Service_Nvnflinger, "buffer slot {} pending release is currently queued",
                          slot);
                return Status::BadValue;
            }
            ++current;
        }

        if (slots[slot].buffer_state == BufferState::Acquired) {
            // TODO: for now, avoid resetting the fence, so that when we next return this
            // slot to the producer, it can wait for its own fence to pass. We should fix this
            // by properly waiting for the fence in the BufferItemConsumer.
            // slots[slot].fence = release_fence;
            slots[slot].buffer_state = BufferState::Free;

            listener = core->connected_producer_listener;

            LOG_DEBUG(Service_Nvnflinger, "releasing slot {}", slot);
        } else if (slots[slot].needs_cleanup_on_release) {
            LOG_DEBUG(Service_Nvnflinger, "releasing a stale buffer slot {} (state = {})", slot,
                      slots[slot].buffer_state);
            slots[slot].needs_cleanup_on_release = false;
            return Status::StaleBufferSlot;
        } else {
            LOG_ERROR(Service_Nvnflinger,
                      "attempted to release buffer slot {} but its state was {}", slot,
                      slots[slot].buffer_state);

            return Status::BadValue;
        }

        core->SignalDequeueCondition();
    }

    // Call back without lock held
    if (listener != nullptr) {
        listener->OnBufferReleased();
    }

    return Status::NoError;
}

Status BufferQueueConsumer::Connect(std::shared_ptr<IConsumerListener> consumer_listener,
                                    bool controlled_by_app) {
    if (consumer_listener == nullptr) {
        LOG_ERROR(Service_Nvnflinger, "consumer_listener may not be nullptr");
        return Status::BadValue;
    }

    LOG_DEBUG(Service_Nvnflinger, "controlled_by_app={}", controlled_by_app);

    std::scoped_lock lock{core->mutex};

    if (core->is_abandoned) {
        LOG_ERROR(Service_Nvnflinger, "BufferQueue has been abandoned");
        return Status::NoInit;
    }

    core->consumer_listener = std::move(consumer_listener);
    core->consumer_controlled_by_app = controlled_by_app;

    return Status::NoError;
}

Status BufferQueueConsumer::Disconnect() {
    LOG_DEBUG(Service_Nvnflinger, "called");

    std::scoped_lock lock{core->mutex};

    if (core->consumer_listener == nullptr) {
        LOG_ERROR(Service_Nvnflinger, "no consumer is connected");
        return Status::BadValue;
    }

    core->is_abandoned = true;
    core->consumer_listener = nullptr;
    core->queue.clear();
    core->FreeAllBuffersLocked();
    core->SignalDequeueCondition();

    return Status::NoError;
}

Status BufferQueueConsumer::GetReleasedBuffers(u64* out_slot_mask) {
    if (out_slot_mask == nullptr) {
        LOG_ERROR(Service_Nvnflinger, "out_slot_mask may not be nullptr");
        return Status::BadValue;
    }

    std::scoped_lock lock{core->mutex};

    if (core->is_abandoned) {
        LOG_ERROR(Service_Nvnflinger, "BufferQueue has been abandoned");
        return Status::NoInit;
    }

    u64 mask = 0;
    for (int s = 0; s < BufferQueueDefs::NUM_BUFFER_SLOTS; ++s) {
        if (!slots[s].acquire_called) {
            mask |= (1ULL << s);
        }
    }

    // Remove from the mask queued buffers for which acquire has been called, since the consumer
    // will not receive their buffer addresses and so must retain their cached information
    auto current(core->queue.begin());
    while (current != core->queue.end()) {
        if (current->acquire_called) {
            mask &= ~(1ULL << current->slot);
        }
        ++current;
    }

    LOG_DEBUG(Service_Nvnflinger, "returning mask {}", mask);
    *out_slot_mask = mask;
    return Status::NoError;
}

/* SetPresentTime
for (int i = 0; i < core->history.size(); i++) {
    if (core->history[i].frame_number = target_frame_number) {
        core->history[i].state == BufferState::Acquired;
        break;
    }
}
*/

void BufferQueueConsumer::Transact(u32 code, std::span<const u8> parcel_data,
                                   std::span<u8> parcel_reply, u32 flags) {
    // Values used by BnGraphicBufferConsumer onTransact
    enum class TransactionId {
        AcquireBuffer = 1,
        DetachBuffer = 2,
        AttachBuffer = 3,
        ReleaseBuffer = 4,
        ConsumerConnect = 5,
        ConsumerDisconnect = 6,
        GetReleasedBuffers = 7,
        SetDefaultBufferSize = 8,
        SetDefaultMaxBufferCount = 9,
        DisableAsyncBuffer = 10,
        SetMaxAcquiredBufferCount = 11,
        SetConsumerName = 12,
        SetDefaultBufferFormat = 13,
        SetConsumerUsageBits = 14,
        SetTransformHint = 15,
        GetSidebandStream = 16,
        Unknown18 = 18,
        Unknown20 = 20,
    };

    Status status{Status::NoError};
    InputParcel parcel_in{parcel_data};
    OutputParcel parcel_out{};

    switch (static_cast<TransactionId>(code)) {
    case TransactionId::AcquireBuffer: {
        BufferItem item;
        const s64 present_when = parcel_in.Read<s64>();

        status = AcquireBuffer(&item, std::chrono::nanoseconds{present_when});

        // TODO: can't write this directly, needs a flattener for the sp<GraphicBuffer>
        // parcel_out.WriteFlattened(item);
        UNREACHABLE();
    }
    case TransactionId::ReleaseBuffer: {
        const s32 slot = parcel_in.Read<s32>();
        const u64 frame_number = parcel_in.Read<u64>();
        const auto release_fence = parcel_in.ReadFlattened<Fence>();

        status = ReleaseBuffer(slot, frame_number, release_fence);

        break;
    }
    case TransactionId::GetReleasedBuffers: {
        u64 slot_mask = 0;

        status = GetReleasedBuffers(&slot_mask);

        parcel_out.Write(slot_mask);
        break;
    }
    default:
        ASSERT_MSG(false, "called, code={} flags={}", code, flags);
        break;
    }

    parcel_out.Write(status);

    const auto serialized = parcel_out.Serialize();
    std::memcpy(parcel_reply.data(), serialized.data(),
                std::min(parcel_reply.size(), serialized.size()));
}

Kernel::KReadableEvent* BufferQueueConsumer::GetNativeHandle(u32 type_id) {
    ASSERT_MSG(false, "called, type_id={}", type_id);
    return nullptr;
}

} // namespace Service::android
