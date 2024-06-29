// SPDX-FileCopyrightText: Copyright 2021 sudachi Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <array>
#include <atomic>
#include <bitset>
#include <functional>
#include <memory>
#include <thread>
#include <unordered_set>
#include <utility>

#include "common/assert.h"
#include "common/logging/log.h"
#include "common/microprofile.h"
#include "common/scope_exit.h"
#include "common/thread.h"
#include "common/thread_worker.h"
#include "core/arm/arm_interface.h"
#include "core/arm/exclusive_monitor.h"
#include "core/core.h"
#include "core/core_timing.h"
#include "core/cpu_manager.h"
#include "core/hardware_properties.h"
#include "core/hle/kernel/init/init_slab_setup.h"
#include "core/hle/kernel/k_client_port.h"
#include "core/hle/kernel/k_dynamic_resource_manager.h"
#include "core/hle/kernel/k_handle_table.h"
#include "core/hle/kernel/k_hardware_timer.h"
#include "core/hle/kernel/k_memory_layout.h"
#include "core/hle/kernel/k_memory_manager.h"
#include "core/hle/kernel/k_object_name.h"
#include "core/hle/kernel/k_page_buffer.h"
#include "core/hle/kernel/k_process.h"
#include "core/hle/kernel/k_resource_limit.h"
#include "core/hle/kernel/k_scheduler.h"
#include "core/hle/kernel/k_scoped_resource_reservation.h"
#include "core/hle/kernel/k_shared_memory.h"
#include "core/hle/kernel/k_system_resource.h"
#include "core/hle/kernel/k_thread.h"
#include "core/hle/kernel/k_worker_task_manager.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/physical_core.h"
#include "core/hle/result.h"
#include "core/hle/service/server_manager.h"
#include "core/hle/service/sm/sm.h"
#include "core/memory.h"

#if MICROPROFILE_ENABLED == 1
MICROPROFILE_DEFINE(Kernel_SVC, "Kernel", "SVC", MP_RGB(70, 200, 70));
#endif

namespace Kernel {

struct KernelCore::Impl {
    static constexpr size_t ApplicationMemoryBlockSlabHeapSize = 20000;
    static constexpr size_t SystemMemoryBlockSlabHeapSize = 10000;
    static constexpr size_t BlockInfoSlabHeapSize = 4000;
    static constexpr size_t ReservedDynamicPageCount = 64;

    explicit Impl(Core::System& system_, KernelCore& kernel_) : system{system_} {}

    void SetMulticore(bool is_multi) {
        is_multicore = is_multi;
    }

    void Initialize(KernelCore& kernel) {
        hardware_timer = std::make_unique<Kernel::KHardwareTimer>(kernel);
        hardware_timer->Initialize();

        global_object_list_container = std::make_unique<KAutoObjectWithListContainer>(kernel);
        global_scheduler_context = std::make_unique<Kernel::GlobalSchedulerContext>(kernel);

        is_phantom_mode_for_singlecore = false;

        // Derive the initial memory layout from the emulated board
        Init::InitializeSlabResourceCounts(kernel);
        DeriveInitialMemoryLayout();
        Init::InitializeSlabHeaps(system, *memory_layout);

        // Initialize kernel memory and resources.
        InitializeSystemResourceLimit(kernel, system.CoreTiming());
        InitializeMemoryLayout();
        InitializeShutdownThreads();
        InitializePhysicalCores();
        InitializePreemption(kernel);
        InitializeGlobalData(kernel);

        // Initialize the Dynamic Slab Heaps.
        {
            const auto& pt_heap_region = memory_layout->GetPageTableHeapRegion();
            ASSERT(pt_heap_region.GetEndAddress() != 0);

            InitializeResourceManagers(kernel, pt_heap_region.GetAddress(),
                                       pt_heap_region.GetSize());
        }

        InitializeHackSharedMemory(kernel);
        RegisterHostThread(nullptr);
    }

    void TerminateAllProcesses() {
        std::scoped_lock lk{process_list_lock};
        for (auto& process : process_list) {
            process->Terminate();
            process->Close();
            process = nullptr;
        }
        process_list.clear();
    }

    void Shutdown() {
        is_shutting_down.store(true, std::memory_order_relaxed);
        SCOPE_EXIT {
            is_shutting_down.store(false, std::memory_order_relaxed);
        };

        CloseServices();

        if (application_process) {
            application_process->Close();
            application_process = nullptr;
        }

        next_object_id = 0;
        next_kernel_process_id = KProcess::InitialProcessIdMin;
        next_user_process_id = KProcess::ProcessIdMin;
        next_thread_id = 1;

        preemption_event = nullptr;

        // Cleanup persistent kernel objects
        auto CleanupObject = [](KAutoObject* obj) {
            if (obj) {
                obj->Close();
                obj = nullptr;
            }
        };
        CleanupObject(font_shared_mem);
        CleanupObject(irs_shared_mem);
        CleanupObject(time_shared_mem);
        CleanupObject(hidbus_shared_mem);
        CleanupObject(system_resource_limit);

        for (u32 core_id = 0; core_id < Core::Hardware::NUM_CPU_CORES; core_id++) {
            if (shutdown_threads[core_id]) {
                shutdown_threads[core_id]->Close();
                shutdown_threads[core_id] = nullptr;
            }

            schedulers[core_id].reset();
        }

        // Next host thead ID to use, 0-3 IDs represent core threads, >3 represent others
        next_host_thread_id = Core::Hardware::NUM_CPU_CORES;

        // Close kernel objects that were not freed on shutdown
        {
            std::scoped_lock lk{registered_in_use_objects_lock};
            if (registered_in_use_objects.size()) {
                for (auto& object : registered_in_use_objects) {
                    object->Close();
                }
                registered_in_use_objects.clear();
            }
        }

        // Track kernel objects that were not freed on shutdown
        {
            std::scoped_lock lk{registered_objects_lock};
            if (registered_objects.size()) {
                LOG_DEBUG(Kernel, "{} kernel objects were dangling on shutdown!",
                          registered_objects.size());
                registered_objects.clear();
            }
        }

        object_name_global_data.reset();

        // Ensure that the object list container is finalized and properly shutdown.
        global_object_list_container->Finalize();
        global_object_list_container.reset();

        hardware_timer->Finalize();
        hardware_timer.reset();
    }

    void CloseServices() {
        // Ensures all servers gracefully shutdown.
        std::scoped_lock lk{server_lock};
        server_managers.clear();
    }

    void InitializePhysicalCores() {
        for (u32 i = 0; i < Core::Hardware::NUM_CPU_CORES; i++) {
            const s32 core{static_cast<s32>(i)};

            schedulers[i] = std::make_unique<Kernel::KScheduler>(system.Kernel());
            cores[i] = std::make_unique<Kernel::PhysicalCore>(system.Kernel(), i);

            auto* main_thread{Kernel::KThread::Create(system.Kernel())};
            main_thread->SetCurrentCore(core);
            ASSERT(Kernel::KThread::InitializeMainThread(system, main_thread, core).IsSuccess());
            KThread::Register(system.Kernel(), main_thread);

            auto* idle_thread{Kernel::KThread::Create(system.Kernel())};
            idle_thread->SetCurrentCore(core);
            ASSERT(Kernel::KThread::InitializeIdleThread(system, idle_thread, core).IsSuccess());
            KThread::Register(system.Kernel(), idle_thread);

            schedulers[i]->Initialize(main_thread, idle_thread, core);
        }
    }

