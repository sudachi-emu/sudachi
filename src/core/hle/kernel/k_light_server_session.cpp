// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/hle/kernel/k_light_server_session.h"
#include "core/hle/kernel/k_light_session.h"
#include "core/hle/kernel/k_thread.h"
#include "core/hle/kernel/k_thread_queue.h"
#include "core/hle/kernel/svc_results.h"

namespace Kernel {

namespace {

constexpr u64 InvalidThreadId = std::numeric_limits<u64>::max();

class ThreadQueueImplForKLightServerSessionRequest final : public KThreadQueue {
private:
    KThread::WaiterList* m_wait_list;

public:
    ThreadQueueImplForKLightServerSessionRequest(KernelCore& kernel, KThread::WaiterList* wl)
        : KThreadQueue(kernel), m_wait_list(wl) {}

    virtual void EndWait(KThread* waiting_thread, Result wait_result) override {
        // Remove the thread from our wait list.
        m_wait_list->erase(m_wait_list->iterator_to(*waiting_thread));

        // Invoke the base end wait handler.
        KThreadQueue::EndWait(waiting_thread, wait_result);
    }

    virtual void CancelWait(KThread* waiting_thread, Result wait_result,
                            bool cancel_timer_task) override {
        // Remove the thread from our wait list.
        m_wait_list->erase(m_wait_list->iterator_to(*waiting_thread));

        // Invoke the base cancel wait handler.
        KThreadQueue::CancelWait(waiting_thread, wait_result, cancel_timer_task);
    }
};

class ThreadQueueImplForKLightServerSessionReceive final : public KThreadQueue {
private:
    KThread** m_server_thread;

public:
    ThreadQueueImplForKLightServerSessionReceive(KernelCore& kernel, KThread** st)
        : KThreadQueue(kernel), m_server_thread(st) {}

    virtual void EndWait(KThread* waiting_thread, Result wait_result) override {
        // Clear the server thread.
        *m_server_thread = nullptr;

        // Set the waiting thread as not cancelable.
        waiting_thread->ClearCancellable();

        // Invoke the base end wait handler.
        KThreadQueue::EndWait(waiting_thread, wait_result);
    }

