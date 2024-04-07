// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <map>

#include "core/arm/arm_interface.h"
#include "core/file_sys/program_metadata.h"
#include "core/hle/kernel/code_set.h"
#include "core/hle/kernel/k_address_arbiter.h"
#include "core/hle/kernel/k_capabilities.h"
#include "core/hle/kernel/k_condition_variable.h"
#include "core/hle/kernel/k_handle_table.h"
#include "core/hle/kernel/k_page_table_manager.h"
#include "core/hle/kernel/k_process_page_table.h"
#include "core/hle/kernel/k_system_resource.h"
#include "core/hle/kernel/k_thread.h"
#include "core/hle/kernel/k_thread_local_page.h"
#include "core/memory.h"

namespace Kernel {

enum class DebugWatchpointType : u8 {
    None = 0,
    Read = 1 << 0,
    Write = 1 << 1,
    ReadOrWrite = Read | Write,
};
DECLARE_ENUM_FLAG_OPERATORS(DebugWatchpointType);

struct DebugWatchpoint {
    KProcessAddress start_address;
    KProcessAddress end_address;
    DebugWatchpointType type;
};

class KProcess final : public KAutoObjectWithSlabHeapAndContainer<KProcess, KWorkerTask> {
    KERNEL_AUTOOBJECT_TRAITS(KProcess, KSynchronizationObject);

public:
    enum class State {
        Created = static_cast<u32>(Svc::ProcessState::Created),
        CreatedAttached = static_cast<u32>(Svc::ProcessState::CreatedAttached),
        Running = static_cast<u32>(Svc::ProcessState::Running),
        Crashed = static_cast<u32>(Svc::ProcessState::Crashed),
        RunningAttached = static_cast<u32>(Svc::ProcessState::RunningAttached),
        Terminating = static_cast<u32>(Svc::ProcessState::Terminating),
        Terminated = static_cast<u32>(Svc::ProcessState::Terminated),
        DebugBreak = static_cast<u32>(Svc::ProcessState::DebugBreak),
    };

    using ThreadList = Common::IntrusiveListMemberTraits<&KThread::m_process_list_node>::ListType;

    static constexpr size_t AslrAlignment = 2_MiB;

public:
    static constexpr u64 InitialProcessIdMin = 1;
    static constexpr u64 InitialProcessIdMax = 0x50;

    static constexpr u64 ProcessIdMin = InitialProcessIdMax + 1;
    static constexpr u64 ProcessIdMax = std::numeric_limits<u64>::max();

private:
    using SharedMemoryInfoList = Common::IntrusiveListBaseTraits<KSharedMemoryInfo>::ListType;
    using TLPTree =
        Common::IntrusiveRedBlackTreeBaseTraits<KThreadLocalPage>::TreeType<KThreadLocalPage>;
    using TLPIterator = TLPTree::iterator;

private:
    KProcessPageTable m_page_table;
    std::atomic<size_t> m_used_kernel_memory_size{};
    TLPTree m_fully_used_tlp_tree{};
    TLPTree m_partially_used_tlp_tree{};
    s32 m_ideal_core_id{};
    KResourceLimit* m_resource_limit{};
    KSystemResource* m_system_resource{};
    size_t m_memory_release_hint{};
    State m_state{};
    KLightLock m_state_lock;
    KLightLock m_list_lock;
    KConditionVariable m_cond_var;
    KAddressArbiter m_address_arbiter;
    std::array<u64, 4> m_entropy{};
    bool m_is_signaled{};
    bool m_is_initialized{};
    bool m_is_application{};
    bool m_is_default_application_system_resource{};
    bool m_is_hbl{};
    std::array<char, 13> m_name{};
    std::atomic<u16> m_num_running_threads{};
    Svc::CreateProcessFlag m_flags{};
    KMemoryManager::Pool m_memory_pool{};
    s64 m_schedule_count{};
    KCapabilities m_capabilities{};
    u64 m_program_id{};
    u64 m_process_id{};
    KProcessAddress m_code_address{};
    size_t m_code_size{};
    size_t m_main_thread_stack_size{};
    size_t m_max_process_memory{};
    u32 m_version{};
    KHandleTable m_handle_table;
    KProcessAddress m_plr_address{};
    KThread* m_exception_thread{};
    ThreadList m_thread_list{};
    SharedMemoryInfoList m_shared_memory_list{};
    bool m_is_suspended{};
    bool m_is_immortal{};
    bool m_is_handle_table_initialized{};
    std::array<std::unique_ptr<Core::ArmInterface>, Core::Hardware::NUM_CPU_CORES>
        m_arm_interfaces{};
    std::array<KThread*, Core::Hardware::NUM_CPU_CORES> m_running_threads{};
    std::array<u64, Core::Hardware::NUM_CPU_CORES> m_running_thread_idle_counts{};
    std::array<u64, Core::Hardware::NUM_CPU_CORES> m_running_thread_switch_counts{};
    std::array<KThread*, Core::Hardware::NUM_CPU_CORES> m_pinned_threads{};
    std::array<DebugWatchpoint, Core::Hardware::NUM_WATCHPOINTS> m_watchpoints{};
    std::map<KProcessAddress, u64> m_debug_page_refcounts{};
    std::atomic<s64> m_cpu_time{};
    std::atomic<s64> m_num_process_switches{};
    std::atomic<s64> m_num_thread_switches{};
    std::atomic<s64> m_num_fpu_switches{};
    std::atomic<s64> m_num_supervisor_calls{};
    std::atomic<s64> m_num_ipc_messages{};
    std::atomic<s64> m_num_ipc_replies{};
    std::atomic<s64> m_num_ipc_receives{};
#ifdef HAS_NCE
    std::unordered_map<u64, u64> m_post_handlers{};
#endif
    std::unique_ptr<Core::ExclusiveMonitor> m_exclusive_monitor;
    Core::Memory::Memory m_memory;

private:
    Result StartTermination();
    void FinishTermination();

