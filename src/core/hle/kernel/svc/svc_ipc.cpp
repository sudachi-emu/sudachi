// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/scope_exit.h"
#include "common/scratch_buffer.h"
#include "core/core.h"
#include "core/hle/kernel/k_client_session.h"
#include "core/hle/kernel/k_hardware_timer.h"
#include "core/hle/kernel/k_process.h"
#include "core/hle/kernel/k_scoped_resource_reservation.h"
#include "core/hle/kernel/k_server_session.h"
#include "core/hle/kernel/k_session.h"
#include "core/hle/kernel/svc.h"
#include "core/hle/kernel/svc_results.h"

namespace Kernel::Svc {

namespace {

Result SendSyncRequestImpl(KernelCore& kernel, uintptr_t message, size_t buffer_size,
                           Handle session_handle) {
    // Get the client session.
    KScopedAutoObject session =
        GetCurrentProcess(kernel).GetHandleTable().GetObject<KClientSession>(session_handle);
    R_UNLESS(session.IsNotNull(), ResultInvalidHandle);

    // Get the parent, and persist a reference to it until we're done.
    KScopedAutoObject parent = session->GetParent();
    ASSERT(parent.IsNotNull());

    // Send the request.
    R_RETURN(session->SendSyncRequest(message, buffer_size));
}

Result ReplyAndReceiveImpl(KernelCore& kernel, int32_t* out_index, uintptr_t message,
                           size_t buffer_size, KPhysicalAddress message_paddr,
                           KSynchronizationObject** objs, int32_t num_objects, Handle reply_target,
                           int64_t timeout_ns) {
    // Reply to the target, if one is specified.
    if (reply_target != InvalidHandle) {
        KScopedAutoObject session =
            GetCurrentProcess(kernel).GetHandleTable().GetObject<KServerSession>(reply_target);
        R_UNLESS(session.IsNotNull(), ResultInvalidHandle);

        // If we fail to reply, we want to set the output index to -1.
        ON_RESULT_FAILURE {
            *out_index = -1;
        };

        // Send the reply.
        R_TRY(session->SendReply(message, buffer_size, message_paddr));
    }

    // Receive a message.
    {
        // Convert the timeout from nanoseconds to ticks.
        // NOTE: Nintendo does not use this conversion logic in WaitSynchronization...
        s64 timeout;
        if (timeout_ns > 0) {
            const s64 offset_tick(timeout_ns);
            if (offset_tick > 0) {
                timeout = kernel.HardwareTimer().GetTick() + offset_tick + 2;
                if (timeout <= 0) {
                    timeout = std::numeric_limits<s64>::max();
                }
            } else {
                timeout = std::numeric_limits<s64>::max();
            }
        } else {
            timeout = timeout_ns;
        }

        // Wait for a message.
        while (true) {
            // Wait for an object.
            s32 index;
            Result result = KSynchronizationObject::Wait(kernel, std::addressof(index), objs,
                                                         num_objects, timeout);
            if (ResultTimedOut == result) {
                R_THROW(result);
            }

            // Receive the request.
            if (R_SUCCEEDED(result)) {
                KServerSession* session = objs[index]->DynamicCast<KServerSession*>();
                if (session != nullptr) {
                    result = session->ReceiveRequest(message, buffer_size, message_paddr);
                    if (ResultNotFound == result) {
                        continue;
                    }
                }
            }

            *out_index = index;
            R_RETURN(result);
        }
    }
}

Result ReplyAndReceiveImpl(KernelCore& kernel, int32_t* out_index, uintptr_t message,
                           size_t buffer_size, KPhysicalAddress message_paddr,
                           KProcessAddress user_handles, int32_t num_handles, Handle reply_target,
                           int64_t timeout_ns) {
    // Ensure number of handles is valid.
    R_UNLESS(0 <= num_handles && num_handles <= Svc::ArgumentHandleCountMax, ResultOutOfRange);

    // Get the synchronization context.
    auto& process = GetCurrentProcess(kernel);
    auto& thread = GetCurrentThread(kernel);
    auto& handle_table = process.GetHandleTable();
    KSynchronizationObject** objs = thread.GetSynchronizationObjectBuffer().data();
    Handle* handles = thread.GetHandleBuffer().data();

    // Copy user handles.
    if (num_handles > 0) {
        // Ensure that we can try to get the handles.
        R_UNLESS(process.GetPageTable().Contains(user_handles, num_handles * sizeof(Handle)),
                 ResultInvalidPointer);

        // Get the handles
        R_UNLESS(
            GetCurrentMemory(kernel).ReadBlock(user_handles, handles, sizeof(Handle) * num_handles),
            ResultInvalidPointer);

        // Convert the handles to objects.
        R_UNLESS(
            handle_table.GetMultipleObjects<KSynchronizationObject>(objs, handles, num_handles),
            ResultInvalidHandle);
    }

    // Ensure handles are closed when we're done.
    SCOPE_EXIT {
        for (auto i = 0; i < num_handles; ++i) {
            objs[i]->Close();
        }
    };

    R_RETURN(ReplyAndReceiveImpl(kernel, out_index, message, buffer_size, message_paddr, objs,
                                 num_handles, reply_target, timeout_ns));
}

} // namespace

/// Makes a blocking IPC call to a service.
Result SendSyncRequest(Core::System& system, Handle session_handle) {
    R_RETURN(SendSyncRequestImpl(system.Kernel(), 0, 0, session_handle));
}

Result SendSyncRequestWithUserBuffer(Core::System& system, uint64_t message, uint64_t buffer_size,
                                     Handle session_handle) {
    auto& kernel = system.Kernel();

    // Validate that the message buffer is page aligned and does not overflow.
    R_UNLESS(Common::IsAligned(message, PageSize), ResultInvalidAddress);
    R_UNLESS(buffer_size > 0, ResultInvalidSize);
    R_UNLESS(Common::IsAligned(buffer_size, PageSize), ResultInvalidSize);
    R_UNLESS(message < message + buffer_size, ResultInvalidCurrentMemory);

    // Get the process page table.
    auto& page_table = GetCurrentProcess(kernel).GetPageTable();

    // Lock the message buffer.
    R_TRY(page_table.LockForIpcUserBuffer(nullptr, message, buffer_size));

    {
        // If we fail to send the message, unlock the message buffer.
        ON_RESULT_FAILURE {
            page_table.UnlockForIpcUserBuffer(message, buffer_size);
        };

        // Send the request.
        ASSERT(message != 0);
        R_TRY(SendSyncRequestImpl(kernel, message, buffer_size, session_handle));
    }

    // We successfully processed, so try to unlock the message buffer.
    R_RETURN(page_table.UnlockForIpcUserBuffer(message, buffer_size));
}

Result SendAsyncRequestWithUserBuffer(Core::System& system, Handle* out_event_handle,
                                      uint64_t message, uint64_t buffer_size,
                                      Handle session_handle) {
    // Get the process and handle table.
    auto& process = GetCurrentProcess(system.Kernel());
    auto& handle_table = process.GetHandleTable();

    // Reserve a new event from the process resource limit.
    KScopedResourceReservation event_reservation(std::addressof(process),
                                                 Svc::LimitableResource::EventCountMax);
    R_UNLESS(event_reservation.Succeeded(), ResultLimitReached);

    // Get the client session.
    KScopedAutoObject session = process.GetHandleTable().GetObject<KClientSession>(session_handle);
    R_UNLESS(session.IsNotNull(), ResultInvalidHandle);

    // Get the parent, and persist a reference to it until we're done.
    KScopedAutoObject parent = session->GetParent();
    ASSERT(parent.IsNotNull());

    // Create a new event.
    KEvent* event = KEvent::Create(system.Kernel());
    R_UNLESS(event != nullptr, ResultOutOfResource);

    // Initialize the event.
    event->Initialize(std::addressof(process));

    // Commit our reservation.
    event_reservation.Commit();

    // At end of scope, kill the standing references to the sub events.
    SCOPE_EXIT {
        event->GetReadableEvent().Close();
        event->Close();
    };

    // Register the event.
    KEvent::Register(system.Kernel(), event);

    // Add the readable event to the handle table.
    R_TRY(handle_table.Add(out_event_handle, std::addressof(event->GetReadableEvent())));

    // Ensure that if we fail to send the request, we close the readable handle.
    ON_RESULT_FAILURE {
        handle_table.Remove(*out_event_handle);
    };

    // Send the async request.
    R_RETURN(session->SendAsyncRequest(event, message, buffer_size));
}

Result ReplyAndReceive(Core::System& system, s32* out_index, uint64_t handles, s32 num_handles,
                       Handle reply_target, s64 timeout_ns) {
    R_RETURN(ReplyAndReceiveImpl(system.Kernel(), out_index, 0, 0, 0, handles, num_handles,
                                 reply_target, timeout_ns));
}

Result ReplyAndReceiveWithUserBuffer(Core::System& system, int32_t* out_index, uint64_t message,
                                     uint64_t buffer_size, uint64_t handles, int32_t num_handles,
                                     Handle reply_target, int64_t timeout_ns) {
    // Validate that the message buffer is page aligned and does not overflow.
    R_UNLESS(Common::IsAligned(message, PageSize), ResultInvalidAddress);
    R_UNLESS(buffer_size > 0, ResultInvalidSize);
    R_UNLESS(Common::IsAligned(buffer_size, PageSize), ResultInvalidSize);
    R_UNLESS(message < message + buffer_size, ResultInvalidCurrentMemory);

    // Get the process page table.
    auto& page_table = GetCurrentProcess(system.Kernel()).GetPageTable();

    // Lock the message buffer, getting its physical address.
    KPhysicalAddress message_paddr;
    R_TRY(page_table.LockForIpcUserBuffer(std::addressof(message_paddr), message, buffer_size));

    {
        // If we fail to send the message, unlock the message buffer.
        ON_RESULT_FAILURE {
            page_table.UnlockForIpcUserBuffer(message, buffer_size);
        };

        // Reply/Receive the request.
        ASSERT(message != 0);
        R_TRY(ReplyAndReceiveImpl(system.Kernel(), out_index, message, buffer_size, message_paddr,
                                  handles, num_handles, reply_target, timeout_ns));
    }

    // We successfully processed, so try to unlock the message buffer.
    R_RETURN(page_table.UnlockForIpcUserBuffer(message, buffer_size));
}

Result SendSyncRequest64(Core::System& system, Handle session_handle) {
    R_RETURN(SendSyncRequest(system, session_handle));
}

Result SendSyncRequestWithUserBuffer64(Core::System& system, uint64_t message_buffer,
                                       uint64_t message_buffer_size, Handle session_handle) {
    R_RETURN(
        SendSyncRequestWithUserBuffer(system, message_buffer, message_buffer_size, session_handle));
}

Result SendAsyncRequestWithUserBuffer64(Core::System& system, Handle* out_event_handle,
                                        uint64_t message_buffer, uint64_t message_buffer_size,
                                        Handle session_handle) {
    R_RETURN(SendAsyncRequestWithUserBuffer(system, out_event_handle, message_buffer,
                                            message_buffer_size, session_handle));
}

Result ReplyAndReceive64(Core::System& system, int32_t* out_index, uint64_t handles,
                         int32_t num_handles, Handle reply_target, int64_t timeout_ns) {
    R_RETURN(ReplyAndReceive(system, out_index, handles, num_handles, reply_target, timeout_ns));
}

Result ReplyAndReceiveWithUserBuffer64(Core::System& system, int32_t* out_index,
                                       uint64_t message_buffer, uint64_t message_buffer_size,
                                       uint64_t handles, int32_t num_handles, Handle reply_target,
                                       int64_t timeout_ns) {
    R_RETURN(ReplyAndReceiveWithUserBuffer(system, out_index, message_buffer, message_buffer_size,
                                           handles, num_handles, reply_target, timeout_ns));
}

Result SendSyncRequest64From32(Core::System& system, Handle session_handle) {
    R_RETURN(SendSyncRequest(system, session_handle));
}

Result SendSyncRequestWithUserBuffer64From32(Core::System& system, uint32_t message_buffer,
                                             uint32_t message_buffer_size, Handle session_handle) {
    R_RETURN(
        SendSyncRequestWithUserBuffer(system, message_buffer, message_buffer_size, session_handle));
}

Result SendAsyncRequestWithUserBuffer64From32(Core::System& system, Handle* out_event_handle,
                                              uint32_t message_buffer, uint32_t message_buffer_size,
                                              Handle session_handle) {
    R_RETURN(SendAsyncRequestWithUserBuffer(system, out_event_handle, message_buffer,
                                            message_buffer_size, session_handle));
}

Result ReplyAndReceive64From32(Core::System& system, int32_t* out_index, uint32_t handles,
                               int32_t num_handles, Handle reply_target, int64_t timeout_ns) {
    R_RETURN(ReplyAndReceive(system, out_index, handles, num_handles, reply_target, timeout_ns));
}

Result ReplyAndReceiveWithUserBuffer64From32(Core::System& system, int32_t* out_index,
                                             uint32_t message_buffer, uint32_t message_buffer_size,
                                             uint32_t handles, int32_t num_handles,
                                             Handle reply_target, int64_t timeout_ns) {
    R_RETURN(ReplyAndReceiveWithUserBuffer(system, out_index, message_buffer, message_buffer_size,
                                           handles, num_handles, reply_target, timeout_ns));
}

} // namespace Kernel::Svc