    // Creates the default system resource limit
    void InitializeSystemResourceLimit(KernelCore& kernel,
                                       const Core::Timing::CoreTiming& core_timing) {
        system_resource_limit = KResourceLimit::Create(system.Kernel());
        system_resource_limit->Initialize();
        KResourceLimit::Register(kernel, system_resource_limit);

        const auto sizes{memory_layout->GetTotalAndKernelMemorySizes()};
        const auto total_size{sizes.first};
        const auto kernel_size{sizes.second};

        // If setting the default system values fails, then something seriously wrong has occurred.
        ASSERT(
            system_resource_limit->SetLimitValue(LimitableResource::PhysicalMemoryMax, total_size)
                .IsSuccess());
        ASSERT(system_resource_limit->SetLimitValue(LimitableResource::ThreadCountMax, 800)
                   .IsSuccess());
        ASSERT(system_resource_limit->SetLimitValue(LimitableResource::EventCountMax, 900)
                   .IsSuccess());
        ASSERT(system_resource_limit->SetLimitValue(LimitableResource::TransferMemoryCountMax, 200)
                   .IsSuccess());
        ASSERT(system_resource_limit->SetLimitValue(LimitableResource::SessionCountMax, 1133)
                   .IsSuccess());
        system_resource_limit->Reserve(LimitableResource::PhysicalMemoryMax, kernel_size);

        // Reserve secure applet memory, introduced in firmware 5.0.0
        constexpr u64 secure_applet_memory_size{4_MiB};
        ASSERT(system_resource_limit->Reserve(LimitableResource::PhysicalMemoryMax,
                                              secure_applet_memory_size));
    }

    void InitializePreemption(KernelCore& kernel) {
        preemption_event = Core::Timing::CreateEvent(
            "PreemptionCallback",
            [this, &kernel](s64 time,
                            std::chrono::nanoseconds) -> std::optional<std::chrono::nanoseconds> {
                {
                    KScopedSchedulerLock lock(kernel);
                    global_scheduler_context->PreemptThreads();
                }
                return std::nullopt;
            });

        const auto time_interval = std::chrono::nanoseconds{std::chrono::milliseconds(10)};
        system.CoreTiming().ScheduleLoopingEvent(time_interval, time_interval, preemption_event);
    }

    void InitializeResourceManagers(KernelCore& kernel, KVirtualAddress address, size_t size) {
        // Ensure that the buffer is suitable for our use.
        ASSERT(Common::IsAligned(GetInteger(address), PageSize));
        ASSERT(Common::IsAligned(size, PageSize));

        // Ensure that we have space for our reference counts.
        const size_t rc_size =
            Common::AlignUp(KPageTableSlabHeap::CalculateReferenceCountSize(size), PageSize);
        ASSERT(rc_size < size);
        size -= rc_size;

        // Initialize the resource managers' shared page manager.
        resource_manager_page_manager = std::make_unique<KDynamicPageManager>();
        resource_manager_page_manager->Initialize(
            address, size, std::max<size_t>(PageSize, KPageBufferSlabHeap::BufferSize));

        // Initialize the KPageBuffer slab heap.
        page_buffer_slab_heap.Initialize(system);

        // Initialize the fixed-size slabheaps.
        app_memory_block_heap = std::make_unique<KMemoryBlockSlabHeap>();
        sys_memory_block_heap = std::make_unique<KMemoryBlockSlabHeap>();
        block_info_heap = std::make_unique<KBlockInfoSlabHeap>();
        app_memory_block_heap->Initialize(resource_manager_page_manager.get(),
                                          ApplicationMemoryBlockSlabHeapSize);
        sys_memory_block_heap->Initialize(resource_manager_page_manager.get(),
                                          SystemMemoryBlockSlabHeapSize);
        block_info_heap->Initialize(resource_manager_page_manager.get(), BlockInfoSlabHeapSize);

        // Reserve all but a fixed number of remaining pages for the page table heap.
        const size_t num_pt_pages = resource_manager_page_manager->GetCount() -
                                    resource_manager_page_manager->GetUsed() -
                                    ReservedDynamicPageCount;
        page_table_heap = std::make_unique<KPageTableSlabHeap>();

        // TODO(bunnei): Pass in address once we support kernel virtual memory allocations.
        page_table_heap->Initialize(
            resource_manager_page_manager.get(), num_pt_pages,
            /*GetPointer<KPageTableManager::RefCount>(address + size)*/ nullptr);

        // Setup the slab managers.
        KDynamicPageManager* const app_dynamic_page_manager = nullptr;
        KDynamicPageManager* const sys_dynamic_page_manager =
            /*KTargetSystem::IsDynamicResourceLimitsEnabled()*/ true
                ? resource_manager_page_manager.get()
                : nullptr;
        app_memory_block_manager = std::make_unique<KMemoryBlockSlabManager>();
        sys_memory_block_manager = std::make_unique<KMemoryBlockSlabManager>();
        app_block_info_manager = std::make_unique<KBlockInfoManager>();
        sys_block_info_manager = std::make_unique<KBlockInfoManager>();
        app_page_table_manager = std::make_unique<KPageTableManager>();
        sys_page_table_manager = std::make_unique<KPageTableManager>();

        app_memory_block_manager->Initialize(app_dynamic_page_manager, app_memory_block_heap.get());
        sys_memory_block_manager->Initialize(sys_dynamic_page_manager, sys_memory_block_heap.get());

        app_block_info_manager->Initialize(app_dynamic_page_manager, block_info_heap.get());
        sys_block_info_manager->Initialize(sys_dynamic_page_manager, block_info_heap.get());

        app_page_table_manager->Initialize(app_dynamic_page_manager, page_table_heap.get());
        sys_page_table_manager->Initialize(sys_dynamic_page_manager, page_table_heap.get());

        // Check that we have the correct number of dynamic pages available.
        ASSERT(resource_manager_page_manager->GetCount() -
                   resource_manager_page_manager->GetUsed() ==
               ReservedDynamicPageCount);

        // Create the system page table managers.
        app_system_resource = std::make_unique<KSystemResource>(kernel);
        sys_system_resource = std::make_unique<KSystemResource>(kernel);
        KAutoObject::Create(std::addressof(*app_system_resource));
        KAutoObject::Create(std::addressof(*sys_system_resource));

        // Set the managers for the system resources.
        app_system_resource->SetManagers(*app_memory_block_manager, *app_block_info_manager,
                                         *app_page_table_manager);
        sys_system_resource->SetManagers(*sys_memory_block_manager, *sys_block_info_manager,
                                         *sys_page_table_manager);
    }

    void InitializeShutdownThreads() {
        for (u32 core_id = 0; core_id < Core::Hardware::NUM_CPU_CORES; core_id++) {
            shutdown_threads[core_id] = KThread::Create(system.Kernel());
            ASSERT(KThread::InitializeHighPriorityThread(system, shutdown_threads[core_id], {}, {},
                                                         core_id)
                       .IsSuccess());
            KThread::Register(system.Kernel(), shutdown_threads[core_id]);
        }
    }