    void PinThread(s32 core_id, KThread* thread) {
        ASSERT(0 <= core_id && core_id < static_cast<s32>(Core::Hardware::NUM_CPU_CORES));
        ASSERT(thread != nullptr);
        ASSERT(m_pinned_threads[core_id] == nullptr);
        m_pinned_threads[core_id] = thread;
    }

    void UnpinThread(s32 core_id, KThread* thread) {
        ASSERT(0 <= core_id && core_id < static_cast<s32>(Core::Hardware::NUM_CPU_CORES));
        ASSERT(thread != nullptr);
        ASSERT(m_pinned_threads[core_id] == thread);
        m_pinned_threads[core_id] = nullptr;
    }

public:
    explicit KProcess(KernelCore& kernel);
    ~KProcess() override;

    Result Initialize(const Svc::CreateProcessParameter& params, KResourceLimit* res_limit,
                      bool is_real);

    Result Initialize(const Svc::CreateProcessParameter& params, const KPageGroup& pg,
                      std::span<const u32> caps, KResourceLimit* res_limit,
                      KMemoryManager::Pool pool, bool immortal);
    Result Initialize(const Svc::CreateProcessParameter& params, std::span<const u32> user_caps,
                      KResourceLimit* res_limit, KMemoryManager::Pool pool,
                      KProcessAddress aslr_space_start);
    void Exit();

    const char* GetName() const {
        return m_name.data();
    }

    u64 GetProgramId() const {
        return m_program_id;
    }

    u64 GetProcessId() const {
        return m_process_id;
    }

    State GetState() const {
        return m_state;
    }

    u64 GetCoreMask() const {
        return m_capabilities.GetCoreMask();
    }
    u64 GetPhysicalCoreMask() const {
        return m_capabilities.GetPhysicalCoreMask();
    }
    u64 GetPriorityMask() const {
        return m_capabilities.GetPriorityMask();
    }

    s32 GetIdealCoreId() const {
        return m_ideal_core_id;
    }
    void SetIdealCoreId(s32 core_id) {
        m_ideal_core_id = core_id;
    }

    bool CheckThreadPriority(s32 prio) const {
        return ((1ULL << prio) & this->GetPriorityMask()) != 0;
    }

    u32 GetCreateProcessFlags() const {
        return static_cast<u32>(m_flags);
    }

    bool Is64Bit() const {
        return True(m_flags & Svc::CreateProcessFlag::Is64Bit);
    }

    KProcessAddress GetEntryPoint() const {
        return m_code_address;
    }

    size_t GetMainStackSize() const {
        return m_main_thread_stack_size;
    }

    KMemoryManager::Pool GetMemoryPool() const {
        return m_memory_pool;
    }

    u64 GetRandomEntropy(size_t i) const {
        return m_entropy[i];
    }

    bool IsApplication() const {
        return m_is_application;
    }

    bool IsDefaultApplicationSystemResource() const {
        return m_is_default_application_system_resource;
    }

    bool IsSuspended() const {
        return m_is_suspended;
    }
    void SetSuspended(bool suspended) {
        m_is_suspended = suspended;
    }

    Result Terminate();

    bool IsTerminated() const {
        return m_state == State::Terminated;
    }

    bool IsPermittedSvc(u32 svc_id) const {
        return m_capabilities.IsPermittedSvc(svc_id);
    }

    bool IsPermittedInterrupt(s32 interrupt_id) const {
        return m_capabilities.IsPermittedInterrupt(interrupt_id);
    }

    bool IsPermittedDebug() const {
        return m_capabilities.IsPermittedDebug();
    }

