// SPDX-FileCopyrightText: 2014 Citra Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <cstddef>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <span>
#include <string>
#include <vector>

#include "common/common_types.h"
#include "core/file_sys/vfs/vfs_types.h"

namespace Core::Frontend {
class EmuWindow;
} // namespace Core::Frontend

namespace FileSys {
class ContentProvider;
class ContentProviderUnion;
enum class ContentProviderUnionSlot;
class VfsFilesystem;
} // namespace FileSys

namespace Kernel {
class GlobalSchedulerContext;
class KernelCore;
class PhysicalCore;
class KProcess;
class KScheduler;
} // namespace Kernel

namespace Loader {
class AppLoader;
enum class ResultStatus : u16;
} // namespace Loader

namespace Core::Memory {
struct CheatEntry;
class Memory;
} // namespace Core::Memory

namespace Service {

namespace Account {
class ProfileManager;
} // namespace Account

namespace AM {
struct FrontendAppletParameters;
class AppletManager;
} // namespace AM

namespace AM::Frontend {
struct FrontendAppletSet;
class FrontendAppletHolder;
} // namespace AM::Frontend

namespace APM {
class Controller;
}

namespace FileSystem {
class FileSystemController;
} // namespace FileSystem

namespace Glue {
class ARPManager;
}

class ServerManager;

namespace SM {
class ServiceManager;
} // namespace SM

} // namespace Service

namespace Tegra {
class DebugContext;
class GPU;
namespace Host1x {
class Host1x;
} // namespace Host1x
} // namespace Tegra

namespace VideoCore {
class RendererBase;
} // namespace VideoCore

namespace AudioCore {
class AudioCore;
} // namespace AudioCore

namespace Core::Timing {
class CoreTiming;
}

namespace Core::HID {
class HIDCore;
}

namespace Network {
class RoomNetwork;
}

namespace Tools {
class RenderdocAPI;
}

namespace Core {

class CpuManager;
class Debugger;
class DeviceMemory;
class ExclusiveMonitor;
class GPUDirtyMemoryManager;
class PerfStats;
class Reporter;
class SpeedLimiter;
class TelemetrySession;

struct PerfStatsResults;

FileSys::VirtualFile GetGameFileFromPath(const FileSys::VirtualFilesystem& vfs,
                                         const std::string& path);

/// Enumeration representing the return values of the System Initialize and Load process.
enum class SystemResultStatus : u32 {
    Success,             ///< Succeeded
    ErrorNotInitialized, ///< Error trying to use core prior to initialization
    ErrorGetLoader,      ///< Error finding the correct application loader
    ErrorSystemFiles,    ///< Error in finding system files
    ErrorSharedFont,     ///< Error in finding shared font
    ErrorVideoCore,      ///< Error in the video core
    ErrorUnknown,        ///< Any other error
    ErrorLoader,         ///< The base for loader errors (too many to repeat)
};

class System {
public:
    using CurrentBuildProcessID = std::array<u8, 0x20>;

    explicit System();

    ~System();

    System(const System&) = delete;
    System& operator=(const System&) = delete;

    System(System&&) = delete;
    System& operator=(System&&) = delete;

    /**
     * Initializes the system
     * This function will initialize core functionality used for system emulation
     */
    void Initialize();

    /**
     * Run the OS and Application
     * This function will start emulation and run the relevant devices
     */
    void Run();

    /**
     * Pause the OS and Application
     * This function will pause emulation and stop the relevant devices
     */
    void Pause();

    /// Check if the core is currently paused.
    [[nodiscard]] bool IsPaused() const;

    /// Shutdown the main emulated process.
    void ShutdownMainProcess();

    /// Check if the core is shutting down.
    [[nodiscard]] bool IsShuttingDown() const;

    /// Set the shutting down state.
    void SetShuttingDown(bool shutting_down);

    /// Forcibly detach the debugger if it is running.
    void DetachDebugger();

    std::unique_lock<std::mutex> StallApplication();
    void UnstallApplication();

    void SetNVDECActive(bool is_nvdec_active);
    [[nodiscard]] bool GetNVDECActive();

    /**
     * Initialize the debugger.
     */
    void InitializeDebugger();

    /**
     * Load an executable application.
     * @param emu_window Reference to the host-system window used for video output and keyboard
     *                   input.
     * @param filepath String path to the executable application to load on the host file system.
     * @param program_index Specifies the index within the container of the program to launch.
     * @returns SystemResultStatus code, indicating if the operation succeeded.
     */
    [[nodiscard]] SystemResultStatus Load(Frontend::EmuWindow& emu_window,
                                          const std::string& filepath,
                                          Service::AM::FrontendAppletParameters& params);

    /**
     * Indicates if the emulated system is powered on (all subsystems initialized and able to run an
     * application).
     * @returns True if the emulated system is powered on, otherwise false.
     */
    [[nodiscard]] bool IsPoweredOn() const;