    void InitializeGlobalData(KernelCore& kernel) {
        object_name_global_data = std::make_unique<KObjectNameGlobalData>(kernel);
    }

    void MakeApplicationProcess(KProcess* process) {
        application_process = process;
        application_process->Open();
    }

    static inline thread_local u8 host_thread_id = UINT8_MAX;

    /// Sets the host thread ID for the caller.
    LTO_NOINLINE u32 SetHostThreadId(std::size_t core_id) {
        // This should only be called during core init.
        ASSERT(host_thread_id == UINT8_MAX);

        // The first four slots are reserved for CPU core threads
        ASSERT(core_id < Core::Hardware::NUM_CPU_CORES);
        host_thread_id = static_cast<u8>(core_id);
        return host_thread_id;
    }

    /// Gets the host thread ID for the caller
    LTO_NOINLINE u32 GetHostThreadId() const {
        return host_thread_id;
    }

    // Gets the dummy KThread for the caller, allocating a new one if this is the first time
    LTO_NOINLINE KThread* GetHostDummyThread(KThread* existing_thread) {
        const auto initialize{[](KThread* thread) LTO_NOINLINE {
            ASSERT(KThread::InitializeDummyThread(thread, nullptr).IsSuccess());
            return thread;
        }};

        thread_local KThread raw_thread{system.Kernel()};
        thread_local KThread* thread = existing_thread ? existing_thread : initialize(&raw_thread);
        return thread;
    }

    /// Registers a CPU core thread by allocating a host thread ID for it
    void RegisterCoreThread(std::size_t core_id) {
        ASSERT(core_id < Core::Hardware::NUM_CPU_CORES);
        const auto this_id = SetHostThreadId(core_id);
        if (!is_multicore) {
            single_core_thread_id = this_id;
        }
    }

    /// Registers a new host thread by allocating a host thread ID for it
    void RegisterHostThread(KThread* existing_thread) {
        [[maybe_unused]] const auto dummy_thread = GetHostDummyThread(existing_thread);
    }

    [[nodiscard]] u32 GetCurrentHostThreadID() {
        const auto this_id = GetHostThreadId();
        if (!is_multicore && single_core_thread_id == this_id) {
            return static_cast<u32>(system.GetCpuManager().CurrentCore());
        }
        return this_id;
    }

    static inline thread_local bool is_phantom_mode_for_singlecore{false};

    LTO_NOINLINE bool IsPhantomModeForSingleCore() const {
        return is_phantom_mode_for_singlecore;
    }

    LTO_NOINLINE void SetIsPhantomModeForSingleCore(bool value) {
        ASSERT(!is_multicore);
        is_phantom_mode_for_singlecore = value;
    }

    bool IsShuttingDown() const {
        return is_shutting_down.load(std::memory_order_relaxed);
    }

    static inline thread_local KThread* current_thread{nullptr};

    LTO_NOINLINE KThread* GetCurrentEmuThread() {
        if (!current_thread) {
            current_thread = GetHostDummyThread(nullptr);
        }
        return current_thread;
    }

    LTO_NOINLINE void SetCurrentEmuThread(KThread* thread) {
        current_thread = thread;
    }