    bool CanForceDebug() const {
        return m_capabilities.CanForceDebug();
    }

    bool IsHbl() const {
        return m_is_hbl;
    }

    u32 GetAllocateOption() const {
        return m_page_table.GetAllocateOption();
    }

    ThreadList& GetThreadList() {
        return m_thread_list;
    }
    const ThreadList& GetThreadList() const {
        return m_thread_list;
    }

    bool EnterUserException();
    bool LeaveUserException();
    bool ReleaseUserException(KThread* thread);

    KThread* GetPinnedThread(s32 core_id) const {
        ASSERT(0 <= core_id && core_id < static_cast<s32>(Core::Hardware::NUM_CPU_CORES));
        return m_pinned_threads[core_id];
    }

    const Svc::SvcAccessFlagSet& GetSvcPermissions() const {
        return m_capabilities.GetSvcPermissions();
    }

    KResourceLimit* GetResourceLimit() const {
        return m_resource_limit;
    }

    bool ReserveResource(Svc::LimitableResource which, s64 value);
    bool ReserveResource(Svc::LimitableResource which, s64 value, s64 timeout);
    void ReleaseResource(Svc::LimitableResource which, s64 value);
    void ReleaseResource(Svc::LimitableResource which, s64 value, s64 hint);

    KLightLock& GetStateLock() {
        return m_state_lock;
    }
    KLightLock& GetListLock() {
        return m_list_lock;
    }

    KProcessPageTable& GetPageTable() {
        return m_page_table;
    }
    const KProcessPageTable& GetPageTable() const {
        return m_page_table;
    }

    KHandleTable& GetHandleTable() {
        return m_handle_table;
    }
    const KHandleTable& GetHandleTable() const {
        return m_handle_table;
    }

    size_t GetUsedUserPhysicalMemorySize() const;
    size_t GetTotalUserPhysicalMemorySize() const;
    size_t GetUsedNonSystemUserPhysicalMemorySize() const;
    size_t GetTotalNonSystemUserPhysicalMemorySize() const;

    Result AddSharedMemory(KSharedMemory* shmem, KProcessAddress address, size_t size);
    void RemoveSharedMemory(KSharedMemory* shmem, KProcessAddress address, size_t size);

    Result CreateThreadLocalRegion(KProcessAddress* out);
    Result DeleteThreadLocalRegion(KProcessAddress addr);

    KProcessAddress GetProcessLocalRegionAddress() const {
        return m_plr_address;
    }

    KThread* GetExceptionThread() const {
        return m_exception_thread;
    }

    void AddCpuTime(s64 diff) {
        m_cpu_time += diff;
    }
    s64 GetCpuTime() {
        return m_cpu_time.load();
    }

    s64 GetScheduledCount() const {
        return m_schedule_count;
    }
    void IncrementScheduledCount() {
        ++m_schedule_count;
    }

    void IncrementRunningThreadCount();
    void DecrementRunningThreadCount();

    size_t GetRequiredSecureMemorySizeNonDefault() const {
        if (!this->IsDefaultApplicationSystemResource() && m_system_resource->IsSecureResource()) {
            auto* secure_system_resource = static_cast<KSecureSystemResource*>(m_system_resource);
            return secure_system_resource->CalculateRequiredSecureMemorySize();
        }

        return 0;
    }

    size_t GetRequiredSecureMemorySize() const {
        if (m_system_resource->IsSecureResource()) {
            auto* secure_system_resource = static_cast<KSecureSystemResource*>(m_system_resource);
            return secure_system_resource->CalculateRequiredSecureMemorySize();
        }

        return 0;
    }

    size_t GetTotalSystemResourceSize() const {
        if (!this->IsDefaultApplicationSystemResource() && m_system_resource->IsSecureResource()) {
            auto* secure_system_resource = static_cast<KSecureSystemResource*>(m_system_resource);
            return secure_system_resource->GetSize();
        }

        return 0;
    }

    size_t GetUsedSystemResourceSize() const {
        if (!this->IsDefaultApplicationSystemResource() && m_system_resource->IsSecureResource()) {
            auto* secure_system_resource = static_cast<KSecureSystemResource*>(m_system_resource);
            return secure_system_resource->GetUsedSize();
        }

        return 0;
    }

    void SetRunningThread(s32 core, KThread* thread, u64 idle_count, u64 switch_count) {
        m_running_threads[core] = thread;
        m_running_thread_idle_counts[core] = idle_count;
        m_running_thread_switch_counts[core] = switch_count;
    }

    void ClearRunningThread(KThread* thread) {
        for (size_t i = 0; i < m_running_threads.size(); ++i) {
            if (m_running_threads[i] == thread) {
                m_running_threads[i] = nullptr;
            }
        }
    }

    const KSystemResource& GetSystemResource() const {
        return *m_system_resource;
    }

