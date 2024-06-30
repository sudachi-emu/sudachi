// SPDX-FileCopyrightText: Copyright 2021 sudachi Emulator Project
// SPDX-FileCopyrightText: Copyright 2014 The Android Open Source Project
// SPDX-License-Identifier: GPL-3.0-or-later
// Parts of this implementation were based on:
// https://cs.android.com/android/platform/superproject/+/android-5.1.1_r38:frameworks/native/include/gui/BufferQueueProducer.h

#pragma once

#include <condition_variable>
#include <memory>
#include <mutex>

#include "common/common_funcs.h"
#include "core/hle/service/nvdrv/nvdata.h"
#include "core/hle/service/nvnflinger/binder.h"
#include "core/hle/service/nvnflinger/buffer_queue_defs.h"
#include "core/hle/service/nvnflinger/buffer_slot.h"
#include "core/hle/service/nvnflinger/graphic_buffer_producer.h"
#include "core/hle/service/nvnflinger/pixel_format.h"
#include "core/hle/service/nvnflinger/status.h"
#include "core/hle/service/nvnflinger/window.h"

namespace Kernel {
class KernelCore;
class KEvent;
class KReadableEvent;
} // namespace Kernel

namespace Service::KernelHelpers {
class ServiceContext;
} // namespace Service::KernelHelpers

namespace Service::Nvidia::NvCore {
class NvMap;
} // namespace Service::Nvidia::NvCore

namespace Service::android {

class BufferQueueCore;
class IProducerListener;
struct NvGraphicBuffer;

class BufferQueueProducer final : public IBinder {
public:
    explicit BufferQueueProducer(Service::KernelHelpers::ServiceContext& service_context_,
                                 std::shared_ptr<BufferQueueCore> buffer_queue_core_,
                                 Service::Nvidia::NvCore::NvMap& nvmap_);
    ~BufferQueueProducer() override;

    void Transact(u32 code, std::span<const u8> parcel_data, std::span<u8> parcel_reply,
                  u32 flags) override;

    Kernel::KReadableEvent* GetNativeHandle(u32 type_id) override;

public:
    Status RequestBuffer(s32 slot, std::shared_ptr<GraphicBuffer>* buf);
    Status SetBufferCount(s32 buffer_count);
    Status DequeueBuffer(s32* out_slot, android::Fence* out_fence, bool async, u32 width,
                         u32 height, PixelFormat format, u32 usage);
    Status DetachBuffer(s32 slot);
    Status DetachNextBuffer(std::shared_ptr<GraphicBuffer>* out_buffer, Fence* out_fence);
    Status AttachBuffer(s32* outSlot, const std::shared_ptr<GraphicBuffer>& buffer);
    Status QueueBuffer(s32 slot, const QueueBufferInput& input, QueueBufferOutput* output);
    void CancelBuffer(s32 slot, const Fence& fence);
    Status Query(NativeWindow what, s32* out_value);
    Status Connect(const std::shared_ptr<IProducerListener>& listener, NativeWindowApi api,
                   bool producer_controlled_by_app, QueueBufferOutput* output);

    Status Disconnect(NativeWindowApi api);
    Status SetPreallocatedBuffer(s32 slot, const std::shared_ptr<NvGraphicBuffer>& buffer);

private:
    BufferQueueProducer(const BufferQueueProducer&) = delete;

    Status WaitForFreeSlotThenRelock(bool async, s32* found, Status* return_flags,
                                     std::unique_lock<std::mutex>& lk) const;

    Kernel::KEvent* buffer_wait_event{};
    Service::KernelHelpers::ServiceContext& service_context;

    std::shared_ptr<BufferQueueCore> core;
    BufferQueueDefs::SlotsType& slots;
    u32 sticky_transform{};
    std::mutex callback_mutex;
    s32 next_callback_ticket{};
    s32 current_callback_ticket{};
    std::condition_variable_any callback_condition;

    u64 position;

    Service::Nvidia::NvCore::NvMap& nvmap;
};

} // namespace Service::android