    void DeriveInitialMemoryLayout() {
        memory_layout = std::make_unique<KMemoryLayout>();

        // Insert the root region for the virtual memory tree, from which all other regions will
        // derive.
        memory_layout->GetVirtualMemoryRegionTree().InsertDirectly(
            KernelVirtualAddressSpaceBase,
            KernelVirtualAddressSpaceBase + KernelVirtualAddressSpaceSize - 1);

        // Insert the root region for the physical memory tree, from which all other regions will
        // derive.
        memory_layout->GetPhysicalMemoryRegionTree().InsertDirectly(
            KernelPhysicalAddressSpaceBase,
            KernelPhysicalAddressSpaceBase + KernelPhysicalAddressSpaceSize - 1);

        // Save start and end for ease of use.
        constexpr KVirtualAddress code_start_virt_addr = KernelVirtualAddressCodeBase;
        constexpr KVirtualAddress code_end_virt_addr = KernelVirtualAddressCodeEnd;

        // Setup the containing kernel region.
        constexpr size_t KernelRegionSize = 1_GiB;
        constexpr size_t KernelRegionAlign = 1_GiB;
        constexpr KVirtualAddress kernel_region_start =
            Common::AlignDown(GetInteger(code_start_virt_addr), KernelRegionAlign);
        size_t kernel_region_size = KernelRegionSize;
        if (!(kernel_region_start + KernelRegionSize - 1 <= KernelVirtualAddressSpaceLast)) {
            kernel_region_size = KernelVirtualAddressSpaceEnd - GetInteger(kernel_region_start);
        }
        ASSERT(memory_layout->GetVirtualMemoryRegionTree().Insert(
            GetInteger(kernel_region_start), kernel_region_size, KMemoryRegionType_Kernel));

        // Setup the code region.
        constexpr size_t CodeRegionAlign = PageSize;
        constexpr KVirtualAddress code_region_start =
            Common::AlignDown(GetInteger(code_start_virt_addr), CodeRegionAlign);
        constexpr KVirtualAddress code_region_end =
            Common::AlignUp(GetInteger(code_end_virt_addr), CodeRegionAlign);
        constexpr size_t code_region_size = code_region_end - code_region_start;
        ASSERT(memory_layout->GetVirtualMemoryRegionTree().Insert(
            GetInteger(code_region_start), code_region_size, KMemoryRegionType_KernelCode));

        // Setup board-specific device physical regions.
        Init::SetupDevicePhysicalMemoryRegions(*memory_layout);

        // Determine the amount of space needed for the misc region.
        size_t misc_region_needed_size;
        {
            // Each core has a one page stack for all three stack types (Main, Idle, Exception).
            misc_region_needed_size = Core::Hardware::NUM_CPU_CORES * (3 * (PageSize + PageSize));

            // Account for each auto-map device.
            for (const auto& region : memory_layout->GetPhysicalMemoryRegionTree()) {
                if (region.HasTypeAttribute(KMemoryRegionAttr_ShouldKernelMap)) {
                    // Check that the region is valid.
                    ASSERT(region.GetEndAddress() != 0);

                    // Account for the region.
                    misc_region_needed_size +=
                        PageSize + (Common::AlignUp(region.GetLastAddress(), PageSize) -
                                    Common::AlignDown(region.GetAddress(), PageSize));
                }
            }

            // Multiply the needed size by three, to account for the need for guard space.
            misc_region_needed_size *= 3;
        }

        // Decide on the actual size for the misc region.
        constexpr size_t MiscRegionAlign = KernelAslrAlignment;
        constexpr size_t MiscRegionMinimumSize = 32_MiB;
        const size_t misc_region_size = Common::AlignUp(
            std::max(misc_region_needed_size, MiscRegionMinimumSize), MiscRegionAlign);
        ASSERT(misc_region_size > 0);

        // Setup the misc region.
        const KVirtualAddress misc_region_start =
            memory_layout->GetVirtualMemoryRegionTree().GetRandomAlignedRegion(
                misc_region_size, MiscRegionAlign, KMemoryRegionType_Kernel);
        ASSERT(memory_layout->GetVirtualMemoryRegionTree().Insert(
            GetInteger(misc_region_start), misc_region_size, KMemoryRegionType_KernelMisc));

        // Determine if we'll use extra thread resources.
        const bool use_extra_resources = KSystemControl::Init::ShouldIncreaseThreadResourceLimit();

        // Setup the stack region.
        constexpr size_t StackRegionSize = 14_MiB;
        constexpr size_t StackRegionAlign = KernelAslrAlignment;
        const KVirtualAddress stack_region_start =
            memory_layout->GetVirtualMemoryRegionTree().GetRandomAlignedRegion(
                StackRegionSize, StackRegionAlign, KMemoryRegionType_Kernel);
        ASSERT(memory_layout->GetVirtualMemoryRegionTree().Insert(
            GetInteger(stack_region_start), StackRegionSize, KMemoryRegionType_KernelStack));

        // Determine the size of the resource region.
        const size_t resource_region_size =
            memory_layout->GetResourceRegionSizeForInit(use_extra_resources);

        // Determine the size of the slab region.
        const size_t slab_region_size =
            Common::AlignUp(Init::CalculateTotalSlabHeapSize(system.Kernel()), PageSize);
        ASSERT(slab_region_size <= resource_region_size);

        // Setup the slab region.
        const KPhysicalAddress code_start_phys_addr = KernelPhysicalAddressCodeBase;
        const KPhysicalAddress code_end_phys_addr = code_start_phys_addr + code_region_size;
        const KPhysicalAddress slab_start_phys_addr = code_end_phys_addr;
        const KPhysicalAddress slab_end_phys_addr = slab_start_phys_addr + slab_region_size;
        constexpr size_t SlabRegionAlign = KernelAslrAlignment;
        const size_t slab_region_needed_size =
            Common::AlignUp(GetInteger(code_end_phys_addr) + slab_region_size, SlabRegionAlign) -
            Common::AlignDown(GetInteger(code_end_phys_addr), SlabRegionAlign);
        const KVirtualAddress slab_region_start =
            memory_layout->GetVirtualMemoryRegionTree().GetRandomAlignedRegion(
                slab_region_needed_size, SlabRegionAlign, KMemoryRegionType_Kernel) +
            (GetInteger(code_end_phys_addr) % SlabRegionAlign);
        ASSERT(memory_layout->GetVirtualMemoryRegionTree().Insert(
            GetInteger(slab_region_start), slab_region_size, KMemoryRegionType_KernelSlab));

        // Setup the temp region.
        constexpr size_t TempRegionSize = 128_MiB;
        constexpr size_t TempRegionAlign = KernelAslrAlignment;
        const KVirtualAddress temp_region_start =
            memory_layout->GetVirtualMemoryRegionTree().GetRandomAlignedRegion(
                TempRegionSize, TempRegionAlign, KMemoryRegionType_Kernel);
        ASSERT(memory_layout->GetVirtualMemoryRegionTree().Insert(
            GetInteger(temp_region_start), TempRegionSize, KMemoryRegionType_KernelTemp));

        // Automatically map in devices that have auto-map attributes.
        for (auto& region : memory_layout->GetPhysicalMemoryRegionTree()) {
            // We only care about kernel regions.
            if (!region.IsDerivedFrom(KMemoryRegionType_Kernel)) {
                continue;
            }

            // Check whether we should map the region.
            if (!region.HasTypeAttribute(KMemoryRegionAttr_ShouldKernelMap)) {
                continue;
            }

            // If this region has already been mapped, no need to consider it.
            if (region.HasTypeAttribute(KMemoryRegionAttr_DidKernelMap)) {
                continue;
            }

            // Check that the region is valid.
            ASSERT(region.GetEndAddress() != 0);

            // Set the attribute to note we've mapped this region.
            region.SetTypeAttribute(KMemoryRegionAttr_DidKernelMap);

            // Create a virtual pair region and insert it into the tree.
            const KPhysicalAddress map_phys_addr = Common::AlignDown(region.GetAddress(), PageSize);
            const size_t map_size =
                Common::AlignUp(region.GetEndAddress(), PageSize) - GetInteger(map_phys_addr);
            const KVirtualAddress map_virt_addr =
                memory_layout->GetVirtualMemoryRegionTree().GetRandomAlignedRegionWithGuard(
                    map_size, PageSize, KMemoryRegionType_KernelMisc, PageSize);
            ASSERT(memory_layout->GetVirtualMemoryRegionTree().Insert(
                GetInteger(map_virt_addr), map_size, KMemoryRegionType_KernelMiscMappedDevice));
            region.SetPairAddress(GetInteger(map_virt_addr) + region.GetAddress() -
                                  GetInteger(map_phys_addr));
        }

        Init::SetupDramPhysicalMemoryRegions(*memory_layout);

        // Insert a physical region for the kernel code region.
        ASSERT(memory_layout->GetPhysicalMemoryRegionTree().Insert(
            GetInteger(code_start_phys_addr), code_region_size, KMemoryRegionType_DramKernelCode));

        // Insert a physical region for the kernel slab region.
        ASSERT(memory_layout->GetPhysicalMemoryRegionTree().Insert(
            GetInteger(slab_start_phys_addr), slab_region_size, KMemoryRegionType_DramKernelSlab));

        // Insert a physical region for the secure applet memory.
        const auto secure_applet_end_phys_addr =
            slab_end_phys_addr + KSystemControl::SecureAppletMemorySize;
        if constexpr (KSystemControl::SecureAppletMemorySize > 0) {
            ASSERT(memory_layout->GetPhysicalMemoryRegionTree().Insert(
                GetInteger(slab_end_phys_addr), KSystemControl::SecureAppletMemorySize,
                KMemoryRegionType_DramKernelSecureAppletMemory));
        }

        // Insert a physical region for the unknown debug2 region.
        constexpr size_t SecureUnknownRegionSize = 0;
        const size_t secure_unknown_size = SecureUnknownRegionSize;
        const auto secure_unknown_end_phys_addr = secure_applet_end_phys_addr + secure_unknown_size;
        if constexpr (SecureUnknownRegionSize > 0) {
            ASSERT(memory_layout->GetPhysicalMemoryRegionTree().Insert(
                GetInteger(secure_applet_end_phys_addr), secure_unknown_size,
                KMemoryRegionType_DramKernelSecureUnknown));
        }

        // Determine size available for kernel page table heaps, requiring > 8 MB.
        const KPhysicalAddress resource_end_phys_addr = slab_start_phys_addr + resource_region_size;
        const size_t page_table_heap_size = resource_end_phys_addr - secure_unknown_end_phys_addr;
        ASSERT(page_table_heap_size / 4_MiB > 2);

        // Insert a physical region for the kernel page table heap region
        ASSERT(memory_layout->GetPhysicalMemoryRegionTree().Insert(
            GetInteger(secure_unknown_end_phys_addr), page_table_heap_size,
            KMemoryRegionType_DramKernelPtHeap));

        // All DRAM regions that we haven't tagged by this point will be mapped under the linear
        // mapping. Tag them.
        for (auto& region : memory_layout->GetPhysicalMemoryRegionTree()) {
            if (region.GetType() == KMemoryRegionType_Dram) {
                // Check that the region is valid.
                ASSERT(region.GetEndAddress() != 0);

                // Set the linear map attribute.
                region.SetTypeAttribute(KMemoryRegionAttr_LinearMapped);
            }
        }

        // Get the linear region extents.
        const auto linear_extents =
            memory_layout->GetPhysicalMemoryRegionTree().GetDerivedRegionExtents(
                KMemoryRegionAttr_LinearMapped);
        ASSERT(linear_extents.GetEndAddress() != 0);

        // Setup the linear mapping region.
        constexpr size_t LinearRegionAlign = 1_GiB;
        const KPhysicalAddress aligned_linear_phys_start =
            Common::AlignDown(linear_extents.GetAddress(), LinearRegionAlign);
        const size_t linear_region_size =
            Common::AlignUp(linear_extents.GetEndAddress(), LinearRegionAlign) -
            GetInteger(aligned_linear_phys_start);
        const KVirtualAddress linear_region_start =
            memory_layout->GetVirtualMemoryRegionTree().GetRandomAlignedRegionWithGuard(
                linear_region_size, LinearRegionAlign, KMemoryRegionType_None, LinearRegionAlign);

        const u64 linear_region_phys_to_virt_diff =
            GetInteger(linear_region_start) - GetInteger(aligned_linear_phys_start);

        // Map and create regions for all the linearly-mapped data.
        {
            KPhysicalAddress cur_phys_addr = 0;
            u64 cur_size = 0;
            for (auto& region : memory_layout->GetPhysicalMemoryRegionTree()) {
                if (!region.HasTypeAttribute(KMemoryRegionAttr_LinearMapped)) {
                    continue;
                }

                ASSERT(region.GetEndAddress() != 0);

                if (cur_size == 0) {
                    cur_phys_addr = region.GetAddress();
                    cur_size = region.GetSize();
                } else if (cur_phys_addr + cur_size == region.GetAddress()) {
                    cur_size += region.GetSize();
                } else {
                    cur_phys_addr = region.GetAddress();
                    cur_size = region.GetSize();
                }

                const KVirtualAddress region_virt_addr =
                    region.GetAddress() + linear_region_phys_to_virt_diff;
                ASSERT(memory_layout->GetVirtualMemoryRegionTree().Insert(
                    GetInteger(region_virt_addr), region.GetSize(),
                    GetTypeForVirtualLinearMapping(region.GetType())));
                region.SetPairAddress(GetInteger(region_virt_addr));

                KMemoryRegion* virt_region =
                    memory_layout->GetVirtualMemoryRegionTree().FindModifiable(
                        GetInteger(region_virt_addr));
                ASSERT(virt_region != nullptr);
                virt_region->SetPairAddress(region.GetAddress());
            }
        }

        // Insert regions for the initial page table region.
        ASSERT(memory_layout->GetPhysicalMemoryRegionTree().Insert(
            GetInteger(resource_end_phys_addr), KernelPageTableHeapSize,
            KMemoryRegionType_DramKernelInitPt));
        ASSERT(memory_layout->GetVirtualMemoryRegionTree().Insert(
            GetInteger(resource_end_phys_addr) + linear_region_phys_to_virt_diff,
            KernelPageTableHeapSize, KMemoryRegionType_VirtualDramKernelInitPt));

        // All linear-mapped DRAM regions that we haven't tagged by this point will be allocated to
        // some pool partition. Tag them.
        for (auto& region : memory_layout->GetPhysicalMemoryRegionTree()) {
            if (region.GetType() == (KMemoryRegionType_Dram | KMemoryRegionAttr_LinearMapped)) {
                region.SetType(KMemoryRegionType_DramPoolPartition);
            }
        }

        // Setup all other memory regions needed to arrange the pool partitions.
        Init::SetupPoolPartitionMemoryRegions(*memory_layout);

        // Cache all linear regions in their own trees for faster access, later.
        memory_layout->InitializeLinearMemoryRegionTrees(aligned_linear_phys_start,
                                                         linear_region_start);
    }

