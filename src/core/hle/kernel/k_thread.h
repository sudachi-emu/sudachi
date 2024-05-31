// SPDX-FileCopyrightText: Copyright 2021 sudachi Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "common/intrusive_list.h"

#include "common/intrusive_red_black_tree.h"
#include "common/scratch_buffer.h"
#include "common/spin_lock.h"
#include "core/arm/arm_interface.h"
#include "core/hle/kernel/k_affinity_mask.h"
#include "core/hle/kernel/k_light_lock.h"
#include "core/hle/kernel/k_spin_lock.h"
#include "core/hle/kernel/k_synchronization_object.h"
#include "core/hle/kernel/k_timer_task.h"
#include "core/hle/kernel/k_typed_address.h"
#include "core/hle/kernel/k_worker_task.h"
#include "core/hle/kernel/slab_helpers.h"
#include "core/hle/kernel/svc_common.h"
#include "core/hle/kernel/svc_types.h"
#include "core/hle/result.h"

namespace Common {
class Fiber;
}

namespace Core {
namespace Memory {
class Memory;
}
class System;
} // namespace Core

namespace Kernel {

class GlobalSchedulerContext;
class KernelCore;
class KProcess;
class KScheduler;
class KThreadQueue;

using KThreadFunction = KProcessAddress;

enum class ThreadType : u32 {
    Main = 0,
    Kernel = 1,
    HighPriority = 2,
    User = 3,
    Dummy = 100, // Special thread type for emulation purposes only
};
DECLARE_ENUM_FLAG_OPERATORS(ThreadType);

enum class SuspendType : u32 {
    Process = 0,
    Thread = 1,
    Debug = 2,
    Backtrace = 3,
    Init = 4,
    System = 5,

    Count,
};

enum class ThreadState : u16 {
    Initialized = 0,
    Waiting = 1,
    Runnable = 2,
    Terminated = 3,

    SuspendShift = 4,
    Mask = (1 << SuspendShift) - 1,

    ProcessSuspended = (1 << (0 + SuspendShift)),
    ThreadSuspended = (1 << (1 + SuspendShift)),
    DebugSuspended = (1 << (2 + SuspendShift)),
    BacktraceSuspended = (1 << (3 + SuspendShift)),
    InitSuspended = (1 << (4 + SuspendShift)),
    SystemSuspended = (1 << (5 + SuspendShift)),

