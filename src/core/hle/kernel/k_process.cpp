// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <random>
#include "common/scope_exit.h"
#include "common/settings.h"
#include "core/arm/dynarmic/arm_dynarmic.h"
#include "core/arm/dynarmic/dynarmic_exclusive_monitor.h"
#include "core/core.h"
#include "core/hle/kernel/k_process.h"
#include "core/hle/kernel/k_scoped_resource_reservation.h"
#include "core/hle/kernel/k_shared_memory.h"
#include "core/hle/kernel/k_shared_memory_info.h"
#include "core/hle/kernel/k_thread_local_page.h"
#include "core/hle/kernel/k_thread_queue.h"
#include "core/hle/kernel/k_worker_task_manager.h"

#include "core/arm/dynarmic/arm_dynarmic_32.h"
#include "core/arm/dynarmic/arm_dynarmic_64.h"
#ifdef HAS_NCE
#include "core/arm/nce/arm_nce.h"
#endif

namespace Kernel {

namespace {

Result TerminateChildren(KernelCore& kernel, KProcess* process,
                         const KThread* thread_to_not_terminate) {
    // Request that all children threads terminate.
    {
        KScopedLightLock proc_lk(process->GetListLock());
        KScopedSchedulerLock sl(kernel);

        if (thread_to_not_terminate != nullptr &&
            process->GetPinnedThread(GetCurrentCoreId(kernel)) == thread_to_not_terminate) {
            // NOTE: Here Nintendo unpins the current thread instead of the thread_to_not_terminate.
            // This is valid because the only caller which uses non-nullptr as argument uses
            // GetCurrentThreadPointer(), but it's still notable because it seems incorrect at
            // first glance.
            process->UnpinCurrentThread();
        }

        auto& thread_list = process->GetThreadList();
        for (auto it = thread_list.begin(); it != thread_list.end(); ++it) {
            if (KThread* thread = std::addressof(*it); thread != thread_to_not_terminate) {
                if (thread->GetState() != ThreadState::Terminated) {
                    thread->RequestTerminate();
                }
            }
        }
    }

    // Wait for all children threads to terminate.
    while (true) {
        // Get the next child.
        KThread* cur_child = nullptr;
        {
            KScopedLightLock proc_lk(process->GetListLock());

            auto& thread_list = process->GetThreadList();
            for (auto it = thread_list.begin(); it != thread_list.end(); ++it) {
                if (KThread* thread = std::addressof(*it); thread != thread_to_not_terminate) {
                    if (thread->GetState() != ThreadState::Terminated) {
                        if (thread->Open()) {
                            cur_child = thread;
                            break;
                        }
                    }
                }
            }
        }

        // If we didn't find any non-terminated children, we're done.
        if (cur_child == nullptr) {
            break;
        }

        // Terminate and close the thread.
        SCOPE_EXIT {
            cur_child->Close();
        };

        if (const Result terminate_result = cur_child->Terminate();
            ResultTerminationRequested == terminate_result) {
            R_THROW(terminate_result);
        }
    }

    R_SUCCEED();
}

class ThreadQueueImplForKProcessEnterUserException final : public KThreadQueue {
private:
    KThread** m_exception_thread;

public:
    explicit ThreadQueueImplForKProcessEnterUserException(KernelCore& kernel, KThread** t)
        : KThreadQueue(kernel), m_exception_thread(t) {}

    virtual void EndWait(KThread* waiting_thread, Result wait_result) override {
        // Set the exception thread.
        *m_exception_thread = waiting_thread;

        // Invoke the base end wait handler.
        KThreadQueue::EndWait(waiting_thread, wait_result);
    }