    void InitializeMemoryLayout() {
        // Initialize the memory manager.
        memory_manager = std::make_unique<KMemoryManager>(system);
        const auto& management_region = memory_layout->GetPoolManagementRegion();
        ASSERT(management_region.GetEndAddress() != 0);
        memory_manager->Initialize(management_region.GetAddress(), management_region.GetSize());
    }

    void InitializeHackSharedMemory(KernelCore& kernel) {
        // Setup memory regions for emulated processes
        // TODO(bunnei): These should not be hardcoded regions initialized within the kernel
        constexpr std::size_t font_size{0x1100000};
        constexpr std::size_t irs_size{0x8000};
        constexpr std::size_t time_size{0x1000};
        constexpr std::size_t hidbus_size{0x1000};

        font_shared_mem = KSharedMemory::Create(system.Kernel());
        irs_shared_mem = KSharedMemory::Create(system.Kernel());
        time_shared_mem = KSharedMemory::Create(system.Kernel());
        hidbus_shared_mem = KSharedMemory::Create(system.Kernel());

        font_shared_mem->Initialize(system.DeviceMemory(), nullptr, Svc::MemoryPermission::None,
                                    Svc::MemoryPermission::Read, font_size);
        KSharedMemory::Register(kernel, font_shared_mem);

        irs_shared_mem->Initialize(system.DeviceMemory(), nullptr, Svc::MemoryPermission::None,
                                   Svc::MemoryPermission::Read, irs_size);
        KSharedMemory::Register(kernel, irs_shared_mem);

        time_shared_mem->Initialize(system.DeviceMemory(), nullptr, Svc::MemoryPermission::None,
                                    Svc::MemoryPermission::Read, time_size);
        KSharedMemory::Register(kernel, time_shared_mem);

        hidbus_shared_mem->Initialize(system.DeviceMemory(), nullptr, Svc::MemoryPermission::None,
                                      Svc::MemoryPermission::Read, hidbus_size);
        KSharedMemory::Register(kernel, hidbus_shared_mem);
    }

    std::mutex registered_objects_lock;
    std::mutex registered_in_use_objects_lock;

    std::atomic<u32> next_object_id{0};
    std::atomic<u64> next_kernel_process_id{KProcess::InitialProcessIdMin};
    std::atomic<u64> next_user_process_id{KProcess::ProcessIdMin};
    std::atomic<u64> next_thread_id{1};

    // Lists all processes that exist in the current session.
    std::mutex process_list_lock;
    std::vector<KProcess*> process_list;
    KProcess* application_process{};
    std::unique_ptr<Kernel::GlobalSchedulerContext> global_scheduler_context;
    std::unique_ptr<Kernel::KHardwareTimer> hardware_timer;

    Init::KSlabResourceCounts slab_resource_counts{};
    KResourceLimit* system_resource_limit{};

    KPageBufferSlabHeap page_buffer_slab_heap;

    std::shared_ptr<Core::Timing::EventType> preemption_event;

    std::unique_ptr<KAutoObjectWithListContainer> global_object_list_container;

    std::unique_ptr<KObjectNameGlobalData> object_name_global_data;

    std::unordered_set<KAutoObject*> registered_objects;
    std::unordered_set<KAutoObject*> registered_in_use_objects;

    std::mutex server_lock;
    std::vector<std::unique_ptr<Service::ServerManager>> server_managers;

    std::array<std::unique_ptr<Kernel::PhysicalCore>, Core::Hardware::NUM_CPU_CORES> cores;

    // Next host thead ID to use, 0-3 IDs represent core threads, >3 represent others
    std::atomic<u32> next_host_thread_id{Core::Hardware::NUM_CPU_CORES};