    /// Gets a reference to the telemetry session for this emulation session.
    [[nodiscard]] Core::TelemetrySession& TelemetrySession();

    /// Gets a reference to the telemetry session for this emulation session.
    [[nodiscard]] const Core::TelemetrySession& TelemetrySession() const;

    /// Prepare the core emulation for a reschedule
    void PrepareReschedule(u32 core_index);

    std::span<GPUDirtyMemoryManager> GetGPUDirtyMemoryManager();

    void GatherGPUDirtyMemory(std::function<void(PAddr, size_t)>& callback);

    [[nodiscard]] size_t GetCurrentHostThreadID() const;

    /// Gets and resets core performance statistics
    [[nodiscard]] PerfStatsResults GetAndResetPerfStats();

    /// Gets the physical core for the CPU core that is currently running
    [[nodiscard]] Kernel::PhysicalCore& CurrentPhysicalCore();

    /// Gets the physical core for the CPU core that is currently running
    [[nodiscard]] const Kernel::PhysicalCore& CurrentPhysicalCore() const;

    /// Gets a reference to the underlying CPU manager.
    [[nodiscard]] CpuManager& GetCpuManager();

    /// Gets a const reference to the underlying CPU manager
    [[nodiscard]] const CpuManager& GetCpuManager() const;

    /// Gets a mutable reference to the system memory instance.
    [[nodiscard]] Core::Memory::Memory& ApplicationMemory();

    /// Gets a constant reference to the system memory instance.
    [[nodiscard]] const Core::Memory::Memory& ApplicationMemory() const;

    /// Gets a mutable reference to the GPU interface
    [[nodiscard]] Tegra::GPU& GPU();

    /// Gets an immutable reference to the GPU interface.
    [[nodiscard]] const Tegra::GPU& GPU() const;

    /// Gets a mutable reference to the Host1x interface
    [[nodiscard]] Tegra::Host1x::Host1x& Host1x();

    /// Gets an immutable reference to the Host1x interface.
    [[nodiscard]] const Tegra::Host1x::Host1x& Host1x() const;

    /// Gets a mutable reference to the renderer.
    [[nodiscard]] VideoCore::RendererBase& Renderer();

    /// Gets an immutable reference to the renderer.
    [[nodiscard]] const VideoCore::RendererBase& Renderer() const;

    /// Gets a mutable reference to the audio interface
    [[nodiscard]] AudioCore::AudioCore& AudioCore();

    /// Gets an immutable reference to the audio interface.
    [[nodiscard]] const AudioCore::AudioCore& AudioCore() const;

    /// Gets the global scheduler
    [[nodiscard]] Kernel::GlobalSchedulerContext& GlobalSchedulerContext();

    /// Gets the global scheduler
    [[nodiscard]] const Kernel::GlobalSchedulerContext& GlobalSchedulerContext() const;

    /// Gets the manager for the guest device memory
    [[nodiscard]] Core::DeviceMemory& DeviceMemory();

    /// Gets the manager for the guest device memory
    [[nodiscard]] const Core::DeviceMemory& DeviceMemory() const;

    /// Provides a pointer to the application process
    [[nodiscard]] Kernel::KProcess* ApplicationProcess();

    /// Provides a constant pointer to the application process.
    [[nodiscard]] const Kernel::KProcess* ApplicationProcess() const;

    /// Provides a reference to the core timing instance.
    [[nodiscard]] Timing::CoreTiming& CoreTiming();

    /// Provides a constant reference to the core timing instance.
    [[nodiscard]] const Timing::CoreTiming& CoreTiming() const;

    /// Provides a reference to the kernel instance.
    [[nodiscard]] Kernel::KernelCore& Kernel();

    /// Provides a constant reference to the kernel instance.
    [[nodiscard]] const Kernel::KernelCore& Kernel() const;

    /// Gets a mutable reference to the HID interface.
    [[nodiscard]] HID::HIDCore& HIDCore();

    /// Gets an immutable reference to the HID interface.
    [[nodiscard]] const HID::HIDCore& HIDCore() const;

    /// Provides a reference to the internal PerfStats instance.
    [[nodiscard]] Core::PerfStats& GetPerfStats();

    /// Provides a constant reference to the internal PerfStats instance.
    [[nodiscard]] const Core::PerfStats& GetPerfStats() const;

    /// Provides a reference to the speed limiter;
    [[nodiscard]] Core::SpeedLimiter& SpeedLimiter();

    /// Provides a constant reference to the speed limiter
    [[nodiscard]] const Core::SpeedLimiter& SpeedLimiter() const;

    [[nodiscard]] u64 GetApplicationProcessProgramID() const;

    /// Gets the name of the current game
    [[nodiscard]] Loader::ResultStatus GetGameName(std::string& out) const;

    void SetStatus(SystemResultStatus new_status, const char* details);

    [[nodiscard]] const std::string& GetStatusDetails() const;

    [[nodiscard]] Loader::AppLoader& GetAppLoader();
    [[nodiscard]] const Loader::AppLoader& GetAppLoader() const;

