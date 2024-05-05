// SPDX-FileCopyrightText: Copyright 2021 sudachi Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <functional>
#include <list>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "common/polyfill_thread.h"
#include "core/hardware_properties.h"
#include "core/hle/kernel/k_auto_object.h"
#include "core/hle/kernel/k_slab_heap.h"
#include "core/hle/kernel/k_typed_address.h"
#include "core/hle/kernel/svc_common.h"

namespace Core {
class ExclusiveMonitor;
class System;
} // namespace Core

namespace Core::Timing {
class CoreTiming;
struct EventType;
} // namespace Core::Timing

namespace Service {
class ServerManager;
}

namespace Service::SM {
class ServiceManager;
}

namespace Kernel {

class KClientPort;
class GlobalSchedulerContext;
class KAutoObjectWithListContainer;
class KClientSession;
class KDebug;
class KDeviceAddressSpace;
class KDynamicPageManager;
class KEvent;
class KEventInfo;
class KHandleTable;
class KHardwareTimer;
class KMemoryLayout;
class KMemoryManager;
class KObjectName;
class KObjectNameGlobalData;
class KPageBuffer;
class KPageBufferSlabHeap;
class KPort;
class KProcess;
class KResourceLimit;
class KScheduler;
class KServerPort;
class KServerSession;
class KSession;
class KSessionRequest;
class KSharedMemory;
class KSharedMemoryInfo;
class KSecureSystemResource;
class KThread;
class KThreadLocalPage;
class KTransferMemory;
class KWorkerTaskManager;
class KCodeMemory;
class PhysicalCore;

namespace Init {
struct KSlabResourceCounts;
}

template <typename T>
class KSlabHeap;

/// Represents a single instance of the kernel.
class KernelCore {
public:
    /// Constructs an instance of the kernel using the given System
    /// instance as a context for any necessary system-related state,
    /// such as threads, CPU core state, etc.
    ///
    /// @post After execution of the constructor, the provided System
    ///       object *must* outlive the kernel instance itself.
    ///
    explicit KernelCore(Core::System& system);
    ~KernelCore();

    KernelCore(const KernelCore&) = delete;
    KernelCore& operator=(const KernelCore&) = delete;

    KernelCore(KernelCore&&) = delete;
    KernelCore& operator=(KernelCore&&) = delete;

    /// Sets if emulation is multicore or single core, must be set before Initialize
    void SetMulticore(bool is_multicore);

    /// Resets the kernel to a clean slate for use.
    void Initialize();

    /// Clears all resources in use by the kernel instance.
    void Shutdown();

    /// Close all active services in use by the kernel instance.
    void CloseServices();

    /// Retrieves a shared pointer to the system resource limit instance.
    const KResourceLimit* GetSystemResourceLimit() const;

    /// Retrieves a shared pointer to the system resource limit instance.
    KResourceLimit* GetSystemResourceLimit();

    /// Adds/removes the given pointer to an internal list of active processes.
    void AppendNewProcess(KProcess* process);
    void RemoveProcess(KProcess* process);

    /// Makes the given process the new application process.
    void MakeApplicationProcess(KProcess* process);

    /// Retrieves a pointer to the application process.
    KProcess* ApplicationProcess();

    /// Retrieves a const pointer to the application process.
    const KProcess* ApplicationProcess() const;

    /// Retrieves the list of processes.
    std::list<KScopedAutoObject<KProcess>> GetProcessList();

    /// Gets the sole instance of the global scheduler
    Kernel::GlobalSchedulerContext& GlobalSchedulerContext();

    /// Gets the sole instance of the global scheduler
    const Kernel::GlobalSchedulerContext& GlobalSchedulerContext() const;

    /// Gets the sole instance of the Scheduler assoviated with cpu core 'id'
    Kernel::KScheduler& Scheduler(std::size_t id);

    /// Gets the sole instance of the Scheduler assoviated with cpu core 'id'
    const Kernel::KScheduler& Scheduler(std::size_t id) const;

    /// Gets the an instance of the respective physical CPU core.
    Kernel::PhysicalCore& PhysicalCore(std::size_t id);

    /// Gets the an instance of the respective physical CPU core.
    const Kernel::PhysicalCore& PhysicalCore(std::size_t id) const;

    /// Gets the current physical core index for the running host thread.
    std::size_t CurrentPhysicalCoreIndex() const;

    /// Gets the sole instance of the Scheduler at the current running core.
    Kernel::KScheduler* CurrentScheduler();

    /// Gets the an instance of the current physical CPU core.
    Kernel::PhysicalCore& CurrentPhysicalCore();

    /// Gets the an instance of the current physical CPU core.
    const Kernel::PhysicalCore& CurrentPhysicalCore() const;

    /// Gets the an instance of the hardware timer.
    Kernel::KHardwareTimer& HardwareTimer();

    /// Stops execution of 'id' core, in order to reschedule a new thread.
    void PrepareReschedule(std::size_t id);

    KAutoObjectWithListContainer& ObjectListContainer();

    const KAutoObjectWithListContainer& ObjectListContainer() const;

    /// Registers all kernel objects with the global emulation state, this is purely for tracking
    /// leaks after emulation has been shutdown.
    void RegisterKernelObject(KAutoObject* object);