    // Kernel memory management
    std::unique_ptr<KMemoryManager> memory_manager;

    // Resource managers
    std::unique_ptr<KDynamicPageManager> resource_manager_page_manager;
    std::unique_ptr<KPageTableSlabHeap> page_table_heap;
    std::unique_ptr<KMemoryBlockSlabHeap> app_memory_block_heap;
    std::unique_ptr<KMemoryBlockSlabHeap> sys_memory_block_heap;
    std::unique_ptr<KBlockInfoSlabHeap> block_info_heap;
    std::unique_ptr<KPageTableManager> app_page_table_manager;
    std::unique_ptr<KPageTableManager> sys_page_table_manager;
    std::unique_ptr<KMemoryBlockSlabManager> app_memory_block_manager;
    std::unique_ptr<KMemoryBlockSlabManager> sys_memory_block_manager;
    std::unique_ptr<KBlockInfoManager> app_block_info_manager;
    std::unique_ptr<KBlockInfoManager> sys_block_info_manager;
    std::unique_ptr<KSystemResource> app_system_resource;
    std::unique_ptr<KSystemResource> sys_system_resource;

    // Shared memory for services
    Kernel::KSharedMemory* hid_shared_mem{};
    Kernel::KSharedMemory* font_shared_mem{};
    Kernel::KSharedMemory* irs_shared_mem{};
    Kernel::KSharedMemory* time_shared_mem{};
    Kernel::KSharedMemory* hidbus_shared_mem{};

    // Memory layout
    std::unique_ptr<KMemoryLayout> memory_layout;

    std::array<KThread*, Core::Hardware::NUM_CPU_CORES> shutdown_threads{};
    std::array<std::unique_ptr<Kernel::KScheduler>, Core::Hardware::NUM_CPU_CORES> schedulers{};

    bool is_multicore{};
    std::atomic_bool is_shutting_down{};
    u32 single_core_thread_id{};

    std::array<u64, Core::Hardware::NUM_CPU_CORES> svc_ticks{};

    KWorkerTaskManager worker_task_manager;