    SuspendFlagMask = ((1 << 6) - 1) << SuspendShift,
};
DECLARE_ENUM_FLAG_OPERATORS(ThreadState);

enum class DpcFlag : u32 {
    Terminating = (1 << 0),
    Terminated = (1 << 1),
};

enum class ThreadWaitReasonForDebugging : u32 {
    None,            ///< Thread is not waiting
    Sleep,           ///< Thread is waiting due to a SleepThread SVC
    IPC,             ///< Thread is waiting for the reply from an IPC request
    Synchronization, ///< Thread is waiting due to a WaitSynchronization SVC
    ConditionVar,    ///< Thread is waiting due to a WaitProcessWideKey SVC
    Arbitration,     ///< Thread is waiting due to a SignalToAddress/WaitForAddress SVC
    Suspended,       ///< Thread is waiting due to process suspension
};

enum class StepState : u32 {
    NotStepping,   ///< Thread is not currently stepping
    StepPending,   ///< Thread will step when next scheduled
    StepPerformed, ///< Thread has stepped, waiting to be scheduled again
};

void SetCurrentThread(KernelCore& kernel, KThread* thread);
KThread* GetCurrentThreadPointer(KernelCore& kernel);
KThread& GetCurrentThread(KernelCore& kernel);
KProcess* GetCurrentProcessPointer(KernelCore& kernel);
KProcess& GetCurrentProcess(KernelCore& kernel);
s32 GetCurrentCoreId(KernelCore& kernel);
Core::Memory::Memory& GetCurrentMemory(KernelCore& kernel);

class KThread final : public KAutoObjectWithSlabHeapAndContainer<KThread, KWorkerTask>,
                      public Common::IntrusiveListBaseNode<KThread>,
                      public KTimerTask {
    KERNEL_AUTOOBJECT_TRAITS(KThread, KSynchronizationObject);

private:
    friend class KScheduler;
    friend class KProcess;

public:
    static constexpr s32 DefaultThreadPriority = 44;
    static constexpr s32 IdleThreadPriority = Svc::LowestThreadPriority + 1;
    static constexpr s32 DummyThreadPriority = Svc::LowestThreadPriority + 2;

    explicit KThread(KernelCore& kernel);
    ~KThread() override;

public:
    using WaiterList = Common::IntrusiveListBaseTraits<KThread>::ListType;

    /**
     * Gets the thread's current priority
     * @return The current thread's priority
     */
    s32 GetPriority() const {
        return m_priority;
    }

    /**
     * Sets the thread's current priority.
     * @param priority The new priority.
     */
    void SetPriority(s32 value) {
        m_priority = value;
    }

    /**
     * Gets the thread's nominal priority.
     * @return The current thread's nominal priority.
     */
    s32 GetBasePriority() const {
        return m_base_priority;
    }

    /**
     * Gets the thread's thread ID
     * @return The thread's ID
     */
    u64 GetThreadId() const {
        return m_thread_id;
    }

    void ContinueIfHasKernelWaiters() {
        if (GetNumKernelWaiters() > 0) {
            Continue();
        }
    }

    void SetBasePriority(s32 value);

    Result Run();

    void Exit();

    Result Terminate();

    ThreadState RequestTerminate();

    u32 GetSuspendFlags() const {
        return m_suspend_allowed_flags & m_suspend_request_flags;
    }

    bool IsSuspended() const {
        return GetSuspendFlags() != 0;
    }

    bool IsSuspendRequested(SuspendType type) const {
        return (m_suspend_request_flags &
                (1U << (static_cast<u32>(ThreadState::SuspendShift) + static_cast<u32>(type)))) !=
               0;
    }

    bool IsSuspendRequested() const {
        return m_suspend_request_flags != 0;
    }

    void RequestSuspend(SuspendType type);

    void Resume(SuspendType type);

    void TrySuspend();

    void UpdateState();

    void Continue();

    constexpr void SetSyncedIndex(s32 index) {
        m_synced_index = index;
    }

    constexpr s32 GetSyncedIndex() const {
        return m_synced_index;
    }

    constexpr void SetWaitResult(Result wait_res) {
        m_wait_result = wait_res;
    }

    constexpr Result GetWaitResult() const {
        return m_wait_result;
    }

    /*
     * Returns the Thread Local Storage address of the current thread
     * @returns Address of the thread's TLS
     */
    KProcessAddress GetTlsAddress() const {
        return m_tls_address;
    }

    /*
     * Returns the value of the TPIDR_EL0 Read/Write system register for this thread.
     * @returns The value of the TPIDR_EL0 register.
     */
    u64 GetTpidrEl0() const {
        return m_thread_context.tpidr;
    }

    /// Sets the value of the TPIDR_EL0 Read/Write system register for this thread.
    void SetTpidrEl0(u64 value) {
        m_thread_context.tpidr = value;
    }

    void CloneFpuStatus();

    Svc::ThreadContext& GetContext() {
        return m_thread_context;
    }

    const Svc::ThreadContext& GetContext() const {
        return m_thread_context;
    }

    std::shared_ptr<Common::Fiber>& GetHostContext();

    ThreadState GetState() const {
        return m_thread_state.load(std::memory_order_relaxed) & ThreadState::Mask;
    }

    ThreadState GetRawState() const {
        return m_thread_state.load(std::memory_order_relaxed);
    }

    void SetState(ThreadState state);

    StepState GetStepState() const {
        return m_step_state;
    }

    void SetStepState(StepState state) {
        m_step_state = state;
    }

    s64 GetLastScheduledTick() const {
        return m_last_scheduled_tick;
    }

    void SetLastScheduledTick(s64 tick) {
        m_last_scheduled_tick = tick;
    }

    void AddCpuTime(s32 core_id, s64 amount) {
        m_cpu_time += amount;
        // TODO(bunnei): Debug kernels track per-core tick counts. Should we?
    }

    s64 GetCpuTime() const {
        return m_cpu_time;
    }

    s32 GetActiveCore() const {
        return m_core_id;
    }

    void SetActiveCore(s32 core) {
        m_core_id = core;
    }

    s32 GetCurrentCore() const {
        return m_current_core_id;
    }

    void SetCurrentCore(s32 core) {
        m_current_core_id = core;
    }

    KProcess* GetOwnerProcess() const {
        return m_parent;
    }

    bool IsUserThread() const {
        return m_parent != nullptr;
    }

    std::span<KSynchronizationObject*> GetSynchronizationObjectBuffer() {
        return m_sync_object_buffer.sync_objects;
    }

    std::span<Handle> GetHandleBuffer() {
        return {m_sync_object_buffer.handles.data() + Svc::ArgumentHandleCountMax,
                Svc::ArgumentHandleCountMax};
    }

    u16 GetUserDisableCount() const;
    void SetInterruptFlag();
    void ClearInterruptFlag();

    KThread* GetLockOwner() const;

    const KAffinityMask& GetAffinityMask() const {
        return m_physical_affinity_mask;
    }

    Result GetCoreMask(s32* out_ideal_core, u64* out_affinity_mask);

    Result GetPhysicalCoreMask(s32* out_ideal_core, u64* out_affinity_mask);

    Result SetCoreMask(s32 cpu_core_id, u64 v_affinity_mask);

    Result SetActivity(Svc::ThreadActivity activity);

    Result Sleep(s64 timeout);

    s64 GetYieldScheduleCount() const {
        return m_schedule_count;
    }

    void SetYieldScheduleCount(s64 count) {
        m_schedule_count = count;
    }

    void WaitCancel();

    bool IsWaitCancelled() const {
        return m_wait_cancelled;
    }

    void ClearWaitCancelled() {
        m_wait_cancelled = false;
    }

    bool IsCancellable() const {
        return m_cancellable;
    }

    void SetCancellable() {
        m_cancellable = true;
    }

    void ClearCancellable() {
        m_cancellable = false;
    }

    u32* GetLightSessionData() const {
        return m_light_ipc_data;
    }
    void SetLightSessionData(u32* data) {
        m_light_ipc_data = data;
    }

    bool IsTerminationRequested() const {
        return m_termination_requested || GetRawState() == ThreadState::Terminated;
    }

    u64 GetId() const override {
        return this->GetThreadId();
    }

    bool IsInitialized() const override {
        return m_initialized;
    }

    uintptr_t GetPostDestroyArgument() const override {
        return reinterpret_cast<uintptr_t>(m_parent) | (m_resource_limit_release_hint ? 1 : 0);
    }

    void Finalize() override;

    bool IsSignaled() const override;

    void OnTimer();

    void DoWorkerTaskImpl();

    static void PostDestroy(uintptr_t arg);

    static Result InitializeDummyThread(KThread* thread, KProcess* owner);

    static Result InitializeMainThread(Core::System& system, KThread* thread, s32 virt_core);

    static Result InitializeIdleThread(Core::System& system, KThread* thread, s32 virt_core);

    static Result InitializeHighPriorityThread(Core::System& system, KThread* thread,
                                               KThreadFunction func, uintptr_t arg, s32 virt_core);

    static Result InitializeUserThread(Core::System& system, KThread* thread, KThreadFunction func,
                                       uintptr_t arg, KProcessAddress user_stack_top, s32 prio,
                                       s32 virt_core, KProcess* owner);

    static Result InitializeServiceThread(Core::System& system, KThread* thread,
                                          std::function<void()>&& thread_func, s32 prio,
                                          s32 virt_core, KProcess* owner);

public:
    struct StackParameters {
        u8 svc_permission[0x10];
        std::atomic<u8> dpc_flags;
        u8 current_svc_id;
        bool is_calling_svc;
        bool is_in_exception_handler;
        bool is_pinned;
        s32 disable_count;
        KThread* cur_thread;
    };

    StackParameters& GetStackParameters() {
        return m_stack_parameters;
    }

    const StackParameters& GetStackParameters() const {
        return m_stack_parameters;
    }

    class QueueEntry {
    public:
        constexpr QueueEntry() = default;

        constexpr void Initialize() {
            m_prev = nullptr;
            m_next = nullptr;
        }

        constexpr KThread* GetPrev() const {
            return m_prev;
        }
        constexpr KThread* GetNext() const {
            return m_next;
        }
        constexpr void SetPrev(KThread* thread) {
            m_prev = thread;
        }
        constexpr void SetNext(KThread* thread) {
            m_next = thread;
        }

    private:
        KThread* m_prev{};
        KThread* m_next{};
    };

    QueueEntry& GetPriorityQueueEntry(s32 core) {
        return m_per_core_priority_queue_entry[core];
    }

    const QueueEntry& GetPriorityQueueEntry(s32 core) const {
        return m_per_core_priority_queue_entry[core];
    }

    s32 GetDisableDispatchCount() const {
        return this->GetStackParameters().disable_count;
    }

    void DisableDispatch() {
        ASSERT(GetCurrentThread(m_kernel).GetDisableDispatchCount() >= 0);
        this->GetStackParameters().disable_count++;
    }

    void EnableDispatch() {
        ASSERT(GetCurrentThread(m_kernel).GetDisableDispatchCount() > 0);
        this->GetStackParameters().disable_count--;
    }

    void Pin(s32 current_core);

    void Unpin();

    void SetInExceptionHandler() {
        this->GetStackParameters().is_in_exception_handler = true;
    }

    void ClearInExceptionHandler() {
        this->GetStackParameters().is_in_exception_handler = false;
    }

    bool IsInExceptionHandler() const {
        return this->GetStackParameters().is_in_exception_handler;
    }

    void SetIsCallingSvc() {
        this->GetStackParameters().is_calling_svc = true;
    }

    void ClearIsCallingSvc() {
        this->GetStackParameters().is_calling_svc = false;
    }

    bool IsCallingSvc() const {
        return this->GetStackParameters().is_calling_svc;
    }

    u8 GetSvcId() const {
        return this->GetStackParameters().current_svc_id;
    }

    void RegisterDpc(DpcFlag flag) {
        this->GetStackParameters().dpc_flags |= static_cast<u8>(flag);
    }

    void ClearDpc(DpcFlag flag) {
        this->GetStackParameters().dpc_flags &= ~static_cast<u8>(flag);
    }

    u8 GetDpc() const {
        return this->GetStackParameters().dpc_flags;
    }

    bool HasDpc() const {
        return this->GetDpc() != 0;
    }

    void SetWaitReasonForDebugging(ThreadWaitReasonForDebugging reason) {
        m_wait_reason_for_debugging = reason;
    }

    ThreadWaitReasonForDebugging GetWaitReasonForDebugging() const {
        return m_wait_reason_for_debugging;
    }

    ThreadType GetThreadType() const {
        return m_thread_type;
    }

    bool IsDummyThread() const {
        return this->GetThreadType() == ThreadType::Dummy;
    }

    void AddWaiter(KThread* thread);

    void RemoveWaiter(KThread* thread);

    Result GetThreadContext3(Svc::ThreadContext* out);

    KThread* RemoveUserWaiterByKey(bool* out_has_waiters, KProcessAddress key) {
        return this->RemoveWaiterByKey(out_has_waiters, key, false);
    }

    KThread* RemoveKernelWaiterByKey(bool* out_has_waiters, KProcessAddress key) {
        return this->RemoveWaiterByKey(out_has_waiters, key, true);
    }

    KProcessAddress GetAddressKey() const {
        return m_address_key;
    }

    u32 GetAddressKeyValue() const {
        return m_address_key_value;
    }

    bool GetIsKernelAddressKey() const {
        return m_is_kernel_address_key;
    }

    //! NB: intentional deviation from official kernel.
    //
    // Separate SetAddressKey into user and kernel versions
    // to cope with arbitrary host pointers making their way
    // into things.

    void SetUserAddressKey(KProcessAddress key, u32 val) {
        ASSERT(m_waiting_lock_info == nullptr);
        m_address_key = key;
        m_address_key_value = val;
        m_is_kernel_address_key = false;
    }

    void SetKernelAddressKey(KProcessAddress key) {
        ASSERT(m_waiting_lock_info == nullptr);
        m_address_key = key;
        m_is_kernel_address_key = true;
    }

    void ClearWaitQueue() {
        m_wait_queue = nullptr;
    }

    void BeginWait(KThreadQueue* queue);
    void NotifyAvailable(KSynchronizationObject* signaled_object, Result wait_result);
    void EndWait(Result wait_result);
    void CancelWait(Result wait_result, bool cancel_timer_task);

    s32 GetNumKernelWaiters() const {
        return m_num_kernel_waiters;
    }

    u64 GetConditionVariableKey() const {
        return m_condvar_key;
    }

    u64 GetAddressArbiterKey() const {
        return m_condvar_key;
    }

    // Dummy threads (used for HLE host threads) cannot wait based on the guest scheduler, and
    // therefore will not block on guest kernel synchronization primitives. These methods handle
    // blocking as needed.

    void RequestDummyThreadWait();
    void DummyThreadBeginWait();
    void DummyThreadEndWait();

    uintptr_t GetArgument() const {
        return m_argument;
    }

    KProcessAddress GetUserStackTop() const {
        return m_stack_top;
    }

public:
    // TODO: This shouldn't be defined in kernel namespace
    struct NativeExecutionParameters {
        u64 tpidr_el0{};
        u64 tpidrro_el0{};
        void* native_context{};
        std::atomic<u32> lock{1};
        bool is_running{};
        u32 magic{Common::MakeMagic('S', 'U', 'D', 'A')};
    };

    NativeExecutionParameters& GetNativeExecutionParameters() {
        return m_native_execution_parameters;
    }

private:
    KThread* RemoveWaiterByKey(bool* out_has_waiters, KProcessAddress key,
                               bool is_kernel_address_key);

    static constexpr size_t PriorityInheritanceCountMax = 10;
    union SyncObjectBuffer {
        std::array<KSynchronizationObject*, Svc::ArgumentHandleCountMax> sync_objects{};
        std::array<Handle,
                   Svc::ArgumentHandleCountMax * (sizeof(KSynchronizationObject*) / sizeof(Handle))>
            handles;
        constexpr SyncObjectBuffer() {}
    };
    static_assert(sizeof(SyncObjectBuffer::sync_objects) == sizeof(SyncObjectBuffer::handles));

    struct ConditionVariableComparator {
        struct RedBlackKeyType {
            u64 cv_key{};
            s32 priority{};

            constexpr u64 GetConditionVariableKey() const {
                return cv_key;
            }

            constexpr s32 GetPriority() const {
                return priority;
            }
        };

        template <typename T>
            requires(std::same_as<T, KThread> || std::same_as<T, RedBlackKeyType>)
        static constexpr int Compare(const T& lhs, const KThread& rhs) {
            const u64 l_key = lhs.GetConditionVariableKey();
            const u64 r_key = rhs.GetConditionVariableKey();

            if (l_key < r_key) {
                // Sort first by key
                return -1;
            } else if (l_key == r_key && lhs.GetPriority() < rhs.GetPriority()) {
                // And then by priority.
                return -1;
            } else {
                return 1;
            }
        }
    };

    void AddWaiterImpl(KThread* thread);
    void RemoveWaiterImpl(KThread* thread);
    static void RestorePriority(KernelCore& kernel, KThread* thread);

    void StartTermination();
    void FinishTermination();

    void IncreaseBasePriority(s32 priority);

    Result Initialize(KThreadFunction func, uintptr_t arg, KProcessAddress user_stack_top, s32 prio,
                      s32 virt_core, KProcess* owner, ThreadType type);

    static Result InitializeThread(KThread* thread, KThreadFunction func, uintptr_t arg,
                                   KProcessAddress user_stack_top, s32 prio, s32 core,
                                   KProcess* owner, ThreadType type,
                                   std::function<void()>&& init_func);

    // For core KThread implementation
    Svc::ThreadContext m_thread_context{};
    Common::IntrusiveListNode m_process_list_node;
    Common::IntrusiveRedBlackTreeNode m_condvar_arbiter_tree_node{};
    s32 m_priority{};
    using ConditionVariableThreadTreeTraits =
        Common::IntrusiveRedBlackTreeMemberTraitsDeferredAssert<
            &KThread::m_condvar_arbiter_tree_node>;
    using ConditionVariableThreadTree =
        ConditionVariableThreadTreeTraits::TreeType<ConditionVariableComparator>;

private:
    struct LockWithPriorityInheritanceComparator {
        struct RedBlackKeyType {
            s32 m_priority;

            constexpr s32 GetPriority() const {
                return m_priority;
            }
        };

        template <typename T>
            requires(std::same_as<T, KThread> || std::same_as<T, RedBlackKeyType>)
        static constexpr int Compare(const T& lhs, const KThread& rhs) {
            if (lhs.GetPriority() < rhs.GetPriority()) {
                // Sort by priority.
                return -1;
            } else {
                return 1;
            }
        }
    };
    static_assert(std::same_as<Common::RedBlackKeyType<LockWithPriorityInheritanceComparator, void>,
                               LockWithPriorityInheritanceComparator::RedBlackKeyType>);

    using LockWithPriorityInheritanceThreadTreeTraits =
        Common::IntrusiveRedBlackTreeMemberTraitsDeferredAssert<
            &KThread::m_condvar_arbiter_tree_node>;
    using LockWithPriorityInheritanceThreadTree =
        ConditionVariableThreadTreeTraits::TreeType<LockWithPriorityInheritanceComparator>;

public:
    class LockWithPriorityInheritanceInfo
        : public KSlabAllocated<LockWithPriorityInheritanceInfo>,
          public Common::IntrusiveListBaseNode<LockWithPriorityInheritanceInfo> {
    public:
        explicit LockWithPriorityInheritanceInfo(KernelCore&) {}

        static LockWithPriorityInheritanceInfo* Create(KernelCore& kernel,
                                                       KProcessAddress address_key,
                                                       bool is_kernel_address_key) {
            // Create a new lock info.
            auto* new_lock = LockWithPriorityInheritanceInfo::Allocate(kernel);
            ASSERT(new_lock != nullptr);

            // Set the new lock's address key.
            new_lock->m_address_key = address_key;
            new_lock->m_is_kernel_address_key = is_kernel_address_key;

            return new_lock;
        }

        void SetOwner(KThread* new_owner) {
            // Set new owner.
            m_owner = new_owner;
        }

        void AddWaiter(KThread* waiter) {
            // Insert the waiter.
            m_tree.insert(*waiter);
            m_waiter_count++;

            waiter->SetWaitingLockInfo(this);
        }

        bool RemoveWaiter(KThread* waiter) {
            m_tree.erase(m_tree.iterator_to(*waiter));

            waiter->SetWaitingLockInfo(nullptr);

            return (--m_waiter_count) == 0;
        }

        KThread* GetHighestPriorityWaiter() {
            return std::addressof(m_tree.front());
        }
        const KThread* GetHighestPriorityWaiter() const {
            return std::addressof(m_tree.front());
        }

        LockWithPriorityInheritanceThreadTree& GetThreadTree() {
            return m_tree;
        }
        const LockWithPriorityInheritanceThreadTree& GetThreadTree() const {
            return m_tree;
        }

        KProcessAddress GetAddressKey() const {
            return m_address_key;
        }
        bool GetIsKernelAddressKey() const {
            return m_is_kernel_address_key;
        }
        KThread* GetOwner() const {
            return m_owner;
        }
        u32 GetWaiterCount() const {
            return m_waiter_count;
        }

    private:
        LockWithPriorityInheritanceThreadTree m_tree{};
        KProcessAddress m_address_key{};
        KThread* m_owner{};
        u32 m_waiter_count{};
        bool m_is_kernel_address_key{};
    };

    void SetWaitingLockInfo(LockWithPriorityInheritanceInfo* lock) {
        m_waiting_lock_info = lock;
    }

    LockWithPriorityInheritanceInfo* GetWaitingLockInfo() {
        return m_waiting_lock_info;
    }

    void AddHeldLock(LockWithPriorityInheritanceInfo* lock_info);
    LockWithPriorityInheritanceInfo* FindHeldLock(KProcessAddress address_key,
                                                  bool is_kernel_address_key);

private:
    using LockWithPriorityInheritanceInfoList =
        Common::IntrusiveListBaseTraits<LockWithPriorityInheritanceInfo>::ListType;

    ConditionVariableThreadTree* m_condvar_tree{};
    u64 m_condvar_key{};
    u64 m_virtual_affinity_mask{};
    KAffinityMask m_physical_affinity_mask{};
    u64 m_thread_id{};
    std::atomic<s64> m_cpu_time{};
    KProcessAddress m_address_key{};
    KProcess* m_parent{};
    KVirtualAddress m_kernel_stack_top{};
    u32* m_light_ipc_data{};
    KProcessAddress m_tls_address{};
    KLightLock m_activity_pause_lock;
    SyncObjectBuffer m_sync_object_buffer{};
    s64 m_schedule_count{};
    s64 m_last_scheduled_tick{};
    std::array<QueueEntry, Core::Hardware::NUM_CPU_CORES> m_per_core_priority_queue_entry{};
    KThreadQueue* m_wait_queue{};
    LockWithPriorityInheritanceInfoList m_held_lock_info_list{};
    LockWithPriorityInheritanceInfo* m_waiting_lock_info{};
    WaiterList m_pinned_waiter_list{};
    u32 m_address_key_value{};
    u32 m_suspend_request_flags{};
    u32 m_suspend_allowed_flags{};
    s32 m_synced_index{};
    Result m_wait_result{ResultSuccess};
    s32 m_base_priority{};
    s32 m_physical_ideal_core_id{};
    s32 m_virtual_ideal_core_id{};
    s32 m_num_kernel_waiters{};
    s32 m_current_core_id{};
    s32 m_core_id{};
    KAffinityMask m_original_physical_affinity_mask{};
    s32 m_original_physical_ideal_core_id{};
    s32 m_num_core_migration_disables{};
    std::atomic<ThreadState> m_thread_state{};
    std::atomic<bool> m_termination_requested{};
    bool m_wait_cancelled{};
    bool m_cancellable{};
    bool m_signaled{};
    bool m_initialized{};
    bool m_debug_attached{};
    s8 m_priority_inheritance_count{};
    bool m_resource_limit_release_hint{};
    bool m_is_kernel_address_key{};
    StackParameters m_stack_parameters{};
    Common::SpinLock m_context_guard{};

    // For emulation
    std::shared_ptr<Common::Fiber> m_host_context{};
    ThreadType m_thread_type{};
    StepState m_step_state{};
    bool m_dummy_thread_runnable{true};
    std::mutex m_dummy_thread_mutex{};
    std::condition_variable m_dummy_thread_cv{};

    // For debugging
    std::vector<KSynchronizationObject*> m_wait_objects_for_debugging{};
    KProcessAddress m_mutex_wait_address_for_debugging{};
    ThreadWaitReasonForDebugging m_wait_reason_for_debugging{};
    uintptr_t m_argument{};
    KProcessAddress m_stack_top{};
    NativeExecutionParameters m_native_execution_parameters{};

public:
    using ConditionVariableThreadTreeType = ConditionVariableThreadTree;

    void SetConditionVariable(ConditionVariableThreadTree* tree, KProcessAddress address,
                              u64 cv_key, u32 value) {
        ASSERT(m_waiting_lock_info == nullptr);
        m_condvar_tree = tree;
        m_condvar_key = cv_key;
        m_address_key = address;
        m_address_key_value = value;
        m_is_kernel_address_key = false;
    }

    void ClearConditionVariable() {
        m_condvar_tree = nullptr;
    }

    bool IsWaitingForConditionVariable() const {
        return m_condvar_tree != nullptr;
    }

    void SetAddressArbiter(ConditionVariableThreadTree* tree, u64 address) {
        ASSERT(m_waiting_lock_info == nullptr);
        m_condvar_tree = tree;
        m_condvar_key = address;
    }

    void ClearAddressArbiter() {
        m_condvar_tree = nullptr;
    }

    bool IsWaitingForAddressArbiter() const {
        return m_condvar_tree != nullptr;
    }

    ConditionVariableThreadTree* GetConditionVariableTree() const {
        return m_condvar_tree;
    }
};

class KScopedDisableDispatch {
public:
    explicit KScopedDisableDispatch(KernelCore& kernel) : m_kernel{kernel} {
        // If we are shutting down the kernel, none of this is relevant anymore.
        if (m_kernel.IsShuttingDown()) {
            return;
        }
        GetCurrentThread(kernel).DisableDispatch();
    }

    ~KScopedDisableDispatch();

private:
    KernelCore& m_kernel;
};

inline void KTimerTask::OnTimer() {
    static_cast<KThread*>(this)->OnTimer();
}

} // namespace Kernel