    virtual void CancelWait(KThread* waiting_thread, Result wait_result,
                            bool cancel_timer_task) override {
        // Remove the thread as a waiter on its mutex owner.
        waiting_thread->GetLockOwner()->RemoveWaiter(waiting_thread);

        // Invoke the base cancel wait handler.
        KThreadQueue::CancelWait(waiting_thread, wait_result, cancel_timer_task);
    }
};

void GenerateRandom(std::span<u64> out_random) {
    std::mt19937 rng(Settings::values.rng_seed_enabled ? Settings::values.rng_seed.GetValue()
                                                       : static_cast<u32>(std::time(nullptr)));
    std::uniform_int_distribution<u64> distribution;
    std::generate(out_random.begin(), out_random.end(), [&] { return distribution(rng); });
}

} // namespace

void KProcess::Finalize() {
    // Delete the process local region.
    this->DeleteThreadLocalRegion(m_plr_address);

    // Get the used memory size.
    const size_t used_memory_size = this->GetUsedNonSystemUserPhysicalMemorySize();

    // Finalize the page table.
    m_page_table.Finalize();

    // Finish using our system resource.
    if (m_system_resource) {
        if (m_system_resource->IsSecureResource()) {
            // Finalize optimized memory. If memory wasn't optimized, this is a no-op.
            m_kernel.MemoryManager().FinalizeOptimizedMemory(this->GetId(), m_memory_pool);
        }

        m_system_resource->Close();
        m_system_resource = nullptr;
    }

    // Free all shared memory infos.
    {
        auto it = m_shared_memory_list.begin();
        while (it != m_shared_memory_list.end()) {
            KSharedMemoryInfo* info = std::addressof(*it);
            KSharedMemory* shmem = info->GetSharedMemory();

            while (!info->Close()) {
                shmem->Close();
            }
            shmem->Close();

            it = m_shared_memory_list.erase(it);
            KSharedMemoryInfo::Free(m_kernel, info);
        }
    }

    // Our thread local page list must be empty at this point.
    ASSERT(m_partially_used_tlp_tree.empty());
    ASSERT(m_fully_used_tlp_tree.empty());

    // Release memory to the resource limit.
    if (m_resource_limit != nullptr) {
        ASSERT(used_memory_size >= m_memory_release_hint);
        m_resource_limit->Release(Svc::LimitableResource::PhysicalMemoryMax, used_memory_size,
                                  used_memory_size - m_memory_release_hint);
        m_resource_limit->Close();
    }

    // Clear expensive resources, as the destructor is not called for guest objects.
    for (auto& interface : m_arm_interfaces) {
        interface.reset();
    }
    m_exclusive_monitor.reset();

    // Perform inherited finalization.
    KSynchronizationObject::Finalize();
}

Result KProcess::Initialize(const Svc::CreateProcessParameter& params, KResourceLimit* res_limit,
                            bool is_real) {
    // TODO: remove this special case
    if (is_real) {
        // Create and clear the process local region.
        R_TRY(this->CreateThreadLocalRegion(std::addressof(m_plr_address)));
        this->GetMemory().ZeroBlock(m_plr_address, Svc::ThreadLocalRegionSize);
    }

    // Copy in the name from parameters.
    static_assert(sizeof(params.name) < sizeof(m_name));
    std::memcpy(m_name.data(), params.name.data(), sizeof(params.name));
    m_name[sizeof(params.name)] = 0;

    // Set misc fields.
    m_state = State::Created;
    m_main_thread_stack_size = 0;
    m_used_kernel_memory_size = 0;
    m_ideal_core_id = 0;
    m_flags = params.flags;
    m_version = params.version;
    m_program_id = params.program_id;
    m_code_address = params.code_address;
    m_code_size = params.code_num_pages * PageSize;
    m_is_application = True(params.flags & Svc::CreateProcessFlag::IsApplication);

    // Set thread fields.
    for (size_t i = 0; i < Core::Hardware::NUM_CPU_CORES; i++) {
        m_running_threads[i] = nullptr;
        m_pinned_threads[i] = nullptr;
        m_running_thread_idle_counts[i] = 0;
        m_running_thread_switch_counts[i] = 0;
    }

    // Set max memory based on address space type.
    switch ((params.flags & Svc::CreateProcessFlag::AddressSpaceMask)) {
    case Svc::CreateProcessFlag::AddressSpace32Bit:
    case Svc::CreateProcessFlag::AddressSpace64BitDeprecated:
    case Svc::CreateProcessFlag::AddressSpace64Bit:
        m_max_process_memory = m_page_table.GetHeapRegionSize();
        break;
    case Svc::CreateProcessFlag::AddressSpace32BitWithoutAlias:
        m_max_process_memory = m_page_table.GetHeapRegionSize() + m_page_table.GetAliasRegionSize();
        break;
    default:
        UNREACHABLE();
    }

    // Generate random entropy.
    GenerateRandom(m_entropy);

    // Clear remaining fields.
    m_num_running_threads = 0;
    m_num_process_switches = 0;
    m_num_thread_switches = 0;
    m_num_fpu_switches = 0;
    m_num_supervisor_calls = 0;
    m_num_ipc_messages = 0;

    m_is_signaled = false;
    m_exception_thread = nullptr;
    m_is_suspended = false;
    m_memory_release_hint = 0;
    m_schedule_count = 0;
    m_is_handle_table_initialized = false;

    // Open a reference to our resource limit.
    m_resource_limit = res_limit;
    m_resource_limit->Open();

    // We're initialized!
    m_is_initialized = true;

    R_SUCCEED();
}

Result KProcess::Initialize(const Svc::CreateProcessParameter& params, const KPageGroup& pg,
                            std::span<const u32> caps, KResourceLimit* res_limit,
                            KMemoryManager::Pool pool, bool immortal) {
    ASSERT(res_limit != nullptr);
    ASSERT((params.code_num_pages * PageSize) / PageSize ==
           static_cast<size_t>(params.code_num_pages));

    // Set members.
    m_memory_pool = pool;
    m_is_default_application_system_resource = false;
    m_is_immortal = immortal;

    // Setup our system resource.
    if (const size_t system_resource_num_pages = params.system_resource_num_pages;
        system_resource_num_pages != 0) {
        // Create a secure system resource.
        KSecureSystemResource* secure_resource = KSecureSystemResource::Create(m_kernel);
        R_UNLESS(secure_resource != nullptr, ResultOutOfResource);

        ON_RESULT_FAILURE {
            secure_resource->Close();
        };

        // Initialize the secure resource.
        R_TRY(secure_resource->Initialize(system_resource_num_pages * PageSize, res_limit,
                                          m_memory_pool));

        // Set our system resource.
        m_system_resource = secure_resource;
    } else {
        // Use the system-wide system resource.
        const bool is_app = True(params.flags & Svc::CreateProcessFlag::IsApplication);
        m_system_resource = std::addressof(is_app ? m_kernel.GetAppSystemResource()
                                                  : m_kernel.GetSystemSystemResource());

        m_is_default_application_system_resource = is_app;

        // Open reference to the system resource.
        m_system_resource->Open();
    }

    // Ensure we clean up our secure resource, if we fail.
    ON_RESULT_FAILURE {
        m_system_resource->Close();
        m_system_resource = nullptr;
    };

    // Setup page table.
    {
        const auto as_type = params.flags & Svc::CreateProcessFlag::AddressSpaceMask;
        const bool enable_aslr = True(params.flags & Svc::CreateProcessFlag::EnableAslr);
        const bool enable_das_merge =
            False(params.flags & Svc::CreateProcessFlag::DisableDeviceAddressSpaceMerge);
        R_TRY(m_page_table.Initialize(as_type, enable_aslr, enable_das_merge, !enable_aslr, pool,
                                      params.code_address, params.code_num_pages * PageSize,
                                      m_system_resource, res_limit, m_memory, 0));
    }
    ON_RESULT_FAILURE_2 {
        m_page_table.Finalize();
    };

    // Ensure our memory is initialized.
    m_memory.SetCurrentPageTable(*this);
    m_memory.SetGPUDirtyManagers(m_kernel.System().GetGPUDirtyMemoryManager());

    // Ensure we can insert the code region.
    R_UNLESS(m_page_table.CanContain(params.code_address, params.code_num_pages * PageSize,
                                     KMemoryState::Code),
             ResultInvalidMemoryRegion);

    // Map the code region.
    R_TRY(m_page_table.MapPageGroup(params.code_address, pg, KMemoryState::Code,
                                    KMemoryPermission::KernelRead));

    // Initialize capabilities.
    R_TRY(m_capabilities.InitializeForKip(caps, std::addressof(m_page_table)));

    // Initialize the process id.
    m_process_id = m_kernel.CreateNewUserProcessID();
    ASSERT(InitialProcessIdMin <= m_process_id);
    ASSERT(m_process_id <= InitialProcessIdMax);

    // Initialize the rest of the process.
    R_TRY(this->Initialize(params, res_limit, true));

    // We succeeded!
    R_SUCCEED();
}

Result KProcess::Initialize(const Svc::CreateProcessParameter& params,
                            std::span<const u32> user_caps, KResourceLimit* res_limit,
                            KMemoryManager::Pool pool, KProcessAddress aslr_space_start) {
    ASSERT(res_limit != nullptr);

    // Set members.
    m_memory_pool = pool;
    m_is_default_application_system_resource = false;
    m_is_immortal = false;

    // Get the memory sizes.
    const size_t code_num_pages = params.code_num_pages;
    const size_t system_resource_num_pages = params.system_resource_num_pages;
    const size_t code_size = code_num_pages * PageSize;
    const size_t system_resource_size = system_resource_num_pages * PageSize;

    // Reserve memory for our code resource.
    KScopedResourceReservation memory_reservation(
        res_limit, Svc::LimitableResource::PhysicalMemoryMax, code_size);
    R_UNLESS(memory_reservation.Succeeded(), ResultLimitReached);

    // Setup our system resource.
    if (system_resource_num_pages != 0) {
        // Create a secure system resource.
        KSecureSystemResource* secure_resource = KSecureSystemResource::Create(m_kernel);
        R_UNLESS(secure_resource != nullptr, ResultOutOfResource);

        ON_RESULT_FAILURE {
            secure_resource->Close();
        };

        // Initialize the secure resource.
        R_TRY(secure_resource->Initialize(system_resource_size, res_limit, m_memory_pool));

        // Set our system resource.
        m_system_resource = secure_resource;

    } else {
        // Use the system-wide system resource.
        const bool is_app = True(params.flags & Svc::CreateProcessFlag::IsApplication);
        m_system_resource = std::addressof(is_app ? m_kernel.GetAppSystemResource()
                                                  : m_kernel.GetSystemSystemResource());

        m_is_default_application_system_resource = is_app;

        // Open reference to the system resource.
        m_system_resource->Open();
    }

    // Ensure we clean up our secure resource, if we fail.
    ON_RESULT_FAILURE {
        m_system_resource->Close();
        m_system_resource = nullptr;
    };

    // Setup page table.
    {
        const auto as_type = params.flags & Svc::CreateProcessFlag::AddressSpaceMask;
        const bool enable_aslr = True(params.flags & Svc::CreateProcessFlag::EnableAslr);
        const bool enable_das_merge =
            False(params.flags & Svc::CreateProcessFlag::DisableDeviceAddressSpaceMerge);
        R_TRY(m_page_table.Initialize(as_type, enable_aslr, enable_das_merge, !enable_aslr, pool,
                                      params.code_address, code_size, m_system_resource, res_limit,
                                      m_memory, aslr_space_start));
    }
    ON_RESULT_FAILURE_2 {
        m_page_table.Finalize();
    };

    // Ensure our memory is initialized.
    m_memory.SetCurrentPageTable(*this);
    m_memory.SetGPUDirtyManagers(m_kernel.System().GetGPUDirtyMemoryManager());

    // Ensure we can insert the code region.
    R_UNLESS(m_page_table.CanContain(params.code_address, code_size, KMemoryState::Code),
             ResultInvalidMemoryRegion);

    // Map the code region.
    R_TRY(m_page_table.MapPages(params.code_address, code_num_pages, KMemoryState::Code,
                                KMemoryPermission::KernelRead | KMemoryPermission::NotMapped));

    // Initialize capabilities.
    R_TRY(m_capabilities.InitializeForUser(user_caps, std::addressof(m_page_table)));

    // Initialize the process id.
    m_process_id = m_kernel.CreateNewUserProcessID();
    ASSERT(ProcessIdMin <= m_process_id);
    ASSERT(m_process_id <= ProcessIdMax);

    // If we should optimize memory allocations, do so.
    if (m_system_resource->IsSecureResource() &&
        True(params.flags & Svc::CreateProcessFlag::OptimizeMemoryAllocation)) {
        R_TRY(m_kernel.MemoryManager().InitializeOptimizedMemory(m_process_id, pool));
    }

    // Initialize the rest of the process.
    R_TRY(this->Initialize(params, res_limit, true));

    // We succeeded, so commit our memory reservation.
    memory_reservation.Commit();
    R_SUCCEED();
}

void KProcess::DoWorkerTaskImpl() {
    // Terminate child threads.
    TerminateChildren(m_kernel, this, nullptr);

    // Finalize the handle table, if we're not immortal.
    if (!m_is_immortal && m_is_handle_table_initialized) {
        this->FinalizeHandleTable();
    }

    // Finish termination.
    this->FinishTermination();
}

Result KProcess::StartTermination() {
    // Finalize the handle table when we're done, if the process isn't immortal.
    SCOPE_EXIT {
        if (!m_is_immortal) {
            this->FinalizeHandleTable();
        }
    };

    // Terminate child threads other than the current one.
    R_RETURN(TerminateChildren(m_kernel, this, GetCurrentThreadPointer(m_kernel)));
}

void KProcess::FinishTermination() {
    // Only allow termination to occur if the process isn't immortal.
    if (!m_is_immortal) {
        // Release resource limit hint.
        if (m_resource_limit != nullptr) {
            m_memory_release_hint = this->GetUsedNonSystemUserPhysicalMemorySize();
            m_resource_limit->Release(Svc::LimitableResource::PhysicalMemoryMax, 0,
                                      m_memory_release_hint);
        }

        // Change state.
        {
            KScopedSchedulerLock sl(m_kernel);
            this->ChangeState(State::Terminated);
        }

        // Close.
        this->Close();
    }
}

void KProcess::Exit() {
    // Determine whether we need to start terminating
    bool needs_terminate = false;
    {
        KScopedLightLock lk(m_state_lock);
        KScopedSchedulerLock sl(m_kernel);

        ASSERT(m_state != State::Created);
        ASSERT(m_state != State::CreatedAttached);
        ASSERT(m_state != State::Crashed);
        ASSERT(m_state != State::Terminated);
        if (m_state == State::Running || m_state == State::RunningAttached ||
            m_state == State::DebugBreak) {
            this->ChangeState(State::Terminating);
            needs_terminate = true;
        }
    }

    // If we need to start termination, do so.
    if (needs_terminate) {
        this->StartTermination();

        // Register the process as a work task.
        m_kernel.WorkerTaskManager().AddTask(m_kernel, KWorkerTaskManager::WorkerType::Exit, this);
    }

    // Exit the current thread.
    GetCurrentThread(m_kernel).Exit();
}

Result KProcess::Terminate() {
    // Determine whether we need to start terminating.
    bool needs_terminate = false;
    {
        KScopedLightLock lk(m_state_lock);

        // Check whether we're allowed to terminate.
        R_UNLESS(m_state != State::Created, ResultInvalidState);
        R_UNLESS(m_state != State::CreatedAttached, ResultInvalidState);

        KScopedSchedulerLock sl(m_kernel);

        if (m_state == State::Running || m_state == State::RunningAttached ||
            m_state == State::Crashed || m_state == State::DebugBreak) {
            this->ChangeState(State::Terminating);
            needs_terminate = true;
        }
    }

    // If we need to terminate, do so.
    if (needs_terminate) {
        // Start termination.
        if (R_SUCCEEDED(this->StartTermination())) {
            // Finish termination.
            this->FinishTermination();
        } else {
            // Register the process as a work task.
            m_kernel.WorkerTaskManager().AddTask(m_kernel, KWorkerTaskManager::WorkerType::Exit,
                                                 this);
        }
    }

    R_SUCCEED();
}

Result KProcess::AddSharedMemory(KSharedMemory* shmem, KProcessAddress address, size_t size) {
    // Lock ourselves, to prevent concurrent access.
    KScopedLightLock lk(m_state_lock);

    // Try to find an existing info for the memory.
    KSharedMemoryInfo* info = nullptr;
    for (auto it = m_shared_memory_list.begin(); it != m_shared_memory_list.end(); ++it) {
        if (it->GetSharedMemory() == shmem) {
            info = std::addressof(*it);
            break;
        }
    }

    // If we didn't find an info, create one.
    if (info == nullptr) {
        // Allocate a new info.
        info = KSharedMemoryInfo::Allocate(m_kernel);
        R_UNLESS(info != nullptr, ResultOutOfResource);

        // Initialize the info and add it to our list.
        info->Initialize(shmem);
        m_shared_memory_list.push_back(*info);
    }

    // Open a reference to the shared memory and its info.
    shmem->Open();
    info->Open();

    R_SUCCEED();
}

void KProcess::RemoveSharedMemory(KSharedMemory* shmem, KProcessAddress address, size_t size) {
    // Lock ourselves, to prevent concurrent access.
    KScopedLightLock lk(m_state_lock);

    // Find an existing info for the memory.
    KSharedMemoryInfo* info = nullptr;
    auto it = m_shared_memory_list.begin();
    for (; it != m_shared_memory_list.end(); ++it) {
        if (it->GetSharedMemory() == shmem) {
            info = std::addressof(*it);
            break;
        }
    }
    ASSERT(info != nullptr);

    // Close a reference to the info and its memory.
    if (info->Close()) {
        m_shared_memory_list.erase(it);
        KSharedMemoryInfo::Free(m_kernel, info);
    }

    shmem->Close();
}

Result KProcess::CreateThreadLocalRegion(KProcessAddress* out) {
    KThreadLocalPage* tlp = nullptr;
    KProcessAddress tlr = 0;

    // See if we can get a region from a partially used TLP.
    {
        KScopedSchedulerLock sl(m_kernel);

        if (auto it = m_partially_used_tlp_tree.begin(); it != m_partially_used_tlp_tree.end()) {
            tlr = it->Reserve();
            ASSERT(tlr != 0);

            if (it->IsAllUsed()) {
                tlp = std::addressof(*it);
                m_partially_used_tlp_tree.erase(it);
                m_fully_used_tlp_tree.insert(*tlp);
            }

            *out = tlr;
            R_SUCCEED();
        }
    }

    // Allocate a new page.
    tlp = KThreadLocalPage::Allocate(m_kernel);
    R_UNLESS(tlp != nullptr, ResultOutOfMemory);
    ON_RESULT_FAILURE {
        KThreadLocalPage::Free(m_kernel, tlp);
    };

    // Initialize the new page.
    R_TRY(tlp->Initialize(m_kernel, this));

    // Reserve a TLR.
    tlr = tlp->Reserve();
    ASSERT(tlr != 0);

    // Insert into our tree.
    {
        KScopedSchedulerLock sl(m_kernel);
        if (tlp->IsAllUsed()) {
            m_fully_used_tlp_tree.insert(*tlp);
        } else {
            m_partially_used_tlp_tree.insert(*tlp);
        }
    }

    // We succeeded!
    *out = tlr;
    R_SUCCEED();
}

Result KProcess::DeleteThreadLocalRegion(KProcessAddress addr) {
    KThreadLocalPage* page_to_free = nullptr;

    // Release the region.
    {
        KScopedSchedulerLock sl(m_kernel);

        // Try to find the page in the partially used list.
        auto it = m_partially_used_tlp_tree.find_key(Common::AlignDown(GetInteger(addr), PageSize));
        if (it == m_partially_used_tlp_tree.end()) {
            // If we don't find it, it has to be in the fully used list.
            it = m_fully_used_tlp_tree.find_key(Common::AlignDown(GetInteger(addr), PageSize));
            R_UNLESS(it != m_fully_used_tlp_tree.end(), ResultInvalidAddress);

            // Release the region.
            it->Release(addr);

            // Move the page out of the fully used list.
            KThreadLocalPage* tlp = std::addressof(*it);
            m_fully_used_tlp_tree.erase(it);
            if (tlp->IsAllFree()) {
                page_to_free = tlp;
            } else {
                m_partially_used_tlp_tree.insert(*tlp);
            }
        } else {
            // Release the region.
            it->Release(addr);

            // Handle the all-free case.
            KThreadLocalPage* tlp = std::addressof(*it);
            if (tlp->IsAllFree()) {
                m_partially_used_tlp_tree.erase(it);
                page_to_free = tlp;
            }
        }
    }

    // If we should free the page it was in, do so.
    if (page_to_free != nullptr) {
        page_to_free->Finalize();

        KThreadLocalPage::Free(m_kernel, page_to_free);
    }

    R_SUCCEED();
}

bool KProcess::ReserveResource(Svc::LimitableResource which, s64 value) {
    if (KResourceLimit* rl = this->GetResourceLimit(); rl != nullptr) {
        return rl->Reserve(which, value);
    } else {
        return true;
    }
}

bool KProcess::ReserveResource(Svc::LimitableResource which, s64 value, s64 timeout) {
    if (KResourceLimit* rl = this->GetResourceLimit(); rl != nullptr) {
        return rl->Reserve(which, value, timeout);
    } else {
        return true;
    }
}

void KProcess::ReleaseResource(Svc::LimitableResource which, s64 value) {
    if (KResourceLimit* rl = this->GetResourceLimit(); rl != nullptr) {
        rl->Release(which, value);
    }
}

void KProcess::ReleaseResource(Svc::LimitableResource which, s64 value, s64 hint) {
    if (KResourceLimit* rl = this->GetResourceLimit(); rl != nullptr) {
        rl->Release(which, value, hint);
    }
}

void KProcess::IncrementRunningThreadCount() {
    ASSERT(m_num_running_threads.load() >= 0);

    ++m_num_running_threads;
}

void KProcess::DecrementRunningThreadCount() {
    ASSERT(m_num_running_threads.load() > 0);

    if (const auto prev = m_num_running_threads--; prev == 1) {
        this->Terminate();
    }
}

bool KProcess::EnterUserException() {
    // Get the current thread.
    KThread* cur_thread = GetCurrentThreadPointer(m_kernel);
    ASSERT(this == cur_thread->GetOwnerProcess());

    // Check that we haven't already claimed the exception thread.
    if (m_exception_thread == cur_thread) {
        return false;
    }

    // Create the wait queue we'll be using.
    ThreadQueueImplForKProcessEnterUserException wait_queue(m_kernel,
                                                            std::addressof(m_exception_thread));

    // Claim the exception thread.
    {
        // Lock the scheduler.
        KScopedSchedulerLock sl(m_kernel);

        // Check that we're not terminating.
        if (cur_thread->IsTerminationRequested()) {
            return false;
        }

        // If we don't have an exception thread, we can just claim it directly.
        if (m_exception_thread == nullptr) {
            m_exception_thread = cur_thread;
            KScheduler::SetSchedulerUpdateNeeded(m_kernel);
            return true;
        }

        // Otherwise, we need to wait until we don't have an exception thread.

        // Add the current thread as a waiter on the current exception thread.
        cur_thread->SetKernelAddressKey(
            reinterpret_cast<uintptr_t>(std::addressof(m_exception_thread)) | 1);
        m_exception_thread->AddWaiter(cur_thread);

        // Wait to claim the exception thread.
        cur_thread->BeginWait(std::addressof(wait_queue));
    }

    // If our wait didn't end due to thread termination, we succeeded.
    return ResultTerminationRequested != cur_thread->GetWaitResult();
}

bool KProcess::LeaveUserException() {
    return this->ReleaseUserException(GetCurrentThreadPointer(m_kernel));
}

bool KProcess::ReleaseUserException(KThread* thread) {
    KScopedSchedulerLock sl(m_kernel);

    if (m_exception_thread == thread) {
        m_exception_thread = nullptr;

        // Remove waiter thread.
        bool has_waiters;
        if (KThread* next = thread->RemoveKernelWaiterByKey(
                std::addressof(has_waiters),
                reinterpret_cast<uintptr_t>(std::addressof(m_exception_thread)) | 1);
            next != nullptr) {
            next->EndWait(ResultSuccess);
        }

        KScheduler::SetSchedulerUpdateNeeded(m_kernel);

        return true;
    } else {
        return false;
    }
}

void KProcess::RegisterThread(KThread* thread) {
    KScopedLightLock lk(m_list_lock);

    m_thread_list.push_back(*thread);
}

void KProcess::UnregisterThread(KThread* thread) {
    KScopedLightLock lk(m_list_lock);

    m_thread_list.erase(m_thread_list.iterator_to(*thread));
}

size_t KProcess::GetUsedUserPhysicalMemorySize() const {
    const size_t norm_size = m_page_table.GetNormalMemorySize();
    const size_t other_size = m_code_size + m_main_thread_stack_size;
    const size_t sec_size = this->GetRequiredSecureMemorySizeNonDefault();

    return norm_size + other_size + sec_size;
}

size_t KProcess::GetTotalUserPhysicalMemorySize() const {
    // Get the amount of free and used size.
    const size_t free_size =
        m_resource_limit->GetFreeValue(Svc::LimitableResource::PhysicalMemoryMax);
    const size_t max_size = m_max_process_memory;

    // Determine used size.
    // NOTE: This does *not* check this->IsDefaultApplicationSystemResource(), unlike
    // GetUsedUserPhysicalMemorySize().
    const size_t norm_size = m_page_table.GetNormalMemorySize();
    const size_t other_size = m_code_size + m_main_thread_stack_size;
    const size_t sec_size = this->GetRequiredSecureMemorySize();
    const size_t used_size = norm_size + other_size + sec_size;

    // NOTE: These function calls will recalculate, introducing a race...it is unclear why Nintendo
    // does it this way.
    if (used_size + free_size > max_size) {
        return max_size;
    } else {
        return free_size + this->GetUsedUserPhysicalMemorySize();
    }
}

size_t KProcess::GetUsedNonSystemUserPhysicalMemorySize() const {
    const size_t norm_size = m_page_table.GetNormalMemorySize();
    const size_t other_size = m_code_size + m_main_thread_stack_size;

    return norm_size + other_size;
}

size_t KProcess::GetTotalNonSystemUserPhysicalMemorySize() const {
    // Get the amount of free and used size.
    const size_t free_size =
        m_resource_limit->GetFreeValue(Svc::LimitableResource::PhysicalMemoryMax);
    const size_t max_size = m_max_process_memory;

    // Determine used size.
    // NOTE: This does *not* check this->IsDefaultApplicationSystemResource(), unlike
    // GetUsedUserPhysicalMemorySize().
    const size_t norm_size = m_page_table.GetNormalMemorySize();
    const size_t other_size = m_code_size + m_main_thread_stack_size;
    const size_t sec_size = this->GetRequiredSecureMemorySize();
    const size_t used_size = norm_size + other_size + sec_size;

    // NOTE: These function calls will recalculate, introducing a race...it is unclear why Nintendo
    // does it this way.
    if (used_size + free_size > max_size) {
        return max_size - this->GetRequiredSecureMemorySizeNonDefault();
    } else {
        return free_size + this->GetUsedNonSystemUserPhysicalMemorySize();
    }
}

Result KProcess::Run(s32 priority, size_t stack_size) {
    // Lock ourselves, to prevent concurrent access.
    KScopedLightLock lk(m_state_lock);

    // Validate that we're in a state where we can initialize.
    const auto state = m_state;
    R_UNLESS(state == State::Created || state == State::CreatedAttached, ResultInvalidState);

    // Place a tentative reservation of a thread for this process.
    KScopedResourceReservation thread_reservation(this, Svc::LimitableResource::ThreadCountMax);
    R_UNLESS(thread_reservation.Succeeded(), ResultLimitReached);

    // Ensure that we haven't already allocated stack.
    ASSERT(m_main_thread_stack_size == 0);

    // Ensure that we're allocating a valid stack.
    stack_size = Common::AlignUp(stack_size, PageSize);
    R_UNLESS(stack_size + m_code_size <= m_max_process_memory, ResultOutOfMemory);
    R_UNLESS(stack_size + m_code_size >= m_code_size, ResultOutOfMemory);

    // Place a tentative reservation of memory for our new stack.
    KScopedResourceReservation mem_reservation(this, Svc::LimitableResource::PhysicalMemoryMax,
                                               stack_size);
    R_UNLESS(mem_reservation.Succeeded(), ResultLimitReached);

    // Allocate and map our stack.
    KProcessAddress stack_top = 0;
    if (stack_size) {
        KProcessAddress stack_bottom;
        R_TRY(m_page_table.MapPages(std::addressof(stack_bottom), stack_size / PageSize,
                                    KMemoryState::Stack, KMemoryPermission::UserReadWrite));

        stack_top = stack_bottom + stack_size;
        m_main_thread_stack_size = stack_size;
    }

    // Ensure our stack is safe to clean up on exit.
    ON_RESULT_FAILURE {
        if (m_main_thread_stack_size) {
            ASSERT(R_SUCCEEDED(m_page_table.UnmapPages(stack_top - m_main_thread_stack_size,
                                                       m_main_thread_stack_size / PageSize,
                                                       KMemoryState::Stack)));
            m_main_thread_stack_size = 0;
        }
    };

    // Set our maximum heap size.
    R_TRY(m_page_table.SetMaxHeapSize(m_max_process_memory -
                                      (m_main_thread_stack_size + m_code_size)));

    // Initialize our handle table.
    R_TRY(this->InitializeHandleTable(m_capabilities.GetHandleTableSize()));
    ON_RESULT_FAILURE_2 {
        this->FinalizeHandleTable();
    };

    // Create a new thread for the process.
    KThread* main_thread = KThread::Create(m_kernel);
    R_UNLESS(main_thread != nullptr, ResultOutOfResource);
    SCOPE_EXIT {
        main_thread->Close();
    };

    // Initialize the thread.
    R_TRY(KThread::InitializeUserThread(m_kernel.System(), main_thread, this->GetEntryPoint(), 0,
                                        stack_top, priority, m_ideal_core_id, this));

    // Register the thread, and commit our reservation.
    KThread::Register(m_kernel, main_thread);
    thread_reservation.Commit();

    // Add the thread to our handle table.
    Handle thread_handle;
    R_TRY(m_handle_table.Add(std::addressof(thread_handle), main_thread));

    // Set the thread arguments.
    main_thread->GetContext().r[0] = 0;
    main_thread->GetContext().r[1] = thread_handle;

    // Update our state.
    this->ChangeState((state == State::Created) ? State::Running : State::RunningAttached);
    ON_RESULT_FAILURE_2 {
        this->ChangeState(state);
    };

    // Suspend for debug, if we should.
    if (m_kernel.System().DebuggerEnabled()) {
        main_thread->RequestSuspend(SuspendType::Debug);
    }

    // Run our thread.
    R_TRY(main_thread->Run());

    // Open a reference to represent that we're running.
    this->Open();

    // We succeeded! Commit our memory reservation.
    mem_reservation.Commit();

    R_SUCCEED();
}

Result KProcess::Reset() {
    // Lock the process and the scheduler.
    KScopedLightLock lk(m_state_lock);
    KScopedSchedulerLock sl(m_kernel);

    // Validate that we're in a state that we can reset.
    R_UNLESS(m_state != State::Terminated, ResultInvalidState);
    R_UNLESS(m_is_signaled, ResultInvalidState);

    // Clear signaled.
    m_is_signaled = false;
    R_SUCCEED();
}

Result KProcess::SetActivity(Svc::ProcessActivity activity) {
    // Lock ourselves and the scheduler.
    KScopedLightLock lk(m_state_lock);
    KScopedLightLock list_lk(m_list_lock);
    KScopedSchedulerLock sl(m_kernel);

    // Validate our state.
    R_UNLESS(m_state != State::Terminating, ResultInvalidState);
    R_UNLESS(m_state != State::Terminated, ResultInvalidState);

    // Either pause or resume.
    if (activity == Svc::ProcessActivity::Paused) {
        // Verify that we're not suspended.
        R_UNLESS(!m_is_suspended, ResultInvalidState);

        // Suspend all threads.
        auto end = this->GetThreadList().end();
        for (auto it = this->GetThreadList().begin(); it != end; ++it) {
            it->RequestSuspend(SuspendType::Process);
        }

        // Set ourselves as suspended.
        this->SetSuspended(true);
    } else {
        ASSERT(activity == Svc::ProcessActivity::Runnable);

        // Verify that we're suspended.
        R_UNLESS(m_is_suspended, ResultInvalidState);

        // Resume all threads.
        auto end = this->GetThreadList().end();
        for (auto it = this->GetThreadList().begin(); it != end; ++it) {
            it->Resume(SuspendType::Process);
        }

        // Set ourselves as resumed.
        this->SetSuspended(false);
    }

    R_SUCCEED();
}

void KProcess::PinCurrentThread() {
    ASSERT(KScheduler::IsSchedulerLockedByCurrentThread(m_kernel));

    // Get the current thread.
    const s32 core_id = GetCurrentCoreId(m_kernel);
    KThread* cur_thread = GetCurrentThreadPointer(m_kernel);

    // If the thread isn't terminated, pin it.
    if (!cur_thread->IsTerminationRequested()) {
        // Pin it.
        this->PinThread(core_id, cur_thread);
        cur_thread->Pin(core_id);

        // An update is needed.
        KScheduler::SetSchedulerUpdateNeeded(m_kernel);
    }
}

void KProcess::UnpinCurrentThread() {
    ASSERT(KScheduler::IsSchedulerLockedByCurrentThread(m_kernel));

    // Get the current thread.
    const s32 core_id = GetCurrentCoreId(m_kernel);
    KThread* cur_thread = GetCurrentThreadPointer(m_kernel);

    // Unpin it.
    cur_thread->Unpin();
    this->UnpinThread(core_id, cur_thread);

    // An update is needed.
    KScheduler::SetSchedulerUpdateNeeded(m_kernel);
}

void KProcess::UnpinThread(KThread* thread) {
    ASSERT(KScheduler::IsSchedulerLockedByCurrentThread(m_kernel));

    // Get the thread's core id.
    const auto core_id = thread->GetActiveCore();

    // Unpin it.
    this->UnpinThread(core_id, thread);
    thread->Unpin();

    // An update is needed.
    KScheduler::SetSchedulerUpdateNeeded(m_kernel);
}

Result KProcess::GetThreadList(s32* out_num_threads, KProcessAddress out_thread_ids,
                               s32 max_out_count) {
    auto& memory = this->GetMemory();

    // Lock the list.
    KScopedLightLock lk(m_list_lock);

    // Iterate over the list.
    s32 count = 0;
    auto end = this->GetThreadList().end();
    for (auto it = this->GetThreadList().begin(); it != end; ++it) {
        // If we're within array bounds, write the id.
        if (count < max_out_count) {
            // Get the thread id.
            KThread* thread = std::addressof(*it);
            const u64 id = thread->GetId();

            // Copy the id to userland.
            memory.Write64(out_thread_ids + count * sizeof(u64), id);
        }

        // Increment the count.
        ++count;
    }

    // We successfully iterated the list.
    *out_num_threads = count;
    R_SUCCEED();
}

void KProcess::Switch(KProcess* cur_process, KProcess* next_process) {}

KProcess::KProcess(KernelCore& kernel)
    : KAutoObjectWithSlabHeapAndContainer(kernel), m_page_table{kernel}, m_state_lock{kernel},
      m_list_lock{kernel}, m_cond_var{kernel.System()}, m_address_arbiter{kernel.System()},
      m_handle_table{kernel}, m_exclusive_monitor{}, m_memory{kernel.System()} {}
KProcess::~KProcess() = default;

Result KProcess::LoadFromMetadata(const FileSys::ProgramMetadata& metadata, std::size_t code_size,
                                  KProcessAddress aslr_space_start, bool is_hbl) {
    // Create a resource limit for the process.
    const auto pool = static_cast<KMemoryManager::Pool>(metadata.GetPoolPartition());
    const auto physical_memory_size = m_kernel.MemoryManager().GetSize(pool);
    auto* res_limit =
        Kernel::CreateResourceLimitForProcess(m_kernel.System(), physical_memory_size);

    // Ensure we maintain a clean state on exit.
    SCOPE_EXIT {
        res_limit->Close();
    };

    // Declare flags and code address.
    Svc::CreateProcessFlag flag{};
    u64 code_address{};

    // Determine if we are an application.
    if (pool == KMemoryManager::Pool::Application) {
        flag |= Svc::CreateProcessFlag::IsApplication;
        m_is_application = true;
    }

    // If we are 64-bit, create as such.
    if (metadata.Is64BitProgram()) {
        flag |= Svc::CreateProcessFlag::Is64Bit;
    }

    // Set the address space type and code address.
    switch (metadata.GetAddressSpaceType()) {
    case FileSys::ProgramAddressSpaceType::Is39Bit:
        flag |= Svc::CreateProcessFlag::AddressSpace64Bit;

        // For 39-bit processes, the ASLR region starts at 0x800'0000 and is ~512GiB large.
        // However, some (buggy) programs/libraries like skyline incorrectly depend on the
        // existence of ASLR pages before the entry point, so we will adjust the load address
        // to point to about 2GiB into the ASLR region.
        code_address = 0x8000'0000;
        break;
    case FileSys::ProgramAddressSpaceType::Is36Bit:
        flag |= Svc::CreateProcessFlag::AddressSpace64BitDeprecated;
        code_address = 0x800'0000;
        break;
    case FileSys::ProgramAddressSpaceType::Is32Bit:
        flag |= Svc::CreateProcessFlag::AddressSpace32Bit;
        code_address = 0x20'0000;
        break;
    case FileSys::ProgramAddressSpaceType::Is32BitNoMap:
        flag |= Svc::CreateProcessFlag::AddressSpace32BitWithoutAlias;
        code_address = 0x20'0000;
        break;
    }

    Svc::CreateProcessParameter params{
        .name = {},
        .version = {},
        .program_id = metadata.GetTitleID(),
        .code_address = code_address + GetInteger(aslr_space_start),
        .code_num_pages = static_cast<s32>(code_size / PageSize),
        .flags = flag,
        .reslimit = Svc::InvalidHandle,
        .system_resource_num_pages = static_cast<s32>(metadata.GetSystemResourceSize() / PageSize),
    };

    // Set the process name.
    const auto& name = metadata.GetName();
    static_assert(sizeof(params.name) <= sizeof(name));
    std::memcpy(params.name.data(), name.data(), sizeof(params.name));

    // Initialize for application process.
    R_TRY(this->Initialize(params, metadata.GetKernelCapabilities(), res_limit, pool,
                           aslr_space_start));

    // Assign remaining properties.
    m_is_hbl = is_hbl;
    m_ideal_core_id = metadata.GetMainThreadCore();

    // Set up emulation context.
    this->InitializeInterfaces();

    // We succeeded.
    R_SUCCEED();
}

void KProcess::LoadModule(CodeSet code_set, KProcessAddress base_addr) {
    const auto ReprotectSegment = [&](const CodeSet::Segment& segment,
                                      Svc::MemoryPermission permission) {
        m_page_table.SetProcessMemoryPermission(segment.addr + base_addr, segment.size, permission);
    };

    this->GetMemory().WriteBlock(base_addr, code_set.memory.data(), code_set.memory.size());

    ReprotectSegment(code_set.CodeSegment(), Svc::MemoryPermission::ReadExecute);
    ReprotectSegment(code_set.RODataSegment(), Svc::MemoryPermission::Read);
    ReprotectSegment(code_set.DataSegment(), Svc::MemoryPermission::ReadWrite);

#ifdef HAS_NCE
    const auto& patch = code_set.PatchSegment();
    if (this->IsApplication() && Settings::IsNceEnabled() && patch.size != 0) {
        auto& buffer = m_kernel.System().DeviceMemory().buffer;
        const auto& code = code_set.CodeSegment();
        buffer.Protect(GetInteger(base_addr + code.addr), code.size,
                       Common::MemoryPermission::Read | Common::MemoryPermission::Execute);
        buffer.Protect(GetInteger(base_addr + patch.addr), patch.size,
                       Common::MemoryPermission::Read | Common::MemoryPermission::Execute);
        ReprotectSegment(code_set.PatchSegment(), Svc::MemoryPermission::None);
    }
#endif
}

void KProcess::InitializeInterfaces() {
    m_exclusive_monitor =
        Core::MakeExclusiveMonitor(this->GetMemory(), Core::Hardware::NUM_CPU_CORES);

#ifdef HAS_NCE
    if (this->IsApplication() && Settings::IsNceEnabled()) {
        // Register the scoped JIT handler before creating any NCE instances
        // so that its signal handler will appear first in the signal chain.
        Core::ScopedJitExecution::RegisterHandler();

        for (size_t i = 0; i < Core::Hardware::NUM_CPU_CORES; i++) {
            m_arm_interfaces[i] = std::make_unique<Core::ArmNce>(m_kernel.System(), true, i);
        }
    } else
#endif
        if (this->Is64Bit()) {
        for (size_t i = 0; i < Core::Hardware::NUM_CPU_CORES; i++) {
            m_arm_interfaces[i] = std::make_unique<Core::ArmDynarmic64>(
                m_kernel.System(), m_kernel.IsMulticore(), this,
                static_cast<Core::DynarmicExclusiveMonitor&>(*m_exclusive_monitor), i);
        }
    } else {
        for (size_t i = 0; i < Core::Hardware::NUM_CPU_CORES; i++) {
            m_arm_interfaces[i] = std::make_unique<Core::ArmDynarmic32>(
                m_kernel.System(), m_kernel.IsMulticore(), this,
                static_cast<Core::DynarmicExclusiveMonitor&>(*m_exclusive_monitor), i);
        }
    }
}

bool KProcess::InsertWatchpoint(KProcessAddress addr, u64 size, DebugWatchpointType type) {
    const auto watch{std::find_if(m_watchpoints.begin(), m_watchpoints.end(), [&](const auto& wp) {
        return wp.type == DebugWatchpointType::None;
    })};

    if (watch == m_watchpoints.end()) {
        return false;
    }

    watch->start_address = addr;
    watch->end_address = addr + size;
    watch->type = type;

    for (KProcessAddress page = Common::AlignDown(GetInteger(addr), PageSize); page < addr + size;
         page += PageSize) {
        m_debug_page_refcounts[page]++;
        this->GetMemory().MarkRegionDebug(page, PageSize, true);
    }

    return true;
}

bool KProcess::RemoveWatchpoint(KProcessAddress addr, u64 size, DebugWatchpointType type) {
    const auto watch{std::find_if(m_watchpoints.begin(), m_watchpoints.end(), [&](const auto& wp) {
        return wp.start_address == addr && wp.end_address == addr + size && wp.type == type;
    })};

    if (watch == m_watchpoints.end()) {
        return false;
    }

    watch->start_address = 0;
    watch->end_address = 0;
    watch->type = DebugWatchpointType::None;

    for (KProcessAddress page = Common::AlignDown(GetInteger(addr), PageSize); page < addr + size;
         page += PageSize) {
        m_debug_page_refcounts[page]--;
        if (!m_debug_page_refcounts[page]) {
            this->GetMemory().MarkRegionDebug(page, PageSize, false);
        }
    }

    return true;
}

} // namespace Kernel
