// SPDX-FileCopyrightText: Copyright 2021 sudachi Emulator Project
// SPDX-FileCopyrightText: Copyright 2014 The Android Open Source Project
// SPDX-License-Identifier: GPL-3.0-or-later
// Parts of this implementation were based on:
// https://cs.android.com/android/platform/superproject/+/android-5.1.1_r38:frameworks/native/libs/gui/BufferQueueProducer.cpp

#include "common/assert.h"
#include "common/logging/log.h"
#include "core/core_timing.h"
#include "core/hle/kernel/k_event.h"
#include "core/hle/kernel/k_readable_event.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/service/kernel_helpers.h"
#include "core/hle/service/nvnflinger/buffer_queue_core.h"
#include "core/hle/service/nvnflinger/buffer_queue_producer.h"
#include "core/hle/service/nvnflinger/consumer_listener.h"
#include "core/hle/service/nvnflinger/parcel.h"
#include "core/hle/service/nvnflinger/ui/graphic_buffer.h"
#include "core/hle/service/nvnflinger/window.h"
#include "core/hle/service/time/clock_types.h"

namespace Service::android {

BufferQueueProducer::BufferQueueProducer(Service::KernelHelpers::ServiceContext& service_context_,
                                         std::shared_ptr<BufferQueueCore> buffer_queue_core_,
                                         Service::Nvidia::NvCore::NvMap& nvmap_)
    : service_context{service_context_}, core{std::move(buffer_queue_core_)}, slots(core->slots),
      nvmap(nvmap_) {
    buffer_wait_event = service_context.CreateEvent("BufferQueue:WaitEvent");
}

BufferQueueProducer::~BufferQueueProducer() {
    service_context.CloseEvent(buffer_wait_event);
}

Status BufferQueueProducer::RequestBuffer(s32 slot, std::shared_ptr<GraphicBuffer>* buf) {
    LOG_DEBUG(Service_Nvnflinger, "slot {}", slot);

    std::scoped_lock lock{core->mutex};

    if (core->is_abandoned) {
        LOG_ERROR(Service_Nvnflinger, "BufferQueue has been abandoned");
        return Status::NoInit;
    }
    if (slot < 0 || slot >= BufferQueueDefs::NUM_BUFFER_SLOTS) {
        LOG_ERROR(Service_Nvnflinger, "slot index {} out of range [0, {})", slot,
                  BufferQueueDefs::NUM_BUFFER_SLOTS);
        return Status::BadValue;
    } else if (slots[slot].buffer_state != BufferState::Dequeued) {
        LOG_ERROR(Service_Nvnflinger, "slot {} is not owned by the producer (state = {})", slot,
                  slots[slot].buffer_state);
        return Status::BadValue;
    }

    slots[slot].request_buffer_called = true;
    *buf = slots[slot].graphic_buffer;

    return Status::NoError;
}

Status BufferQueueProducer::SetBufferCount(s32 buffer_count) {
    LOG_DEBUG(Service_Nvnflinger, "count = {}", buffer_count);

    std::shared_ptr<IConsumerListener> listener;
    {
        std::scoped_lock lock{core->mutex};
        core->WaitWhileAllocatingLocked();

        if (core->is_abandoned) {
            LOG_ERROR(Service_Nvnflinger, "BufferQueue has been abandoned");
            return Status::NoInit;
        }

        if (buffer_count > BufferQueueDefs::NUM_BUFFER_SLOTS) {
            LOG_ERROR(Service_Nvnflinger, "buffer_count {} too large (max {})", buffer_count,
                      BufferQueueDefs::NUM_BUFFER_SLOTS);
            return Status::BadValue;
        }

        // There must be no dequeued buffers when changing the buffer count.
        for (s32 s{}; s < BufferQueueDefs::NUM_BUFFER_SLOTS; ++s) {
            if (slots[s].buffer_state == BufferState::Dequeued) {
                LOG_ERROR(Service_Nvnflinger, "buffer owned by producer");
                return Status::BadValue;
            }
        }

        if (buffer_count == 0) {
            core->override_max_buffer_count = 0;
            core->SignalDequeueCondition();
            return Status::NoError;
        }

        const s32 min_buffer_slots = core->GetMinMaxBufferCountLocked(false);
        if (buffer_count < min_buffer_slots) {
            LOG_ERROR(Service_Nvnflinger, "requested buffer count {} is less than minimum {}",
                      buffer_count, min_buffer_slots);
            return Status::BadValue;
        }

        // Here we are guaranteed that the producer doesn't have any dequeued buffers and will
        // release all of its buffer references.
        if (core->GetPreallocatedBufferCountLocked() <= 0) {
            core->FreeAllBuffersLocked();
        }

        core->override_max_buffer_count = buffer_count;
        core->SignalDequeueCondition();
        buffer_wait_event->Signal();
        listener = core->consumer_listener;
    }

    // Call back without lock held
    if (listener != nullptr) {
        listener->OnBuffersReleased();
    }

    return Status::NoError;
}

Status BufferQueueProducer::WaitForFreeSlotThenRelock(bool async, s32* found, Status* return_flags,
                                                      std::unique_lock<std::mutex>& lk) const {
    bool try_again = true;

    while (try_again) {
        if (core->is_abandoned) {
            LOG_ERROR(Service_Nvnflinger, "BufferQueue has been abandoned");
            return Status::NoInit;
        }

        const s32 max_buffer_count = core->GetMaxBufferCountLocked(async);
        if (async && core->override_max_buffer_count) {
            if (core->override_max_buffer_count < max_buffer_count) {
                *found = BufferQueueCore::INVALID_BUFFER_SLOT;
                return Status::BadValue;
            }
        }

        // Free up any buffers that are in slots beyond the max buffer count
        for (s32 s = max_buffer_count; s < BufferQueueDefs::NUM_BUFFER_SLOTS; ++s) {
            ASSERT(slots[s].buffer_state == BufferState::Free);
            if (slots[s].graphic_buffer != nullptr && slots[s].buffer_state == BufferState::Free &&
                !slots[s].is_preallocated) {
                core->FreeBufferLocked(s);
                *return_flags |= Status::ReleaseAllBuffers;
            }
        }

        // Look for a free buffer to give to the client
        *found = BufferQueueCore::INVALID_BUFFER_SLOT;
        s32 dequeued_count{};
        s32 acquired_count{};
        for (s32 s{}; s < max_buffer_count; ++s) {
            switch (slots[s].buffer_state) {
            case BufferState::Dequeued:
                ++dequeued_count;
                break;
            case BufferState::Acquired:
                ++acquired_count;
                break;
            case BufferState::Free:
                // We return the oldest of the free buffers to avoid stalling the producer if
                // possible, since the consumer may still have pending reads of in-flight buffers
                if (*found == BufferQueueCore::INVALID_BUFFER_SLOT ||
                    slots[s].frame_number < slots[*found].frame_number) {
                    *found = s;
                }
                break;
            default:
                break;
            }
        }

        // Producers are not allowed to dequeue more than one buffer if they did not set a buffer
        // count
        if (!core->override_max_buffer_count && dequeued_count) {
            LOG_ERROR(Service_Nvnflinger,
                      "can't dequeue multiple buffers without setting the buffer count");
            return Status::InvalidOperation;
        }

        // See whether a buffer has been queued since the last SetBufferCount so we know whether to
        // perform the min undequeued buffers check below
        if (core->buffer_has_been_queued) {
            // Make sure the producer is not trying to dequeue more buffers than allowed
            const s32 new_undequeued_count = max_buffer_count - (dequeued_count + 1);
            const s32 min_undequeued_count = core->GetMinUndequeuedBufferCountLocked(async);
            if (new_undequeued_count < min_undequeued_count) {
                LOG_ERROR(Service_Nvnflinger,
                          "min undequeued buffer count({}) exceeded (dequeued={} undequeued={})",
                          min_undequeued_count, dequeued_count, new_undequeued_count);
                return Status::InvalidOperation;
            }
        }

        // If we disconnect and reconnect quickly, we can be in a state where our slots are empty
        // but we have many buffers in the queue. This can cause us to run out of memory if we
        // outrun the consumer. Wait here if it looks like we have too many buffers queued up.
        const bool too_many_buffers = core->queue.size() > static_cast<size_t>(max_buffer_count);
        if (too_many_buffers) {
            LOG_ERROR(Service_Nvnflinger, "queue size is {}, waiting", core->queue.size());
        }

        // If no buffer is found, or if the queue has too many buffers outstanding, wait for a
        // buffer to be acquired or released, or for the max buffer count to change.
        try_again = (*found == BufferQueueCore::INVALID_BUFFER_SLOT) || too_many_buffers;
        if (try_again) {
            // Return an error if we're in non-blocking mode (producer and consumer are controlled
            // by the application).
            if (core->dequeue_buffer_cannot_block &&
                (acquired_count <= core->max_acquired_buffer_count)) {
                return Status::WouldBlock;
            }

            if (!core->WaitForDequeueCondition(lk)) {
                // We are no longer running
                return Status::NoError;
            }
        }
    }

    return Status::NoError;
}

Status BufferQueueProducer::DequeueBuffer(s32* out_slot, Fence* out_fence, bool async, u32 width,
                                          u32 height, PixelFormat format, u32 usage) {
    LOG_DEBUG(Service_Nvnflinger, "async={} w={} h={} format={}, usage={}",
              async ? "true" : "false", width, height, format, usage);

    if ((width != 0 && height == 0) || (width == 0 && height != 0)) {
        LOG_ERROR(Service_Nvnflinger, "invalid size: w={} h={}", width, height);
        return Status::BadValue;
    }

    Status return_flags = Status::NoError;
    bool attached_by_consumer = false;
    {
        std::unique_lock lock{core->mutex};
        core->WaitWhileAllocatingLocked();

        if (format == PixelFormat::NoFormat) {
            format = core->default_buffer_format;
        }

        // Enable the usage bits the consumer requested
        usage |= core->consumer_usage_bit;

        s32 found{};
        Status status = WaitForFreeSlotThenRelock(async, &found, &return_flags, lock);
        if (status != Status::NoError) {
            return status;
        }

        // This should not happen
        if (found == BufferQueueCore::INVALID_BUFFER_SLOT) {
            LOG_ERROR(Service_Nvnflinger, "no available buffer slots");
            return Status::Busy;
        }

        *out_slot = found;

        attached_by_consumer = slots[found].attached_by_consumer;

        const bool use_default_size = !width && !height;
        if (use_default_size) {
            width = core->default_width;
            height = core->default_height;
        }

        slots[found].buffer_state = BufferState::Dequeued;

        const std::shared_ptr<GraphicBuffer>& buffer(slots[found].graphic_buffer);
        if ((buffer == nullptr) || (buffer->Width() != width) || (buffer->Height() != height) ||
            (buffer->Format() != format) || ((buffer->Usage() & usage) != usage)) {
            slots[found].acquire_called = false;
            slots[found].graphic_buffer = nullptr;
            slots[found].request_buffer_called = false;
            slots[found].fence = Fence::NoFence();

            return_flags |= Status::BufferNeedsReallocation;
        }

        slots[found].queue_time = Time::Clock::TimeSpanType::FromSeconds(0);
        slots[found].presentation_time = Time::Clock::TimeSpanType::FromSeconds(0);

        *out_fence = slots[found].fence;
        slots[found].fence = Fence::NoFence();
    }

    if ((return_flags & Status::BufferNeedsReallocation) != Status::None) {
        LOG_DEBUG(Service_Nvnflinger, "allocating a new buffer for slot {}", *out_slot);

        auto graphic_buffer = std::make_shared<GraphicBuffer>(width, height, format, usage);
        if (graphic_buffer == nullptr) {
            LOG_ERROR(Service_Nvnflinger, "creating GraphicBuffer failed");
            return Status::NoMemory;
        }

        {
            std::scoped_lock lock{core->mutex};

            if (core->is_abandoned) {
                LOG_ERROR(Service_Nvnflinger, "BufferQueue has been abandoned");
                return Status::NoInit;
            }

            slots[*out_slot].frame_number = UINT32_MAX;
            slots[*out_slot].graphic_buffer = graphic_buffer;
        }
    }

    if (attached_by_consumer) {
        return_flags |= Status::BufferNeedsReallocation;
    }

    LOG_DEBUG(Service_Nvnflinger, "returning slot={} frame={}, flags={}", *out_slot,
              slots[*out_slot].frame_number, return_flags);

    return return_flags;
}

Status BufferQueueProducer::DetachBuffer(s32 slot) {
    LOG_DEBUG(Service_Nvnflinger, "slot {}", slot);

    std::scoped_lock lock{core->mutex};

    if (core->is_abandoned) {
        LOG_ERROR(Service_Nvnflinger, "BufferQueue has been abandoned");
        return Status::NoInit;
    }

    if (slot < 0 || slot >= BufferQueueDefs::NUM_BUFFER_SLOTS) {
        LOG_ERROR(Service_Nvnflinger, "slot {} out of range [0, {})", slot,
                  BufferQueueDefs::NUM_BUFFER_SLOTS);
        return Status::BadValue;
    } else if (slots[slot].buffer_state != BufferState::Dequeued) {
        LOG_ERROR(Service_Nvnflinger, "slot {} is not owned by the producer (state = {})", slot,
                  slots[slot].buffer_state);
        return Status::BadValue;
    } else if (!slots[slot].request_buffer_called) {
        LOG_ERROR(Service_Nvnflinger, "buffer in slot {} has not been requested", slot);
        return Status::BadValue;
    }

    core->FreeBufferLocked(slot);
    core->SignalDequeueCondition();

    return Status::NoError;
}

Status BufferQueueProducer::DetachNextBuffer(std::shared_ptr<GraphicBuffer>* out_buffer,
                                             Fence* out_fence) {
    if (out_buffer == nullptr) {
        LOG_ERROR(Service_Nvnflinger, "out_buffer must not be nullptr");
        return Status::BadValue;
    } else if (out_fence == nullptr) {
        LOG_ERROR(Service_Nvnflinger, "out_fence must not be nullptr");
        return Status::BadValue;
    }

    std::scoped_lock lock{core->mutex};
    core->WaitWhileAllocatingLocked();

    if (core->is_abandoned) {
        LOG_ERROR(Service_Nvnflinger, "BufferQueue has been abandoned");
        return Status::NoInit;
    }

    // Find the oldest valid slot
    int found = BufferQueueCore::INVALID_BUFFER_SLOT;
    for (int s = 0; s < BufferQueueDefs::NUM_BUFFER_SLOTS; ++s) {
        if (slots[s].buffer_state == BufferState::Free && slots[s].graphic_buffer != nullptr) {
            if (found == BufferQueueCore::INVALID_BUFFER_SLOT ||
                slots[s].frame_number < slots[found].frame_number) {
                found = s;
            }
        }
    }

    if (found == BufferQueueCore::INVALID_BUFFER_SLOT) {
        return Status::NoMemory;
    }

    LOG_DEBUG(Service_Nvnflinger, "Detached slot {}", found);

    *out_buffer = slots[found].graphic_buffer;
    *out_fence = slots[found].fence;

    core->FreeBufferLocked(found);

    return Status::NoError;
}

Status BufferQueueProducer::AttachBuffer(s32* out_slot,
                                         const std::shared_ptr<GraphicBuffer>& buffer) {
    if (out_slot == nullptr) {
        LOG_ERROR(Service_Nvnflinger, "out_slot must not be nullptr");
        return Status::BadValue;
    } else if (buffer == nullptr) {
        LOG_ERROR(Service_Nvnflinger, "Cannot attach nullptr buffer");
        return Status::BadValue;
    }

    std::unique_lock lock{core->mutex};
    core->WaitWhileAllocatingLocked();

    Status return_flags = Status::NoError;
    s32 found{};

    const auto status = WaitForFreeSlotThenRelock(false, &found, &return_flags, lock);
    if (status != Status::NoError) {
        return status;
    }

    // This should not happen
    if (found == BufferQueueCore::INVALID_BUFFER_SLOT) {
        LOG_ERROR(Service_Nvnflinger, "No available buffer slots");
        return Status::Busy;
    }

    *out_slot = found;

    LOG_DEBUG(Service_Nvnflinger, "Returning slot {} flags={}", *out_slot, return_flags);

    slots[*out_slot].graphic_buffer = buffer;
    slots[*out_slot].buffer_state = BufferState::Dequeued;
    slots[*out_slot].fence = Fence::NoFence();
    slots[*out_slot].request_buffer_called = true;

    return return_flags;
}

Status BufferQueueProducer::QueueBuffer(s32 slot, const QueueBufferInput& input,
                                        QueueBufferOutput* output) {
    s64 timestamp{};
    bool is_auto_timestamp{};
    Common::Rectangle<s32> crop;
    NativeWindowScalingMode scaling_mode{};
    NativeWindowTransform transform;
    u32 sticky_transform_{};
    bool async{};
    s32 swap_interval{};
    Fence fence{};

    input.Deflate(&timestamp, &is_auto_timestamp, &crop, &scaling_mode, &transform,
                  &sticky_transform_, &async, &swap_interval, &fence);

    switch (scaling_mode) {
    case NativeWindowScalingMode::Freeze:
    case NativeWindowScalingMode::ScaleToWindow:
    case NativeWindowScalingMode::ScaleCrop:
    case NativeWindowScalingMode::NoScaleCrop:
    case NativeWindowScalingMode::PreserveAspectRatio:
        break;
    default:
        LOG_ERROR(Service_Nvnflinger, "unknown scaling mode {}", scaling_mode);
        return Status::BadValue;
    }

    std::shared_ptr<IConsumerListener> frame_available_listener;
    std::shared_ptr<IConsumerListener> frame_replaced_listener;
    s32 callback_ticket{};
    BufferItem item;

    {
        std::scoped_lock lock{core->mutex};

        if (core->is_abandoned) {
            LOG_ERROR(Service_Nvnflinger, "BufferQueue has been abandoned");
            return Status::NoInit;
        }

        const s32 max_buffer_count = core->GetMaxBufferCountLocked(async);
        if (async && core->override_max_buffer_count) {
            if (core->override_max_buffer_count < max_buffer_count) {
                LOG_ERROR(Service_Nvnflinger, "async mode is invalid with "
                                              "buffer count override");
                return Status::BadValue;
            }
        }

        if (slot < 0 || slot >= max_buffer_count) {
            LOG_ERROR(Service_Nvnflinger, "slot index {} out of range [0, {})", slot,
                      max_buffer_count);
            return Status::BadValue;
        } else if (slots[slot].buffer_state != BufferState::Dequeued) {
            LOG_ERROR(Service_Nvnflinger,
                      "slot {} is not owned by the producer "
                      "(state = {})",
                      slot, slots[slot].buffer_state);
            return Status::BadValue;
        } else if (!slots[slot].request_buffer_called) {
            LOG_ERROR(Service_Nvnflinger,
                      "slot {} was queued without requesting "
                      "a buffer",
                      slot);
            return Status::BadValue;
        }

        LOG_DEBUG(Service_Nvnflinger,
                  "slot={} frame={} time={} crop=[{},{},{},{}] transform={} scale={}", slot,
                  core->frame_counter + 1, timestamp, crop.Left(), crop.Top(), crop.Right(),
                  crop.Bottom(), transform, scaling_mode);

        const std::shared_ptr<GraphicBuffer>& graphic_buffer(slots[slot].graphic_buffer);
        Common::Rectangle<s32> buffer_rect(graphic_buffer->Width(), graphic_buffer->Height());
        Common::Rectangle<s32> cropped_rect;
        [[maybe_unused]] const bool unused = crop.Intersect(buffer_rect, &cropped_rect);

        if (cropped_rect != crop) {
            LOG_ERROR(Service_Nvnflinger, "crop rect is not contained within the buffer in slot {}",
                      slot);
            return Status::BadValue;
        }

        auto& ctx = service_context;
        auto& system = ctx.m_system;

        slots[slot].fence = fence;
        slots[slot].buffer_state = BufferState::Queued;
        ++core->frame_counter;
        slots[slot].frame_number = core->frame_counter;
        slots[slot].queue_time = Time::Clock::TimeSpanType::FromTicks<Core::Hardware::CNTFREQ>(
            system.CoreTiming().GetClockTicks()); // this is not the true value
        slots[slot].presentation_time = Time::Clock::TimeSpanType::FromSeconds(
            0); // doesn't need to be set afaik but do it anyway

        item.acquire_called = slots[slot].acquire_called;
        item.graphic_buffer = slots[slot].graphic_buffer;
        item.crop = crop;
        item.transform = transform & ~NativeWindowTransform::InverseDisplay;
        item.transform_to_display_inverse =
            (transform & NativeWindowTransform::InverseDisplay) != NativeWindowTransform::None;
        item.scaling_mode = static_cast<u32>(scaling_mode);
        item.timestamp = timestamp;
        item.is_auto_timestamp = is_auto_timestamp;
        item.frame_number = core->frame_counter;
        item.slot = slot;
        item.fence = fence;
        item.is_droppable = core->dequeue_buffer_cannot_block || async;
        item.swap_interval = swap_interval;

        // TODO: .queue_time should be changed to the correct value
        position = (position + 1) % 8;
        LOG_WARNING(Service_Nvnflinger, "position={}", position);
        core->history[position] = {.frame_number = core->frame_counter,
                                   .queue_time = slots[slot].queue_time,
                                   .state = BufferState::Queued};

        sticky_transform = sticky_transform_;

        if (core->queue.empty()) {
            // When the queue is empty, we can simply queue this buffer
            core->queue.push_back(item);
            frame_available_listener = core->consumer_listener;
        } else {
            // When the queue is not empty, we need to look at the front buffer
            // state to see if we need to replace it
            auto front(core->queue.begin());

            if (front->is_droppable) {
                // If the front queued buffer is still being tracked, we first
                // mark it as freed
                if (core->StillTracking(*front)) {
                    slots[front->slot].buffer_state = BufferState::Free;
                    // Reset the frame number of the freed buffer so that it is the first in line to
                    // be dequeued again
                    slots[front->slot].frame_number = 0;
                }
                // Overwrite the droppable buffer with the incoming one
                *front = item;
                frame_replaced_listener = core->consumer_listener;
            } else {
                core->queue.push_back(item);
                frame_available_listener = core->consumer_listener;
            }
        }

        core->buffer_has_been_queued = true;
        core->SignalDequeueCondition();
        output->Inflate(core->default_width, core->default_height, core->transform_hint,
                        static_cast<u32>(core->queue.size()), core->frame_counter);

        if ((sticky_transform & 8) != 0) {
            output->transform_hint |= (u32)NativeWindowTransform::ReturnFrameNumber;
            output->frame_number = slots[slot].frame_number;
        }

        // Take a ticket for the callback functions
        callback_ticket = next_callback_ticket++;
    }

    // Don't send the GraphicBuffer through the callback, and don't send the slot number, since the
    // consumer shouldn't need it
    item.graphic_buffer.reset();
    item.slot = BufferItem::INVALID_BUFFER_SLOT;

    // Call back without the main BufferQueue lock held, but with the callback lock held so we can
    // ensure that callbacks occur in order
    {
        std::scoped_lock lock{callback_mutex};
        while (callback_ticket != current_callback_ticket) {
            callback_condition.wait(callback_mutex);
        }

        if (frame_available_listener != nullptr) {
            frame_available_listener->OnFrameAvailable(item);
        } else if (frame_replaced_listener != nullptr) {
            frame_replaced_listener->OnFrameReplaced(item);
        }

        ++current_callback_ticket;
        callback_condition.notify_all();
    }

    return Status::NoError;
}

void BufferQueueProducer::CancelBuffer(s32 slot, const Fence& fence) {
    LOG_DEBUG(Service_Nvnflinger, "slot {}", slot);

    std::scoped_lock lock{core->mutex};

    if (core->is_abandoned) {
        LOG_ERROR(Service_Nvnflinger, "BufferQueue has been abandoned");
        return;
    }

    if (slot < 0 || slot >= BufferQueueDefs::NUM_BUFFER_SLOTS) {
        LOG_ERROR(Service_Nvnflinger, "slot index {} out of range [0, {})", slot,
                  BufferQueueDefs::NUM_BUFFER_SLOTS);
        return;
    } else if (slots[slot].buffer_state != BufferState::Dequeued) {
        LOG_ERROR(Service_Nvnflinger, "slot {} is not owned by the producer (state = {})", slot,
                  slots[slot].buffer_state);
        return;
    }

    slots[slot].buffer_state = BufferState::Free;
    slots[slot].frame_number = 0;
    slots[slot].fence = fence;

    core->SignalDequeueCondition();
    buffer_wait_event->Signal();
}

Status BufferQueueProducer::Query(NativeWindow what, s32* out_value) {
    std::scoped_lock lock{core->mutex};

    if (out_value == nullptr) {
        LOG_ERROR(Service_Nvnflinger, "outValue was nullptr");
        return Status::BadValue;
    }

    if (core->is_abandoned) {
        LOG_ERROR(Service_Nvnflinger, "BufferQueue has been abandoned");
        return Status::NoInit;
    }

    u32 value{};
    switch (what) {
    case NativeWindow::Width:
        value = core->default_width;
        break;
    case NativeWindow::Height:
        value = core->default_height;
        break;
    case NativeWindow::Format:
        value = static_cast<u32>(core->default_buffer_format);
        break;
    case NativeWindow::MinUndequeedBuffers:
        value = core->GetMinUndequeuedBufferCountLocked(false);
        break;
    case NativeWindow::StickyTransform:
        value = sticky_transform;
        break;
    case NativeWindow::ConsumerRunningBehind:
        value = (core->queue.size() > 1);
        break;
    case NativeWindow::ConsumerUsageBits:
        value = core->consumer_usage_bit;
        break;
    default:
        ASSERT(false);
        return Status::BadValue;
    }

    LOG_WARNING(Service_Nvnflinger, "what = {}, value = {}", what, value);

    *out_value = static_cast<s32>(value);

    return Status::NoError;
}

Status BufferQueueProducer::Connect(const std::shared_ptr<IProducerListener>& listener,
                                    NativeWindowApi api, bool producer_controlled_by_app,
                                    QueueBufferOutput* output) {
    std::scoped_lock lock{core->mutex};

    LOG_DEBUG(Service_Nvnflinger, "api = {} producer_controlled_by_app = {}", api,
              producer_controlled_by_app);

    if (core->is_abandoned) {
        LOG_ERROR(Service_Nvnflinger, "BufferQueue has been abandoned");
        return Status::NoInit;
    }

    if (core->consumer_listener == nullptr) {
        LOG_ERROR(Service_Nvnflinger, "BufferQueue has no consumer");
        return Status::NoInit;
    }

    if (output == nullptr) {
        LOG_ERROR(Service_Nvnflinger, "output was nullptr");
        return Status::BadValue;
    }

    if (core->connected_api != NativeWindowApi::NoConnectedApi) {
        LOG_ERROR(Service_Nvnflinger, "already connected (cur = {} req = {})", core->connected_api,
                  api);
        return Status::BadValue;
    }

    Status status = Status::NoError;
    switch (api) {
    case NativeWindowApi::Egl:
    case NativeWindowApi::Cpu:
    case NativeWindowApi::Media:
    case NativeWindowApi::Camera:
        core->connected_api = api;
        output->Inflate(core->default_width, core->default_height, core->transform_hint,
                        static_cast<u32>(core->queue.size()), core->frame_counter);
        core->connected_producer_listener = listener;
        break;
    default:
        LOG_ERROR(Service_Nvnflinger, "unknown api = {}", api);
        status = Status::BadValue;
        break;
    }

    core->buffer_has_been_queued = false;
    core->dequeue_buffer_cannot_block =
        core->consumer_controlled_by_app && producer_controlled_by_app;

    return status;
}

Status BufferQueueProducer::Disconnect(NativeWindowApi api) {
    LOG_DEBUG(Service_Nvnflinger, "api = {}", api);

    Status status = Status::NoError;
    std::shared_ptr<IConsumerListener> listener;

    {
        std::scoped_lock lock{core->mutex};

        core->WaitWhileAllocatingLocked();

        if (core->is_abandoned) {
            // Disconnecting after the surface has been abandoned is a no-op.
            return Status::NoError;
        }

        switch (api) {
        case NativeWindowApi::Egl:
        case NativeWindowApi::Cpu:
        case NativeWindowApi::Media:
        case NativeWindowApi::Camera:
            if (core->connected_api == api) {
                core->queue.clear();
                core->FreeAllBuffersLocked();
                core->connected_producer_listener = nullptr;
                core->connected_api = NativeWindowApi::NoConnectedApi;
                core->SignalDequeueCondition();
                buffer_wait_event->Signal();
                listener = core->consumer_listener;
            } else {
                LOG_ERROR(Service_Nvnflinger, "still connected to another api (cur = {} req = {})",
                          core->connected_api, api);
                status = Status::BadValue;
            }
            break;
        default:
            LOG_ERROR(Service_Nvnflinger, "unknown api = {}", api);
            status = Status::BadValue;
            break;
        }
    }

    // Call back without lock held
    if (listener != nullptr) {
        listener->OnBuffersReleased();
    }

    return status;
}

Status BufferQueueProducer::SetPreallocatedBuffer(s32 slot,
                                                  const std::shared_ptr<NvGraphicBuffer>& buffer) {
    LOG_DEBUG(Service_Nvnflinger, "slot {}", slot);

    if (slot < 0 || slot >= BufferQueueDefs::NUM_BUFFER_SLOTS) {
        return Status::BadValue;
    }

    std::scoped_lock lock{core->mutex};

    slots[slot] = {};
    slots[slot].fence = Fence::NoFence();
    slots[slot].graphic_buffer = std::make_shared<GraphicBuffer>(nvmap, buffer);
    slots[slot].frame_number = 0;

    // Most games preallocate a buffer and pass a valid buffer here. However, it is possible for
    // this to be called with an empty buffer, Naruto Ultimate Ninja Storm is a game that does this.
    if (buffer) {
        slots[slot].is_preallocated = true;

        core->override_max_buffer_count = core->GetPreallocatedBufferCountLocked();
        core->default_width = buffer->Width();
        core->default_height = buffer->Height();
        core->default_buffer_format = buffer->Format();
    }

    core->SignalDequeueCondition();
    buffer_wait_event->Signal();

    return Status::NoError;
}

void BufferQueueProducer::Transact(u32 code, std::span<const u8> parcel_data,
                                   std::span<u8> parcel_reply, u32 flags) {
    // Values used by BnGraphicBufferProducer onTransact
    enum class TransactionId {
        RequestBuffer = 1,
        SetBufferCount = 2,
        DequeueBuffer = 3,
        DetachBuffer = 4,
        DetachNextBuffer = 5,
        AttachBuffer = 6,
        QueueBuffer = 7,
        CancelBuffer = 8,
        Query = 9,
        Connect = 10,
        Disconnect = 11,
        AllocateBuffers = 13,
        SetPreallocatedBuffer = 14,
        GetBufferHistory = 17,
    };

    Status status{Status::NoError};
    InputParcel parcel_in{parcel_data};
    OutputParcel parcel_out{};

    switch (static_cast<TransactionId>(code)) {
    case TransactionId::Connect: {
        const auto enable_listener = parcel_in.Read<bool>();
        const auto api = parcel_in.Read<NativeWindowApi>();
        const auto producer_controlled_by_app = parcel_in.Read<bool>();

        UNIMPLEMENTED_IF_MSG(enable_listener, "Listener is unimplemented!");

        std::shared_ptr<IProducerListener> listener;
        QueueBufferOutput output{};

        status = Connect(listener, api, producer_controlled_by_app, &output);

        LOG_WARNING(Service_Nvnflinger, "Connect, output.frame_number={}, output.transform_hint={}",
                    output.frame_number, output.transform_hint);

        parcel_out.Write(output);
        if (output.transform_hint & (u32)NativeWindowTransform::ReturnFrameNumber)
            parcel_out.Write<u64>(output.frame_number);
        break;
    }
    case TransactionId::SetPreallocatedBuffer: {
        const auto slot = parcel_in.Read<s32>();
        const auto buffer = parcel_in.ReadObject<NvGraphicBuffer>();

        status = SetPreallocatedBuffer(slot, buffer);
        break;
    }
    case TransactionId::DequeueBuffer: {
        const auto is_async = parcel_in.Read<bool>();
        const auto width = parcel_in.Read<u32>();
        const auto height = parcel_in.Read<u32>();
        const auto pixel_format = parcel_in.Read<PixelFormat>();
        const auto usage = parcel_in.Read<u32>();

        s32 slot{};
        Fence fence{};

        status = DequeueBuffer(&slot, &fence, is_async, width, height, pixel_format, usage);

        parcel_out.Write(slot);
        parcel_out.WriteFlattenedObject(&fence);
        break;
    }
    case TransactionId::RequestBuffer: {
        const auto slot = parcel_in.Read<s32>();

        std::shared_ptr<GraphicBuffer> buf;

        status = RequestBuffer(slot, &buf);

        parcel_out.WriteFlattenedObject<NvGraphicBuffer>(buf.get());
        break;
    }
    case TransactionId::QueueBuffer: {
        const auto slot = parcel_in.Read<s32>();

        QueueBufferInput input{parcel_in};
        QueueBufferOutput output;

        status = QueueBuffer(slot, input, &output);
        LOG_WARNING(Service_Nvnflinger,
                    "QueueBuffer, output.frame_number={}, output.transform_hint={}",
                    output.frame_number, output.transform_hint);

        parcel_out.Write(output);
        if (output.transform_hint & (u32)NativeWindowTransform::ReturnFrameNumber)
            parcel_out.Write<u64>(output.frame_number);
        break;
    }
    case TransactionId::Query: {
        const auto what = parcel_in.Read<NativeWindow>();

        s32 value{};

        status = Query(what, &value);

        parcel_out.Write(value);
        break;
    }
    case TransactionId::CancelBuffer: {
        const auto slot = parcel_in.Read<s32>();
        const auto fence = parcel_in.ReadFlattened<Fence>();

        CancelBuffer(slot, fence);
        break;
    }
    case TransactionId::Disconnect: {
        const auto api = parcel_in.Read<NativeWindowApi>();

        status = Disconnect(api);
        break;
    }
    case TransactionId::DetachBuffer: {
        const auto slot = parcel_in.Read<s32>();

        status = DetachBuffer(slot);
        break;
    }
    case TransactionId::SetBufferCount: {
        const auto buffer_count = parcel_in.Read<s32>();

        status = SetBufferCount(buffer_count);
        break;
    }
    case TransactionId::GetBufferHistory: {
        LOG_WARNING(Service_Nvnflinger, "(STUBBED) called");

        std::scoped_lock lock{core->mutex};

        auto buffer_history_count = std::min(parcel_in.Read<s32>(), (s32)core->history.size());

        BufferQueueCore::BufferInfo* info = new BufferQueueCore::BufferInfo[buffer_history_count];
        auto pos = position;
        for (int i = 0; i < buffer_history_count; i++) {
            info[i] = core->history[(pos - i) % core->history.size()];
            LOG_WARNING(Service_Nvnflinger, "frame_number={}, state={}",
                        core->history[(pos - i) % core->history.size()].frame_number,
                        (u32)core->history[(pos - i) % core->history.size()].state);
            pos--;
        }

        parcel_out.Write<s32>(buffer_history_count);
        parcel_out.WriteFlattenedObject<BufferQueueCore::BufferInfo>(info);
        status = Status::NoError;

        break;
    }
    default:
        ASSERT_MSG(false, "Unimplemented TransactionId {}", code);
        break;
    }

    parcel_out.Write(status);

    const auto serialized = parcel_out.Serialize();
    std::memcpy(parcel_reply.data(), serialized.data(),
                std::min(parcel_reply.size(), serialized.size()));
}

Kernel::KReadableEvent* BufferQueueProducer::GetNativeHandle(u32 type_id) {
    return &buffer_wait_event->GetReadableEvent();
}

} // namespace Service::android