    [[nodiscard]] Service::SM::ServiceManager& ServiceManager();
    [[nodiscard]] const Service::SM::ServiceManager& ServiceManager() const;

    void SetFilesystem(FileSys::VirtualFilesystem vfs);

    [[nodiscard]] FileSys::VirtualFilesystem GetFilesystem() const;

    void RegisterCheatList(const std::vector<Memory::CheatEntry>& list,
                           const std::array<u8, 0x20>& build_id, u64 main_region_begin,
                           u64 main_region_size);

    void SetFrontendAppletSet(Service::AM::Frontend::FrontendAppletSet&& set);

    [[nodiscard]] Service::AM::Frontend::FrontendAppletHolder& GetFrontendAppletHolder();
    [[nodiscard]] const Service::AM::Frontend::FrontendAppletHolder& GetFrontendAppletHolder()
        const;

    [[nodiscard]] Service::AM::AppletManager& GetAppletManager();

    void SetContentProvider(std::unique_ptr<FileSys::ContentProviderUnion> provider);

    [[nodiscard]] FileSys::ContentProvider& GetContentProvider();
    [[nodiscard]] const FileSys::ContentProvider& GetContentProvider() const;

    [[nodiscard]] FileSys::ContentProviderUnion& GetContentProviderUnion();
    [[nodiscard]] const FileSys::ContentProviderUnion& GetContentProviderUnion() const;

    [[nodiscard]] Service::FileSystem::FileSystemController& GetFileSystemController();
    [[nodiscard]] const Service::FileSystem::FileSystemController& GetFileSystemController() const;

    void RegisterContentProvider(FileSys::ContentProviderUnionSlot slot,
                                 FileSys::ContentProvider* provider);

    void ClearContentProvider(FileSys::ContentProviderUnionSlot slot);

    [[nodiscard]] const Reporter& GetReporter() const;

    [[nodiscard]] Service::Glue::ARPManager& GetARPManager();
    [[nodiscard]] const Service::Glue::ARPManager& GetARPManager() const;

    [[nodiscard]] Service::APM::Controller& GetAPMController();
    [[nodiscard]] const Service::APM::Controller& GetAPMController() const;

    [[nodiscard]] Service::Account::ProfileManager& GetProfileManager();
    [[nodiscard]] const Service::Account::ProfileManager& GetProfileManager() const;

    [[nodiscard]] Core::Debugger& GetDebugger();
    [[nodiscard]] const Core::Debugger& GetDebugger() const;

    /// Gets a mutable reference to the Room Network.
    [[nodiscard]] Network::RoomNetwork& GetRoomNetwork();

    /// Gets an immutable reference to the Room Network.
    [[nodiscard]] const Network::RoomNetwork& GetRoomNetwork() const;

    [[nodiscard]] Tools::RenderdocAPI& GetRenderdocAPI();

    void SetExitLocked(bool locked);
    bool GetExitLocked() const;

    void SetExitRequested(bool requested);
    bool GetExitRequested() const;

    void SetApplicationProcessBuildID(const CurrentBuildProcessID& id);
    [[nodiscard]] const CurrentBuildProcessID& GetApplicationProcessBuildID() const;

    /// Register a host thread as an emulated CPU Core.
    void RegisterCoreThread(std::size_t id);

    /// Register a host thread as an auxiliary thread.
    void RegisterHostThread();

    /// Tells if system is running on multicore.
    [[nodiscard]] bool IsMulticore() const;

    /// Tells if the system debugger is enabled.
    [[nodiscard]] bool DebuggerEnabled() const;

    /// Runs a server instance until shutdown.
    void RunServer(std::unique_ptr<Service::ServerManager>&& server_manager);

    /// Type used for the frontend to designate a callback for System to re-launch the application
    /// using a specified program index.
    using ExecuteProgramCallback = std::function<void(std::size_t)>;

    /**
     * Registers a callback from the frontend for System to re-launch the application using a
     * specified program index.
     * @param callback Callback from the frontend to relaunch the application.
     */
    void RegisterExecuteProgramCallback(ExecuteProgramCallback&& callback);

    /**
     * Instructs the frontend to re-launch the application using the specified program_index.
     * @param program_index Specifies the index within the application of the program to launch.
     */
    void ExecuteProgram(std::size_t program_index);

    /**
     * Gets a reference to the user channel stack.
     * It is used to transfer data between programs.
     */
    [[nodiscard]] std::deque<std::vector<u8>>& GetUserChannel();

    /// Type used for the frontend to designate a callback for System to exit the application.
    using ExitCallback = std::function<void()>;

    /**
     * Registers a callback from the frontend for System to exit the application.
     * @param callback Callback from the frontend to exit the application.
     */
    void RegisterExitCallback(ExitCallback&& callback);

    /// Instructs the frontend to exit the application.
    void Exit();

    /// Applies any changes to settings to this core instance.
    void ApplySettings();

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};

} // namespace Core