    /// Unregisters a kernel object previously registered with RegisterKernelObject when it was
    /// destroyed during the current emulation session.
    void UnregisterKernelObject(KAutoObject* object);

    /// Registers kernel objects with guest in use state, this is purely for close
    /// after emulation has been shutdown.
    void RegisterInUseObject(KAutoObject* object);

    /// Unregisters a kernel object previously registered with RegisterInUseObject when it was
    /// destroyed during the current emulation session.
    void UnregisterInUseObject(KAutoObject* object);

    // Runs the given server manager until shutdown.
    void RunServer(std::unique_ptr<Service::ServerManager>&& server_manager);

    /// Gets the current host_thread/guest_thread pointer.
    KThread* GetCurrentEmuThread() const;

    /// Sets the current guest_thread pointer.
    void SetCurrentEmuThread(KThread* thread);

    /// Gets the current host_thread handle.
    u32 GetCurrentHostThreadID() const;

    /// Register the current thread as a CPU Core Thread.
    void RegisterCoreThread(std::size_t core_id);

    /// Register the current thread as a non CPU core thread.
    void RegisterHostThread(KThread* existing_thread = nullptr);

    void RunOnGuestCoreProcess(std::string&& process_name, std::function<void()> func);

    std::jthread RunOnHostCoreProcess(std::string&& process_name, std::function<void()> func);

    std::jthread RunOnHostCoreThread(std::string&& thread_name, std::function<void()> func);

    /// Gets global data for KObjectName.
    KObjectNameGlobalData& ObjectNameGlobalData();

    /// Gets the virtual memory manager for the kernel.
    KMemoryManager& MemoryManager();

    /// Gets the virtual memory manager for the kernel.
    const KMemoryManager& MemoryManager() const;

    /// Gets the application resource manager.
    KSystemResource& GetAppSystemResource();

    /// Gets the application resource manager.
    const KSystemResource& GetAppSystemResource() const;

    /// Gets the system resource manager.
    KSystemResource& GetSystemSystemResource();

    /// Gets the system resource manager.
    const KSystemResource& GetSystemSystemResource() const;

    /// Gets the shared memory object for font services.
    Kernel::KSharedMemory& GetFontSharedMem();

    /// Gets the shared memory object for font services.
    const Kernel::KSharedMemory& GetFontSharedMem() const;

    /// Gets the shared memory object for IRS services.
    Kernel::KSharedMemory& GetIrsSharedMem();

    /// Gets the shared memory object for IRS services.
    const Kernel::KSharedMemory& GetIrsSharedMem() const;

    /// Gets the shared memory object for Time services.
    Kernel::KSharedMemory& GetTimeSharedMem();

    /// Gets the shared memory object for Time services.
    const Kernel::KSharedMemory& GetTimeSharedMem() const;

    /// Gets the shared memory object for HIDBus services.
    Kernel::KSharedMemory& GetHidBusSharedMem();

    /// Gets the shared memory object for HIDBus services.
    const Kernel::KSharedMemory& GetHidBusSharedMem() const;

    /// Suspend/unsuspend emulated processes.
    void SuspendEmulation(bool suspend);

    /// Exceptional exit application process.
    void ExceptionalExitApplication();

    /// Notify emulated CPU cores to shut down.
    void ShutdownCores();

    bool IsMulticore() const;

    bool IsShuttingDown() const;

    /// Workaround for single-core mode when preempting threads while idle.
    bool IsPhantomModeForSingleCore() const;
    void SetIsPhantomModeForSingleCore(bool value);

    Core::System& System();
    const Core::System& System() const;

    /// Gets the slab heap for the specified kernel object type.
    template <typename T>
    KSlabHeap<T>& SlabHeap();

    /// Gets the current slab resource counts.
    Init::KSlabResourceCounts& SlabResourceCounts();

    /// Gets the current slab resource counts.
    const Init::KSlabResourceCounts& SlabResourceCounts() const;

    /// Gets the current worker task manager, used for dispatching KThread/KProcess tasks.
    KWorkerTaskManager& WorkerTaskManager();

    /// Gets the current worker task manager, used for dispatching KThread/KProcess tasks.
    const KWorkerTaskManager& WorkerTaskManager() const;

    /// Gets the memory layout.
    const KMemoryLayout& MemoryLayout() const;

private:
    friend class KProcess;
    friend class KThread;

    /// Creates a new object ID, incrementing the internal object ID counter.
    u32 CreateNewObjectID();

    /// Creates a new process ID, incrementing the internal process ID counter;
    u64 CreateNewKernelProcessID();

    /// Creates a new process ID, incrementing the internal process ID counter;
    u64 CreateNewUserProcessID();

    /// Creates a new thread ID, incrementing the internal thread ID counter.
    u64 CreateNewThreadID();

    /// Provides a reference to the global handle table.
    KHandleTable& GlobalHandleTable();

    /// Provides a const reference to the global handle table.
    const KHandleTable& GlobalHandleTable() const;

    struct Impl;
    std::unique_ptr<Impl> impl;

    bool exception_exited{};

private:
    /// Helper to encapsulate all slab heaps in a single heap allocated container
    struct SlabHeapContainer;

    std::unique_ptr<SlabHeapContainer> slab_heap_container;
};

} // namespace Kernel