    // System context
    Core::System& system;
};

KernelCore::KernelCore(Core::System& system) : impl{std::make_unique<Impl>(system, *this)} {}
KernelCore::~KernelCore() = default;

void KernelCore::SetMulticore(bool is_multicore) {
    impl->SetMulticore(is_multicore);
}

void KernelCore::Initialize() {
    slab_heap_container = std::make_unique<SlabHeapContainer>();
    impl->Initialize(*this);
}

void KernelCore::Shutdown() {
    impl->Shutdown();
}

void KernelCore::CloseServices() {
    impl->CloseServices();
}

const KResourceLimit* KernelCore::GetSystemResourceLimit() const {
    return impl->system_resource_limit;
}

KResourceLimit* KernelCore::GetSystemResourceLimit() {
    return impl->system_resource_limit;
}

void KernelCore::AppendNewProcess(KProcess* process) {
    process->Open();

    std::scoped_lock lk{impl->process_list_lock};
    impl->process_list.push_back(process);
}

void KernelCore::RemoveProcess(KProcess* process) {
    std::scoped_lock lk{impl->process_list_lock};
    if (std::erase(impl->process_list, process)) {
        process->Close();
    }
}

void KernelCore::MakeApplicationProcess(KProcess* process) {
    impl->MakeApplicationProcess(process);
}

KProcess* KernelCore::ApplicationProcess() {
    return impl->application_process;
}

const KProcess* KernelCore::ApplicationProcess() const {
    return impl->application_process;
}

std::list<KScopedAutoObject<KProcess>> KernelCore::GetProcessList() {
    std::list<KScopedAutoObject<KProcess>> processes;
    std::scoped_lock lk{impl->process_list_lock};

    for (auto* const process : impl->process_list) {
        processes.emplace_back(process);
    }

    return processes;
}

Kernel::GlobalSchedulerContext& KernelCore::GlobalSchedulerContext() {
    return *impl->global_scheduler_context;
}

const Kernel::GlobalSchedulerContext& KernelCore::GlobalSchedulerContext() const {
    return *impl->global_scheduler_context;
}

Kernel::KScheduler& KernelCore::Scheduler(std::size_t id) {
    return *impl->schedulers[id];
}

const Kernel::KScheduler& KernelCore::Scheduler(std::size_t id) const {
    return *impl->schedulers[id];
}

Kernel::PhysicalCore& KernelCore::PhysicalCore(std::size_t id) {
    return *impl->cores[id];
}

const Kernel::PhysicalCore& KernelCore::PhysicalCore(std::size_t id) const {
    return *impl->cores[id];
}

size_t KernelCore::CurrentPhysicalCoreIndex() const {
    const u32 core_id = impl->GetCurrentHostThreadID();
    if (core_id >= Core::Hardware::NUM_CPU_CORES) {
        return Core::Hardware::NUM_CPU_CORES - 1;
    }
    return core_id;
}

Kernel::PhysicalCore& KernelCore::CurrentPhysicalCore() {
    return *impl->cores[CurrentPhysicalCoreIndex()];
}

const Kernel::PhysicalCore& KernelCore::CurrentPhysicalCore() const {
    return *impl->cores[CurrentPhysicalCoreIndex()];
}

Kernel::KScheduler* KernelCore::CurrentScheduler() {
    const u32 core_id = impl->GetCurrentHostThreadID();
    if (core_id >= Core::Hardware::NUM_CPU_CORES) {
        // This is expected when called from not a guest thread
        return {};
    }
    return impl->schedulers[core_id].get();
}

Kernel::KHardwareTimer& KernelCore::HardwareTimer() {
    return *impl->hardware_timer;
}

KAutoObjectWithListContainer& KernelCore::ObjectListContainer() {
    return *impl->global_object_list_container;
}

const KAutoObjectWithListContainer& KernelCore::ObjectListContainer() const {
    return *impl->global_object_list_container;
}

void KernelCore::PrepareReschedule(std::size_t id) {
    // TODO: Reimplement, this
}

void KernelCore::RegisterKernelObject(KAutoObject* object) {
    std::scoped_lock lk{impl->registered_objects_lock};
    impl->registered_objects.insert(object);
}

void KernelCore::UnregisterKernelObject(KAutoObject* object) {
    std::scoped_lock lk{impl->registered_objects_lock};
    impl->registered_objects.erase(object);
}

void KernelCore::RegisterInUseObject(KAutoObject* object) {
    std::scoped_lock lk{impl->registered_in_use_objects_lock};
    impl->registered_in_use_objects.insert(object);
}

void KernelCore::UnregisterInUseObject(KAutoObject* object) {
    std::scoped_lock lk{impl->registered_in_use_objects_lock};
    impl->registered_in_use_objects.erase(object);
}

void KernelCore::RunServer(std::unique_ptr<Service::ServerManager>&& server_manager) {
    auto* manager = server_manager.get();

    {
        std::scoped_lock lk{impl->server_lock};
        if (impl->is_shutting_down) {
            return;
        }

        impl->server_managers.emplace_back(std::move(server_manager));
    }

    manager->LoopProcess();
}

u32 KernelCore::CreateNewObjectID() {
    return impl->next_object_id++;
}

u64 KernelCore::CreateNewThreadID() {
    return impl->next_thread_id++;
}

u64 KernelCore::CreateNewKernelProcessID() {
    return impl->next_kernel_process_id++;
}

u64 KernelCore::CreateNewUserProcessID() {
    return impl->next_user_process_id++;
}

void KernelCore::RegisterCoreThread(std::size_t core_id) {
    impl->RegisterCoreThread(core_id);
}

void KernelCore::RegisterHostThread(KThread* existing_thread) {
    impl->RegisterHostThread(existing_thread);

    if (existing_thread != nullptr) {
        ASSERT(GetCurrentEmuThread() == existing_thread);
    }
}

static std::jthread RunHostThreadFunc(KernelCore& kernel, KProcess* process,
                                      std::string&& thread_name, std::function<void()>&& func) {
    // Reserve a new thread from the process resource limit.
    KScopedResourceReservation thread_reservation(process, LimitableResource::ThreadCountMax);
    ASSERT(thread_reservation.Succeeded());

    // Initialize the thread.
    KThread* thread = KThread::Create(kernel);
    ASSERT(R_SUCCEEDED(KThread::InitializeDummyThread(thread, process)));

    // Commit the thread reservation.
    thread_reservation.Commit();

    // Register the thread.
    KThread::Register(kernel, thread);

    return std::jthread(
        [&kernel, thread, thread_name_{std::move(thread_name)}, func_{std::move(func)}] {
            // Set the thread name.
            Common::SetCurrentThreadName(thread_name_.c_str());

            // Set the thread as current.
            kernel.RegisterHostThread(thread);

            // Run the callback.
            func_();

            // Close the thread.
            // This will free the process if it is the last reference.
            thread->Close();
        });
}

std::jthread KernelCore::RunOnHostCoreProcess(std::string&& process_name,
                                              std::function<void()> func) {
    // Make a new process.
    KProcess* process = KProcess::Create(*this);
    ASSERT(R_SUCCEEDED(
        process->Initialize(Svc::CreateProcessParameter{}, GetSystemResourceLimit(), false)));

    // Ensure that we don't hold onto any extra references.
    SCOPE_EXIT {
        process->Close();
    };

    // Register the new process.
    KProcess::Register(*this, process);

    // Run the host thread.
    return RunHostThreadFunc(*this, process, std::move(process_name), std::move(func));
}

std::jthread KernelCore::RunOnHostCoreThread(std::string&& thread_name,
                                             std::function<void()> func) {
    // Get the current process.
    KProcess* process = GetCurrentProcessPointer(*this);

    // Run the host thread.
    return RunHostThreadFunc(*this, process, std::move(thread_name), std::move(func));
}

void KernelCore::RunOnGuestCoreProcess(std::string&& process_name, std::function<void()> func) {
    constexpr s32 ServiceThreadPriority = 16;
    constexpr s32 ServiceThreadCore = 3;

    // Make a new process.
    KProcess* process = KProcess::Create(*this);
    ASSERT(R_SUCCEEDED(
        process->Initialize(Svc::CreateProcessParameter{}, GetSystemResourceLimit(), false)));

    // Ensure that we don't hold onto any extra references.
    SCOPE_EXIT {
        process->Close();
    };

    // Register the new process.
    KProcess::Register(*this, process);

    // Reserve a new thread from the process resource limit.
    KScopedResourceReservation thread_reservation(process, LimitableResource::ThreadCountMax);
    ASSERT(thread_reservation.Succeeded());

    // Initialize the thread.
    KThread* thread = KThread::Create(*this);
    ASSERT(R_SUCCEEDED(KThread::InitializeServiceThread(
        System(), thread, std::move(func), ServiceThreadPriority, ServiceThreadCore, process)));

    // Commit the thread reservation.
    thread_reservation.Commit();

    // Register the new thread.
    KThread::Register(*this, thread);

    // Begin running the thread.
    ASSERT(R_SUCCEEDED(thread->Run()));
}

u32 KernelCore::GetCurrentHostThreadID() const {
    return impl->GetCurrentHostThreadID();
}

KThread* KernelCore::GetCurrentEmuThread() const {
    return impl->GetCurrentEmuThread();
}

void KernelCore::SetCurrentEmuThread(KThread* thread) {
    impl->SetCurrentEmuThread(thread);
}

KObjectNameGlobalData& KernelCore::ObjectNameGlobalData() {
    return *impl->object_name_global_data;
}

KMemoryManager& KernelCore::MemoryManager() {
    return *impl->memory_manager;
}

const KMemoryManager& KernelCore::MemoryManager() const {
    return *impl->memory_manager;
}

KSystemResource& KernelCore::GetAppSystemResource() {
    return *impl->app_system_resource;
}

const KSystemResource& KernelCore::GetAppSystemResource() const {
    return *impl->app_system_resource;
}

KSystemResource& KernelCore::GetSystemSystemResource() {
    return *impl->sys_system_resource;
}

const KSystemResource& KernelCore::GetSystemSystemResource() const {
    return *impl->sys_system_resource;
}

Kernel::KSharedMemory& KernelCore::GetFontSharedMem() {
    return *impl->font_shared_mem;
}

const Kernel::KSharedMemory& KernelCore::GetFontSharedMem() const {
    return *impl->font_shared_mem;
}

Kernel::KSharedMemory& KernelCore::GetIrsSharedMem() {
    return *impl->irs_shared_mem;
}

const Kernel::KSharedMemory& KernelCore::GetIrsSharedMem() const {
    return *impl->irs_shared_mem;
}

Kernel::KSharedMemory& KernelCore::GetTimeSharedMem() {
    return *impl->time_shared_mem;
}

const Kernel::KSharedMemory& KernelCore::GetTimeSharedMem() const {
    return *impl->time_shared_mem;
}

Kernel::KSharedMemory& KernelCore::GetHidBusSharedMem() {
    return *impl->hidbus_shared_mem;
}

const Kernel::KSharedMemory& KernelCore::GetHidBusSharedMem() const {
    return *impl->hidbus_shared_mem;
}

void KernelCore::SuspendEmulation(bool suspended) {
    const bool should_suspend{exception_exited || suspended};
    auto processes = GetProcessList();

    for (auto& process : processes) {
        KScopedLightLock ll{process->GetListLock()};

        for (auto& thread : process->GetThreadList()) {
            if (should_suspend) {
                thread.RequestSuspend(SuspendType::System);
            } else {
                thread.Resume(SuspendType::System);
            }
        }
    }

    if (!should_suspend) {
        return;
    }

    // Wait for process execution to stop.
    // KernelCore::SuspendEmulation must be called from locked context,
    // or we could race another call, interfering with waiting.
    const auto TryWait = [&]() {
        KScopedSchedulerLock sl{*this};

        for (auto& process : processes) {
            for (auto i = 0; i < static_cast<s32>(Core::Hardware::NUM_CPU_CORES); ++i) {
                if (Scheduler(i).GetSchedulerCurrentThread()->GetOwnerProcess() ==
                    process.GetPointerUnsafe()) {
                    // A thread has not finished running yet.
                    // Continue waiting.
                    return false;
                }
            }
        }

        return true;
    };

    while (!TryWait()) {
        // ...
    }
}

void KernelCore::ShutdownCores() {
    impl->TerminateAllProcesses();

    KScopedSchedulerLock lk{*this};

    for (auto* thread : impl->shutdown_threads) {
        void(thread->Run());
    }
}

bool KernelCore::IsMulticore() const {
    return impl->is_multicore;
}

bool KernelCore::IsShuttingDown() const {
    return impl->IsShuttingDown();
}

void KernelCore::ExceptionalExitApplication() {
    exception_exited = true;
    SuspendEmulation(true);
}

void KernelCore::EnterSVCProfile() {
#if MICROPROFILE_ENABLED == 1
    impl->svc_ticks[CurrentPhysicalCoreIndex()] = MicroProfileEnter(MICROPROFILE_TOKEN(Kernel_SVC));
#endif
}

void KernelCore::ExitSVCProfile() {
#if MICROPROFILE_ENABLED == 1
    MicroProfileLeave(MICROPROFILE_TOKEN(Kernel_SVC), impl->svc_ticks[CurrentPhysicalCoreIndex()]);
#endif
}

Init::KSlabResourceCounts& KernelCore::SlabResourceCounts() {
    return impl->slab_resource_counts;
}

const Init::KSlabResourceCounts& KernelCore::SlabResourceCounts() const {
    return impl->slab_resource_counts;
}

KWorkerTaskManager& KernelCore::WorkerTaskManager() {
    return impl->worker_task_manager;
}

const KWorkerTaskManager& KernelCore::WorkerTaskManager() const {
    return impl->worker_task_manager;
}

const KMemoryLayout& KernelCore::MemoryLayout() const {
    return *impl->memory_layout;
}

bool KernelCore::IsPhantomModeForSingleCore() const {
    return impl->IsPhantomModeForSingleCore();
}

void KernelCore::SetIsPhantomModeForSingleCore(bool value) {
    impl->SetIsPhantomModeForSingleCore(value);
}

Core::System& KernelCore::System() {
    return impl->system;
}

const Core::System& KernelCore::System() const {
    return impl->system;
}

struct KernelCore::SlabHeapContainer {
    KSlabHeap<KClientSession> client_session;
    KSlabHeap<KEvent> event;
    KSlabHeap<KPort> port;
    KSlabHeap<KProcess> process;
    KSlabHeap<KResourceLimit> resource_limit;
    KSlabHeap<KSession> session;
    KSlabHeap<KLightSession> light_session;
    KSlabHeap<KSharedMemory> shared_memory;
    KSlabHeap<KSharedMemoryInfo> shared_memory_info;
    KSlabHeap<KThread> thread;
    KSlabHeap<KTransferMemory> transfer_memory;
    KSlabHeap<KCodeMemory> code_memory;
    KSlabHeap<KDeviceAddressSpace> device_address_space;
    KSlabHeap<KPageBuffer> page_buffer;
    KSlabHeap<KThreadLocalPage> thread_local_page;
    KSlabHeap<KObjectName> object_name;
    KSlabHeap<KSessionRequest> session_request;
    KSlabHeap<KSecureSystemResource> secure_system_resource;
    KSlabHeap<KThread::LockWithPriorityInheritanceInfo> lock_info;
    KSlabHeap<KEventInfo> event_info;
    KSlabHeap<KDebug> debug;
};

template <typename T>
KSlabHeap<T>& KernelCore::SlabHeap() {
    if constexpr (std::is_same_v<T, KClientSession>) {
        return slab_heap_container->client_session;
    } else if constexpr (std::is_same_v<T, KEvent>) {
        return slab_heap_container->event;
    } else if constexpr (std::is_same_v<T, KPort>) {
        return slab_heap_container->port;
    } else if constexpr (std::is_same_v<T, KProcess>) {
        return slab_heap_container->process;
    } else if constexpr (std::is_same_v<T, KResourceLimit>) {
        return slab_heap_container->resource_limit;
    } else if constexpr (std::is_same_v<T, KSession>) {
        return slab_heap_container->session;
    } else if constexpr (std::is_same_v<T, KLightSession>) {
        return slab_heap_container->light_session;
    } else if constexpr (std::is_same_v<T, KSharedMemory>) {
        return slab_heap_container->shared_memory;
    } else if constexpr (std::is_same_v<T, KSharedMemoryInfo>) {
        return slab_heap_container->shared_memory_info;
    } else if constexpr (std::is_same_v<T, KThread>) {
        return slab_heap_container->thread;
    } else if constexpr (std::is_same_v<T, KTransferMemory>) {
        return slab_heap_container->transfer_memory;
    } else if constexpr (std::is_same_v<T, KCodeMemory>) {
        return slab_heap_container->code_memory;
    } else if constexpr (std::is_same_v<T, KDeviceAddressSpace>) {
        return slab_heap_container->device_address_space;
    } else if constexpr (std::is_same_v<T, KPageBuffer>) {
        return slab_heap_container->page_buffer;
    } else if constexpr (std::is_same_v<T, KThreadLocalPage>) {
        return slab_heap_container->thread_local_page;
    } else if constexpr (std::is_same_v<T, KObjectName>) {
        return slab_heap_container->object_name;
    } else if constexpr (std::is_same_v<T, KSessionRequest>) {
        return slab_heap_container->session_request;
    } else if constexpr (std::is_same_v<T, KSecureSystemResource>) {
        return slab_heap_container->secure_system_resource;
    } else if constexpr (std::is_same_v<T, KThread::LockWithPriorityInheritanceInfo>) {
        return slab_heap_container->lock_info;
    } else if constexpr (std::is_same_v<T, KEventInfo>) {
        return slab_heap_container->event_info;
    } else if constexpr (std::is_same_v<T, KDebug>) {
        return slab_heap_container->debug;
    }
}

template KSlabHeap<KClientSession>& KernelCore::SlabHeap();
template KSlabHeap<KEvent>& KernelCore::SlabHeap();
template KSlabHeap<KPort>& KernelCore::SlabHeap();
template KSlabHeap<KProcess>& KernelCore::SlabHeap();
template KSlabHeap<KResourceLimit>& KernelCore::SlabHeap();
template KSlabHeap<KSession>& KernelCore::SlabHeap();
template KSlabHeap<KLightSession>& KernelCore::SlabHeap();
template KSlabHeap<KSharedMemory>& KernelCore::SlabHeap();
template KSlabHeap<KSharedMemoryInfo>& KernelCore::SlabHeap();
template KSlabHeap<KThread>& KernelCore::SlabHeap();
template KSlabHeap<KTransferMemory>& KernelCore::SlabHeap();
template KSlabHeap<KCodeMemory>& KernelCore::SlabHeap();
template KSlabHeap<KDeviceAddressSpace>& KernelCore::SlabHeap();
template KSlabHeap<KPageBuffer>& KernelCore::SlabHeap();
template KSlabHeap<KThreadLocalPage>& KernelCore::SlabHeap();
template KSlabHeap<KObjectName>& KernelCore::SlabHeap();
template KSlabHeap<KSessionRequest>& KernelCore::SlabHeap();
template KSlabHeap<KSecureSystemResource>& KernelCore::SlabHeap();
template KSlabHeap<KThread::LockWithPriorityInheritanceInfo>& KernelCore::SlabHeap();
template KSlabHeap<KEventInfo>& KernelCore::SlabHeap();
template KSlabHeap<KDebug>& KernelCore::SlabHeap();

} // namespace Kernel