    const KMemoryBlockSlabManager& GetMemoryBlockSlabManager() const {
        return m_system_resource->GetMemoryBlockSlabManager();
    }
    const KBlockInfoManager& GetBlockInfoManager() const {
        return m_system_resource->GetBlockInfoManager();
    }
    const KPageTableManager& GetPageTableManager() const {
        return m_system_resource->GetPageTableManager();
    }

    KThread* GetRunningThread(s32 core) const {
        return m_running_threads[core];
    }
    u64 GetRunningThreadIdleCount(s32 core) const {
        return m_running_thread_idle_counts[core];
    }
    u64 GetRunningThreadSwitchCount(s32 core) const {
        return m_running_thread_switch_counts[core];
    }

    void RegisterThread(KThread* thread);
    void UnregisterThread(KThread* thread);

    Result Run(s32 priority, size_t stack_size);

    Result Reset();

    void SetDebugBreak() {
        if (m_state == State::RunningAttached) {
            this->ChangeState(State::DebugBreak);
        }
    }

    void SetAttached() {
        if (m_state == State::DebugBreak) {
            this->ChangeState(State::RunningAttached);
        }
    }

    Result SetActivity(Svc::ProcessActivity activity);

    void PinCurrentThread();
    void UnpinCurrentThread();
    void UnpinThread(KThread* thread);

    void SignalConditionVariable(uintptr_t cv_key, int32_t count) {
        return m_cond_var.Signal(cv_key, count);
    }

    Result WaitConditionVariable(KProcessAddress address, uintptr_t cv_key, u32 tag, s64 ns) {
        R_RETURN(m_cond_var.Wait(address, cv_key, tag, ns));
    }

    Result SignalAddressArbiter(uintptr_t address, Svc::SignalType signal_type, s32 value,
                                s32 count) {
        R_RETURN(m_address_arbiter.SignalToAddress(address, signal_type, value, count));
    }

    Result WaitAddressArbiter(uintptr_t address, Svc::ArbitrationType arb_type, s32 value,
                              s64 timeout) {
        R_RETURN(m_address_arbiter.WaitForAddress(address, arb_type, value, timeout));
    }

    Result GetThreadList(s32* out_num_threads, KProcessAddress out_thread_ids, s32 max_out_count);

    static void Switch(KProcess* cur_process, KProcess* next_process);

#ifdef HAS_NCE
    std::unordered_map<u64, u64>& GetPostHandlers() noexcept {
        return m_post_handlers;
    }
#endif

    Core::ArmInterface* GetArmInterface(size_t core_index) const {
        return m_arm_interfaces[core_index].get();
    }

public:
    // Attempts to insert a watchpoint into a free slot. Returns false if none are available.
    bool InsertWatchpoint(KProcessAddress addr, u64 size, DebugWatchpointType type);

    // Attempts to remove the watchpoint specified by the given parameters.
    bool RemoveWatchpoint(KProcessAddress addr, u64 size, DebugWatchpointType type);

    const std::array<DebugWatchpoint, Core::Hardware::NUM_WATCHPOINTS>& GetWatchpoints() const {
        return m_watchpoints;
    }

public:
    Result LoadFromMetadata(const FileSys::ProgramMetadata& metadata, std::size_t code_size,
                            KProcessAddress aslr_space_start, bool is_hbl);

    void LoadModule(CodeSet code_set, KProcessAddress base_addr);

    void InitializeInterfaces();

    Core::Memory::Memory& GetMemory() {
        return m_memory;
    }

    Core::ExclusiveMonitor& GetExclusiveMonitor() const {
        return *m_exclusive_monitor;
    }

public:
    // Overridden parent functions.
    bool IsInitialized() const override {
        return m_is_initialized;
    }

    static void PostDestroy(uintptr_t arg) {}

    void Finalize() override;

    u64 GetIdImpl() const {
        return this->GetProcessId();
    }
    u64 GetId() const override {
        return this->GetIdImpl();
    }

    virtual bool IsSignaled() const override {
        ASSERT(KScheduler::IsSchedulerLockedByCurrentThread(m_kernel));
        return m_is_signaled;
    }

    void DoWorkerTaskImpl();

private:
    void ChangeState(State new_state) {
        if (m_state != new_state) {
            m_state = new_state;
            m_is_signaled = true;
            this->NotifyAvailable();
        }
    }

    Result InitializeHandleTable(s32 size) {
        // Try to initialize the handle table.
        R_TRY(m_handle_table.Initialize(size));

        // We succeeded, so note that we did.
        m_is_handle_table_initialized = true;
        R_SUCCEED();
    }

    void FinalizeHandleTable() {
        // Finalize the table.
        m_handle_table.Finalize();

        // Note that the table is finalized.
        m_is_handle_table_initialized = false;
    }
};

} // namespace Kernel