    virtual void CancelWait(KThread* waiting_thread, Result wait_result,
                            bool cancel_timer_task) override {
        // Clear the server thread.
        *m_server_thread = nullptr;

        // Set the waiting thread as not cancelable.
        waiting_thread->ClearCancellable();

        // Invoke the base cancel wait handler.
        KThreadQueue::CancelWait(waiting_thread, wait_result, cancel_timer_task);
    }
};

} // namespace

KLightServerSession::KLightServerSession(KernelCore& kernel) : KAutoObject(kernel) {}
KLightServerSession::~KLightServerSession() = default;

void KLightServerSession::Destroy() {
    this->CleanupRequests();

    m_parent->OnServerClosed();
}

void KLightServerSession::OnClientClosed() {
    this->CleanupRequests();
}

Result KLightServerSession::OnRequest(KThread* request_thread) {
    ThreadQueueImplForKLightServerSessionRequest wait_queue(m_kernel,
                                                            std::addressof(m_request_list));

    // Send the request.
    {
        // Lock the scheduler.
        KScopedSchedulerLock sl(m_kernel);

        // Check that the server isn't closed.
        R_UNLESS(!m_parent->IsServerClosed(), ResultSessionClosed);

        // Check that the request thread isn't terminating.
        R_UNLESS(!request_thread->IsTerminationRequested(), ResultTerminationRequested);

        // Add the request thread to our list.
        m_request_list.push_back(*request_thread);

        // Begin waiting on the request.
        request_thread->SetWaitReasonForDebugging(ThreadWaitReasonForDebugging::IPC);
        request_thread->BeginWait(std::addressof(wait_queue));

        // If we have a server thread, end its wait.
        if (m_server_thread != nullptr) {
            m_server_thread->EndWait(ResultSuccess);
        }
    }

    // NOTE: Nintendo returns GetCurrentThread().GetWaitResult() here.
    // This is technically incorrect, although it doesn't cause problems in practice
    // because this is only ever called with request_thread = GetCurrentThreadPointer().
    R_RETURN(request_thread->GetWaitResult());
}

Result KLightServerSession::ReplyAndReceive(u32* data) {
    // Set the server context.
    GetCurrentThread(m_kernel).SetLightSessionData(data);

    // Reply, if we need to.
    if (data[0] & KLightSession::ReplyFlag) {
        KScopedSchedulerLock sl(m_kernel);

        // Check that we're open.
        R_UNLESS(!m_parent->IsClientClosed(), ResultSessionClosed);
        R_UNLESS(!m_parent->IsServerClosed(), ResultSessionClosed);

        // Check that we have a request to reply to.
        R_UNLESS(m_current_request != nullptr, ResultInvalidState);

        // Check that the server thread id is correct.
        R_UNLESS(m_server_thread_id == GetCurrentThread(m_kernel).GetId(), ResultInvalidState);

        // If we can reply, do so.
        if (!m_current_request->IsTerminationRequested()) {
            std::memcpy(m_current_request->GetLightSessionData(),
                        GetCurrentThread(m_kernel).GetLightSessionData(), KLightSession::DataSize);
            m_current_request->EndWait(ResultSuccess);
        }

        // Close our current request.
        m_current_request->Close();

        // Clear our current request.
        m_current_request = nullptr;
        m_server_thread_id = InvalidThreadId;
    }

    // Create the wait queue for our receive.
    ThreadQueueImplForKLightServerSessionReceive wait_queue(m_kernel,
                                                            std::addressof(m_server_thread));

    // Receive.
    while (true) {
        // Try to receive a request.
        {
            KScopedSchedulerLock sl(m_kernel);

            // Check that we aren't already receiving.
            R_UNLESS(m_server_thread == nullptr, ResultInvalidState);
            R_UNLESS(m_server_thread_id == InvalidThreadId, ResultInvalidState);

            // Check that we're open.
            R_UNLESS(!m_parent->IsClientClosed(), ResultSessionClosed);
            R_UNLESS(!m_parent->IsServerClosed(), ResultSessionClosed);

            // Check that we're not terminating.
            R_UNLESS(!GetCurrentThread(m_kernel).IsTerminationRequested(),
                     ResultTerminationRequested);

            // If we have a request available, use it.
            if (auto head = m_request_list.begin(); head != m_request_list.end()) {
                // Set our current request.
                m_current_request = std::addressof(*head);
                m_current_request->Open();

                // Set our server thread id.
                m_server_thread_id = GetCurrentThread(m_kernel).GetId();

                // Copy the client request data.
                std::memcpy(GetCurrentThread(m_kernel).GetLightSessionData(),
                            m_current_request->GetLightSessionData(), KLightSession::DataSize);

                // We successfully received.
                R_SUCCEED();
            }

            // We need to wait for a request to come in.

            // Check if we were cancelled.
            if (GetCurrentThread(m_kernel).IsWaitCancelled()) {
                GetCurrentThread(m_kernel).ClearWaitCancelled();
                R_THROW(ResultCancelled);
            }

            // Mark ourselves as cancellable.
            GetCurrentThread(m_kernel).SetCancellable();

            // Wait for a request to come in.
            m_server_thread = GetCurrentThreadPointer(m_kernel);
            GetCurrentThread(m_kernel).SetWaitReasonForDebugging(ThreadWaitReasonForDebugging::IPC);
            GetCurrentThread(m_kernel).BeginWait(std::addressof(wait_queue));
        }

        // We waited to receive a request; if our wait failed, return the failing result.
        R_TRY(GetCurrentThread(m_kernel).GetWaitResult());
    }
}

void KLightServerSession::CleanupRequests() {
    // Cleanup all pending requests.
    {
        KScopedSchedulerLock sl(m_kernel);

        // Handle the current request.
        if (m_current_request != nullptr) {
            // Reply to the current request.
            if (!m_current_request->IsTerminationRequested()) {
                m_current_request->EndWait(ResultSessionClosed);
            }

            // Clear our current request.
            m_current_request->Close();
            m_current_request = nullptr;
            m_server_thread_id = InvalidThreadId;
        }

        // Reply to all other requests.
        for (auto& thread : m_request_list) {
            thread.EndWait(ResultSessionClosed);
        }

        // Wait up our server thread, if we have one.
        if (m_server_thread != nullptr) {
            m_server_thread->EndWait(ResultSessionClosed);
        }
    }
}

} // namespace Kernel
