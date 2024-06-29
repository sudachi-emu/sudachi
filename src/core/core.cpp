// SPDX-FileCopyrightText: 2014 Citra Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <array>
#include <atomic>
#include <memory>
#include <utility>

#include "audio_core/audio_core.h"
#include "common/fs/fs.h"
#include "common/logging/log.h"
#include "common/microprofile.h"
#include "common/settings.h"
#include "common/settings_enums.h"
#include "common/string_util.h"
#include "core/arm/exclusive_monitor.h"
#include "core/core.h"
#include "core/core_timing.h"
#include "core/cpu_manager.h"
#include "core/debugger/debugger.h"
#include "core/device_memory.h"
#include "core/file_sys/fs_filesystem.h"
#include "core/file_sys/patch_manager.h"
#include "core/file_sys/registered_cache.h"
#include "core/file_sys/romfs_factory.h"
#include "core/file_sys/savedata_factory.h"
#include "core/file_sys/vfs/vfs_concat.h"
#include "core/file_sys/vfs/vfs_real.h"
#include "core/gpu_dirty_memory_manager.h"
#include "core/hle/kernel/k_memory_manager.h"
#include "core/hle/kernel/k_process.h"
#include "core/hle/kernel/k_resource_limit.h"
#include "core/hle/kernel/k_scheduler.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/physical_core.h"
#include "core/hle/service/acc/profile_manager.h"
#include "core/hle/service/am/applet_manager.h"
#include "core/hle/service/am/frontend/applets.h"
#include "core/hle/service/am/process_creation.h"
#include "core/hle/service/apm/apm_controller.h"
#include "core/hle/service/filesystem/filesystem.h"
#include "core/hle/service/glue/glue_manager.h"
#include "core/hle/service/glue/time/static.h"
#include "core/hle/service/psc/time/static.h"
#include "core/hle/service/psc/time/steady_clock.h"
#include "core/hle/service/psc/time/system_clock.h"
#include "core/hle/service/psc/time/time_zone_service.h"
#include "core/hle/service/service.h"
#include "core/hle/service/services.h"
#include "core/hle/service/set/system_settings_server.h"
#include "core/hle/service/sm/sm.h"
#include "core/internal_network/network.h"
#include "core/loader/loader.h"
#include "core/memory.h"
#include "core/memory/cheat_engine.h"
#include "core/perf_stats.h"
#include "core/reporter.h"
#include "core/telemetry_session.h"
#include "core/tools/freezer.h"
#include "core/tools/renderdoc.h"
#include "hid_core/hid_core.h"
#include "network/network.h"
#include "video_core/host1x/host1x.h"
#include "video_core/renderer_base.h"
#include "video_core/video_core.h"

#if MICROPROFILE == 1
MICROPROFILE_DEFINE(ARM_CPU0, "ARM", "CPU 0", MP_RGB(255, 64, 64));
MICROPROFILE_DEFINE(ARM_CPU1, "ARM", "CPU 1", MP_RGB(255, 64, 64));
MICROPROFILE_DEFINE(ARM_CPU2, "ARM", "CPU 2", MP_RGB(255, 64, 64));
MICROPROFILE_DEFINE(ARM_CPU3, "ARM", "CPU 3", MP_RGB(255, 64, 64));
#endif

namespace Core {

FileSys::VirtualFile GetGameFileFromPath(const FileSys::VirtualFilesystem& vfs,
                                         const std::string& path) {
    // To account for split 00+01+etc files.
    std::string dir_name;
    std::string filename;
    Common::SplitPath(path, &dir_name, &filename, nullptr);

    if (filename == "00") {
        const auto dir = vfs->OpenDirectory(dir_name, FileSys::OpenMode::Read);
        std::vector<FileSys::VirtualFile> concat;

        for (u32 i = 0; i < 0x10; ++i) {
            const auto file_name = fmt::format("{:02X}", i);
            auto next = dir->GetFile(file_name);

            if (next != nullptr) {
                concat.push_back(std::move(next));
            } else {
                next = dir->GetFile(file_name);

                if (next == nullptr) {
                    break;
                }

                concat.push_back(std::move(next));
            }
        }

        return FileSys::ConcatenatedVfsFile::MakeConcatenatedFile(dir->GetName(),
                                                                  std::move(concat));
    }

    if (Common::FS::IsDir(path)) {
        return vfs->OpenFile(path + "/main", FileSys::OpenMode::Read);
    }

    return vfs->OpenFile(path, FileSys::OpenMode::Read);
}

struct System::Impl {
    explicit Impl(System& system)
        : kernel{system}, fs_controller{system}, hid_core{}, room_network{}, cpu_manager{system},
          reporter{system}, applet_manager{system}, frontend_applets{system}, profile_manager{} {}

    void Initialize(System& system) {
        device_memory = std::make_unique<Core::DeviceMemory>();

        is_multicore = Settings::values.use_multi_core.GetValue();
        extended_memory_layout =
            Settings::values.memory_layout_mode.GetValue() != Settings::MemoryLayout::Memory_4Gb;

        core_timing.SetMulticore(is_multicore);
        core_timing.Initialize([&system]() { system.RegisterHostThread(); });

        // Create a default fs if one doesn't already exist.
        if (virtual_filesystem == nullptr) {
            virtual_filesystem = std::make_shared<FileSys::RealVfsFilesystem>();
        }
        if (content_provider == nullptr) {
            content_provider = std::make_unique<FileSys::ContentProviderUnion>();
        }

        // Create default implementations of applets if one is not provided.
        frontend_applets.SetDefaultAppletsIfMissing();

        is_async_gpu = Settings::values.use_asynchronous_gpu_emulation.GetValue();

        kernel.SetMulticore(is_multicore);
        cpu_manager.SetMulticore(is_multicore);
        cpu_manager.SetAsyncGpu(is_async_gpu);
    }

    void ReinitializeIfNecessary(System& system) {
        const bool must_reinitialize =
            is_multicore != Settings::values.use_multi_core.GetValue() ||
            extended_memory_layout != (Settings::values.memory_layout_mode.GetValue() !=
                                       Settings::MemoryLayout::Memory_4Gb);

        if (!must_reinitialize) {
            return;
        }

        LOG_DEBUG(Kernel, "Re-initializing");

        is_multicore = Settings::values.use_multi_core.GetValue();
        extended_memory_layout =
            Settings::values.memory_layout_mode.GetValue() != Settings::MemoryLayout::Memory_4Gb;

        Initialize(system);
    }

    void RefreshTime(System& system) {
        if (!system.IsPoweredOn()) {
            return;
        }

        auto settings_service =
            system.ServiceManager().GetService<Service::Set::ISystemSettingsServer>("set:sys",
                                                                                    true);
        auto static_service_a =
            system.ServiceManager().GetService<Service::Glue::Time::StaticService>("time:a", true);

        auto static_service_s =
            system.ServiceManager().GetService<Service::PSC::Time::StaticService>("time:s", true);

        std::shared_ptr<Service::PSC::Time::SystemClock> user_clock;
        static_service_a->GetStandardUserSystemClock(&user_clock);

        std::shared_ptr<Service::PSC::Time::SystemClock> local_clock;
        static_service_a->GetStandardLocalSystemClock(&local_clock);

        std::shared_ptr<Service::PSC::Time::SystemClock> network_clock;
        static_service_s->GetStandardNetworkSystemClock(&network_clock);

        std::shared_ptr<Service::Glue::Time::TimeZoneService> timezone_service;
        static_service_a->GetTimeZoneService(&timezone_service);

        Service::PSC::Time::LocationName name{};
        auto new_name = Settings::GetTimeZoneString(Settings::values.time_zone_index.GetValue());
        std::memcpy(name.data(), new_name.data(), std::min(name.size(), new_name.size()));

        timezone_service->SetDeviceLocationName(name);

        u64 time_offset = 0;
        if (Settings::values.custom_rtc_enabled) {
            time_offset = Settings::values.custom_rtc_offset.GetValue();
        }

        const auto posix_time = std::chrono::system_clock::now().time_since_epoch();
        const u64 current_time =
            +std::chrono::duration_cast<std::chrono::seconds>(posix_time).count();
        const u64 new_time = current_time + time_offset;

        Service::PSC::Time::SystemClockContext context{};
        settings_service->SetUserSystemClockContext(context);
        user_clock->SetCurrentTime(new_time);

        local_clock->SetCurrentTime(new_time);

        network_clock->GetSystemClockContext(&context);
        settings_service->SetNetworkSystemClockContext(context);
        network_clock->SetCurrentTime(new_time);
    }

    void Run() {
        std::unique_lock<std::mutex> lk(suspend_guard);

        kernel.SuspendEmulation(false);
        core_timing.SyncPause(false);
        is_paused.store(false, std::memory_order_relaxed);
    }

    void Pause() {
        std::unique_lock<std::mutex> lk(suspend_guard);

        core_timing.SyncPause(true);
        kernel.SuspendEmulation(true);
        is_paused.store(true, std::memory_order_relaxed);
    }

    bool IsPaused() const {
        return is_paused.load(std::memory_order_relaxed);
    }

    std::unique_lock<std::mutex> StallApplication() {
        std::unique_lock<std::mutex> lk(suspend_guard);
        kernel.SuspendEmulation(true);
        core_timing.SyncPause(true);
        return lk;
    }

    void UnstallApplication() {
        if (!IsPaused()) {
            core_timing.SyncPause(false);
            kernel.SuspendEmulation(false);
        }
    }

    void SetNVDECActive(bool is_nvdec_active) {
        nvdec_active = is_nvdec_active;
    }

    bool GetNVDECActive() {
        return nvdec_active;
    }

    void InitializeDebugger(System& system, u16 port) {
        debugger = std::make_unique<Debugger>(system, port);
    }

    void InitializeKernel(System& system) {
        LOG_DEBUG(Core, "initialized OK");

        // Setting changes may require a full system reinitialization (e.g., disabling multicore).
        ReinitializeIfNecessary(system);

        kernel.Initialize();
        cpu_manager.Initialize();
    }

    SystemResultStatus SetupForApplicationProcess(System& system, Frontend::EmuWindow& emu_window) {
        telemetry_session = std::make_unique<Core::TelemetrySession>();

        host1x_core = std::make_unique<Tegra::Host1x::Host1x>(system);
        gpu_core = VideoCore::CreateGPU(emu_window, system);
        if (!gpu_core) {
            return SystemResultStatus::ErrorVideoCore;
        }

        audio_core = std::make_unique<AudioCore::AudioCore>(system);

        service_manager = std::make_shared<Service::SM::ServiceManager>(kernel);
        services =
            std::make_unique<Service::Services>(service_manager, system, stop_event.get_token());

        is_powered_on = true;
        exit_locked = false;
        exit_requested = false;

#if MICROPROFILE_ENABLED == 1
        microprofile_cpu[0] = MICROPROFILE_TOKEN(ARM_CPU0);
        microprofile_cpu[1] = MICROPROFILE_TOKEN(ARM_CPU1);
        microprofile_cpu[2] = MICROPROFILE_TOKEN(ARM_CPU2);
        microprofile_cpu[3] = MICROPROFILE_TOKEN(ARM_CPU3);
#endif

        if (Settings::values.enable_renderdoc_hotkey) {
            renderdoc_api = std::make_unique<Tools::RenderdocAPI>();
        }

        LOG_DEBUG(Core, "Initialized OK");

        return SystemResultStatus::Success;
    }

    SystemResultStatus Load(System& system, Frontend::EmuWindow& emu_window,
                            const std::string& filepath,
                            Service::AM::FrontendAppletParameters& params) {
        InitializeKernel(system);

        const auto file = GetGameFileFromPath(virtual_filesystem, filepath);

        // Create the application process
        Loader::ResultStatus load_result{};
        std::vector<u8> control;
        auto process =
            Service::AM::CreateApplicationProcess(control, app_loader, load_result, system, file,
                                                  params.program_id, params.program_index);

        if (load_result != Loader::ResultStatus::Success) {
            LOG_CRITICAL(Core, "Failed to load ROM (Error {})!", load_result);
            ShutdownMainProcess();

            return static_cast<SystemResultStatus>(
                static_cast<u32>(SystemResultStatus::ErrorLoader) + static_cast<u32>(load_result));
        }

        if (!app_loader) {
            LOG_CRITICAL(Core, "Failed to obtain loader for {}!", filepath);
            return SystemResultStatus::ErrorGetLoader;
        }

        if (app_loader->ReadProgramId(params.program_id) != Loader::ResultStatus::Success) {
            LOG_ERROR(Core, "Failed to find program id for ROM!");
        }

        std::string name = "Unknown program";
        if (app_loader->ReadTitle(name) != Loader::ResultStatus::Success) {
            LOG_ERROR(Core, "Failed to read title for ROM!");
        }

        LOG_INFO(Core, "Loading {} ({:016X}) ...", name, params.program_id);

        // Make the process created be the application
        kernel.MakeApplicationProcess(process->GetHandle());

        // Set up the rest of the system.
        SystemResultStatus init_result{SetupForApplicationProcess(system, emu_window)};
        if (init_result != SystemResultStatus::Success) {
            LOG_CRITICAL(Core, "Failed to initialize system (Error {})!",
                         static_cast<int>(init_result));
            ShutdownMainProcess();
            return init_result;
        }

        telemetry_session->AddInitialInfo(*app_loader, fs_controller, *content_provider);

        // Initialize cheat engine
        if (cheat_engine) {
            cheat_engine->Initialize();
        }

        // Register with applet manager
        // All threads are started, begin main process execution, now that we're in the clear
        applet_manager.CreateAndInsertByFrontendAppletParameters(std::move(process), params);

        if (Settings::values.gamecard_inserted) {
            if (Settings::values.gamecard_current_game) {
                fs_controller.SetGameCard(GetGameFileFromPath(virtual_filesystem, filepath));
            } else if (!Settings::values.gamecard_path.GetValue().empty()) {
                const auto& gamecard_path = Settings::values.gamecard_path.GetValue();
                fs_controller.SetGameCard(GetGameFileFromPath(virtual_filesystem, gamecard_path));
            }
        }

        perf_stats = std::make_unique<PerfStats>(params.program_id);
        // Reset counters and set time origin to current frame
        GetAndResetPerfStats();
        perf_stats->BeginSystemFrame();

        std::string title_version;
        const FileSys::PatchManager pm(params.program_id, system.GetFileSystemController(),
                                       system.GetContentProvider());
        const auto metadata = pm.GetControlMetadata();
        if (metadata.first != nullptr) {
            title_version = metadata.first->GetVersionString();
        }
        if (auto room_member = room_network.GetRoomMember().lock()) {
            Network::GameInfo game_info;
            game_info.name = name;
            game_info.id = params.program_id;
            game_info.version = title_version;
            room_member->SendGameInfo(game_info);
        }

        status = SystemResultStatus::Success;
        return status;
    }

    void ShutdownMainProcess() {
        SetShuttingDown(true);

        // Log last frame performance stats if game was loaded
        if (perf_stats) {
            const auto perf_results = GetAndResetPerfStats();
            constexpr auto performance = Common::Telemetry::FieldType::Performance;

            telemetry_session->AddField(performance, "Shutdown_EmulationSpeed",
                                        perf_results.emulation_speed * 100.0);
            telemetry_session->AddField(performance, "Shutdown_Framerate",
                                        perf_results.average_game_fps);
            telemetry_session->AddField(performance, "Shutdown_Frametime",
                                        perf_results.frametime * 1000.0);
            telemetry_session->AddField(performance, "Mean_Frametime_MS",
                                        perf_stats->GetMeanFrametime());
        }

        is_powered_on = false;
        exit_locked = false;
        exit_requested = false;

        if (gpu_core != nullptr) {
            gpu_core->NotifyShutdown();
        }

        stop_event.request_stop();
        core_timing.SyncPause(false);
        Network::CancelPendingSocketOperations();
        kernel.SuspendEmulation(true);
        kernel.CloseServices();
        kernel.ShutdownCores();
        services.reset();
        service_manager.reset();
        fs_controller.Reset();
        cheat_engine.reset();
        telemetry_session.reset();
        core_timing.ClearPendingEvents();
        app_loader.reset();
        audio_core.reset();
        gpu_core.reset();
        host1x_core.reset();
        perf_stats.reset();
        cpu_manager.Shutdown();
        debugger.reset();
        kernel.Shutdown();
        stop_event = {};
        Network::RestartSocketOperations();

        if (auto room_member = room_network.GetRoomMember().lock()) {
            Network::GameInfo game_info{};
            room_member->SendGameInfo(game_info);
        }

        // Reset all glue registrations
        arp_manager.ResetAll();

        LOG_DEBUG(Core, "Shutdown OK");
    }

    bool IsShuttingDown() const {
        return is_shutting_down;
    }

    void SetShuttingDown(bool shutting_down) {
        is_shutting_down = shutting_down;
    }

    Loader::ResultStatus GetGameName(std::string& out) const {
        if (app_loader == nullptr)
            return Loader::ResultStatus::ErrorNotInitialized;
        return app_loader->ReadTitle(out);
    }

    void SetStatus(SystemResultStatus new_status, const char* details = nullptr) {
        status = new_status;
        if (details) {
            status_details = details;
        }
    }

    PerfStatsResults GetAndResetPerfStats() {
        return perf_stats->GetAndResetStats(core_timing.GetGlobalTimeUs());
    }

    mutable std::mutex suspend_guard;
    std::atomic_bool is_paused{};
    std::atomic<bool> is_shutting_down{};

    Timing::CoreTiming core_timing;
    Kernel::KernelCore kernel;
    /// RealVfsFilesystem instance
    FileSys::VirtualFilesystem virtual_filesystem;
    /// ContentProviderUnion instance
    std::unique_ptr<FileSys::ContentProviderUnion> content_provider;
    Service::FileSystem::FileSystemController fs_controller;
    /// AppLoader used to load the current executing application
    std::unique_ptr<Loader::AppLoader> app_loader;
    std::unique_ptr<Tegra::GPU> gpu_core;
    std::unique_ptr<Tegra::Host1x::Host1x> host1x_core;
    std::unique_ptr<Core::DeviceMemory> device_memory;
    std::unique_ptr<AudioCore::AudioCore> audio_core;
    Core::HID::HIDCore hid_core;
    Network::RoomNetwork room_network;

    CpuManager cpu_manager;
    std::atomic_bool is_powered_on{};
    bool exit_locked = false;
    bool exit_requested = false;

    bool nvdec_active{};

    Reporter reporter;
    std::unique_ptr<Memory::CheatEngine> cheat_engine;
    std::unique_ptr<Tools::Freezer> memory_freezer;
    std::array<u8, 0x20> build_id{};

    std::unique_ptr<Tools::RenderdocAPI> renderdoc_api;

    /// Applets
    Service::AM::AppletManager applet_manager;
    Service::AM::Frontend::FrontendAppletHolder frontend_applets;

    /// APM (Performance) services
    Service::APM::Controller apm_controller{core_timing};

    /// Service State
    Service::Glue::ARPManager arp_manager;
    Service::Account::ProfileManager profile_manager;

    /// Service manager
    std::shared_ptr<Service::SM::ServiceManager> service_manager;

    /// Services
    std::unique_ptr<Service::Services> services;

    /// Telemetry session for this emulation session
    std::unique_ptr<Core::TelemetrySession> telemetry_session;

    /// Network instance
    Network::NetworkInstance network_instance;

    /// Debugger
    std::unique_ptr<Core::Debugger> debugger;

    SystemResultStatus status = SystemResultStatus::Success;
    std::string status_details = "";

    std::unique_ptr<Core::PerfStats> perf_stats;
    Core::SpeedLimiter speed_limiter;

    bool is_multicore{};
    bool is_async_gpu{};
    bool extended_memory_layout{};

    ExecuteProgramCallback execute_program_callback;
    ExitCallback exit_callback;
    std::stop_source stop_event;

    std::array<u64, Core::Hardware::NUM_CPU_CORES> dynarmic_ticks{};
    std::array<MicroProfileToken, Core::Hardware::NUM_CPU_CORES> microprofile_cpu{};

    std::array<Core::GPUDirtyMemoryManager, Core::Hardware::NUM_CPU_CORES>
        gpu_dirty_memory_managers;

    std::deque<std::vector<u8>> user_channel;
};

System::System() : impl{std::make_unique<Impl>(*this)} {}

System::~System() = default;

CpuManager& System::GetCpuManager() {
    return impl->cpu_manager;
}

const CpuManager& System::GetCpuManager() const {
    return impl->cpu_manager;
}

void System::Initialize() {
    impl->Initialize(*this);
}

void System::Run() {
    impl->Run();
}

void System::Pause() {
    impl->Pause();
}

bool System::IsPaused() const {
    return impl->IsPaused();
}

void System::ShutdownMainProcess() {
    impl->ShutdownMainProcess();
}

bool System::IsShuttingDown() const {
    return impl->IsShuttingDown();
}

void System::SetShuttingDown(bool shutting_down) {
    impl->SetShuttingDown(shutting_down);
}

void System::DetachDebugger() {
    if (impl->debugger) {
        impl->debugger->NotifyShutdown();
    }
}

std::unique_lock<std::mutex> System::StallApplication() {
    return impl->StallApplication();
}

void System::UnstallApplication() {
    impl->UnstallApplication();
}

void System::SetNVDECActive(bool is_nvdec_active) {
    impl->SetNVDECActive(is_nvdec_active);
}

bool System::GetNVDECActive() {
    return impl->GetNVDECActive();
}

void System::InitializeDebugger() {
    impl->InitializeDebugger(*this, Settings::values.gdbstub_port.GetValue());
}

SystemResultStatus System::Load(Frontend::EmuWindow& emu_window, const std::string& filepath,
                                Service::AM::FrontendAppletParameters& params) {
    return impl->Load(*this, emu_window, filepath, params);
}

bool System::IsPoweredOn() const {
    return impl->is_powered_on.load(std::memory_order::relaxed);
}

void System::PrepareReschedule(const u32 core_index) {
    impl->kernel.PrepareReschedule(core_index);
}

size_t System::GetCurrentHostThreadID() const {
    return impl->kernel.GetCurrentHostThreadID();
}

std::span<GPUDirtyMemoryManager> System::GetGPUDirtyMemoryManager() {
    return impl->gpu_dirty_memory_managers;
}

void System::GatherGPUDirtyMemory(std::function<void(PAddr, size_t)>& callback) {
    for (auto& manager : impl->gpu_dirty_memory_managers) {
        manager.Gather(callback);
    }
}

PerfStatsResults System::GetAndResetPerfStats() {
    return impl->GetAndResetPerfStats();
}

TelemetrySession& System::TelemetrySession() {
    return *impl->telemetry_session;
}

const TelemetrySession& System::TelemetrySession() const {
    return *impl->telemetry_session;
}

Kernel::PhysicalCore& System::CurrentPhysicalCore() {
    return impl->kernel.CurrentPhysicalCore();
}

const Kernel::PhysicalCore& System::CurrentPhysicalCore() const {
    return impl->kernel.CurrentPhysicalCore();
}

/// Gets the global scheduler
Kernel::GlobalSchedulerContext& System::GlobalSchedulerContext() {
    return impl->kernel.GlobalSchedulerContext();
}

/// Gets the global scheduler
const Kernel::GlobalSchedulerContext& System::GlobalSchedulerContext() const {
    return impl->kernel.GlobalSchedulerContext();
}

Kernel::KProcess* System::ApplicationProcess() {
    return impl->kernel.ApplicationProcess();
}

Core::DeviceMemory& System::DeviceMemory() {
    return *impl->device_memory;
}

const Core::DeviceMemory& System::DeviceMemory() const {
    return *impl->device_memory;
}

const Kernel::KProcess* System::ApplicationProcess() const {
    return impl->kernel.ApplicationProcess();
}

Memory::Memory& System::ApplicationMemory() {
    return impl->kernel.ApplicationProcess()->GetMemory();
}

const Core::Memory::Memory& System::ApplicationMemory() const {
    return impl->kernel.ApplicationProcess()->GetMemory();
}

Tegra::GPU& System::GPU() {
    return *impl->gpu_core;
}

const Tegra::GPU& System::GPU() const {
    return *impl->gpu_core;
}

Tegra::Host1x::Host1x& System::Host1x() {
    return *impl->host1x_core;
}

const Tegra::Host1x::Host1x& System::Host1x() const {
    return *impl->host1x_core;
}

VideoCore::RendererBase& System::Renderer() {
    return impl->gpu_core->Renderer();
}

const VideoCore::RendererBase& System::Renderer() const {
    return impl->gpu_core->Renderer();
}

Kernel::KernelCore& System::Kernel() {
    return impl->kernel;
}

const Kernel::KernelCore& System::Kernel() const {
    return impl->kernel;
}

HID::HIDCore& System::HIDCore() {
    return impl->hid_core;
}

const HID::HIDCore& System::HIDCore() const {
    return impl->hid_core;
}

AudioCore::AudioCore& System::AudioCore() {
    return *impl->audio_core;
}

const AudioCore::AudioCore& System::AudioCore() const {
    return *impl->audio_core;
}

Timing::CoreTiming& System::CoreTiming() {
    return impl->core_timing;
}

const Timing::CoreTiming& System::CoreTiming() const {
    return impl->core_timing;
}

Core::PerfStats& System::GetPerfStats() {
    return *impl->perf_stats;
}

const Core::PerfStats& System::GetPerfStats() const {
    return *impl->perf_stats;
}

Core::SpeedLimiter& System::SpeedLimiter() {
    return impl->speed_limiter;
}

const Core::SpeedLimiter& System::SpeedLimiter() const {
    return impl->speed_limiter;
}

u64 System::GetApplicationProcessProgramID() const {
    return impl->kernel.ApplicationProcess()->GetProgramId();
}

Loader::ResultStatus System::GetGameName(std::string& out) const {
    return impl->GetGameName(out);
}

void System::SetStatus(SystemResultStatus new_status, const char* details) {
    impl->SetStatus(new_status, details);
}

const std::string& System::GetStatusDetails() const {
    return impl->status_details;
}

Loader::AppLoader& System::GetAppLoader() {
    return *impl->app_loader;
}

const Loader::AppLoader& System::GetAppLoader() const {
    return *impl->app_loader;
}

void System::SetFilesystem(FileSys::VirtualFilesystem vfs) {
    impl->virtual_filesystem = std::move(vfs);
}

FileSys::VirtualFilesystem System::GetFilesystem() const {
    return impl->virtual_filesystem;
}

void System::RegisterCheatList(const std::vector<Memory::CheatEntry>& list,
                               const std::array<u8, 32>& build_id, u64 main_region_begin,
                               u64 main_region_size) {
    impl->cheat_engine = std::make_unique<Memory::CheatEngine>(*this, list, build_id);
    impl->cheat_engine->SetMainMemoryParameters(main_region_begin, main_region_size);
}

void System::SetFrontendAppletSet(Service::AM::Frontend::FrontendAppletSet&& set) {
    impl->frontend_applets.SetFrontendAppletSet(std::move(set));
}

Service::AM::Frontend::FrontendAppletHolder& System::GetFrontendAppletHolder() {
    return impl->frontend_applets;
}

const Service::AM::Frontend::FrontendAppletHolder& System::GetFrontendAppletHolder() const {
    return impl->frontend_applets;
}

Service::AM::AppletManager& System::GetAppletManager() {
    return impl->applet_manager;
}

void System::SetContentProvider(std::unique_ptr<FileSys::ContentProviderUnion> provider) {
    impl->content_provider = std::move(provider);
}

FileSys::ContentProvider& System::GetContentProvider() {
    return *impl->content_provider;
}

const FileSys::ContentProvider& System::GetContentProvider() const {
    return *impl->content_provider;
}

FileSys::ContentProviderUnion& System::GetContentProviderUnion() {
    return *impl->content_provider;
}

const FileSys::ContentProviderUnion& System::GetContentProviderUnion() const {
    return *impl->content_provider;
}

Service::FileSystem::FileSystemController& System::GetFileSystemController() {
    return impl->fs_controller;
}

const Service::FileSystem::FileSystemController& System::GetFileSystemController() const {
    return impl->fs_controller;
}

void System::RegisterContentProvider(FileSys::ContentProviderUnionSlot slot,
                                     FileSys::ContentProvider* provider) {
    impl->content_provider->SetSlot(slot, provider);
}

void System::ClearContentProvider(FileSys::ContentProviderUnionSlot slot) {
    impl->content_provider->ClearSlot(slot);
}

const Reporter& System::GetReporter() const {
    return impl->reporter;
}

Service::Glue::ARPManager& System::GetARPManager() {
    return impl->arp_manager;
}

const Service::Glue::ARPManager& System::GetARPManager() const {
    return impl->arp_manager;
}

Service::APM::Controller& System::GetAPMController() {
    return impl->apm_controller;
}

const Service::APM::Controller& System::GetAPMController() const {
    return impl->apm_controller;
}

Service::Account::ProfileManager& System::GetProfileManager() {
    return impl->profile_manager;
}

const Service::Account::ProfileManager& System::GetProfileManager() const {
    return impl->profile_manager;
}

void System::SetExitLocked(bool locked) {
    impl->exit_locked = locked;
}

bool System::GetExitLocked() const {
    return impl->exit_locked;
}

void System::SetExitRequested(bool requested) {
    impl->exit_requested = requested;
}

bool System::GetExitRequested() const {
    return impl->exit_requested;
}

void System::SetApplicationProcessBuildID(const CurrentBuildProcessID& id) {
    impl->build_id = id;
}

const System::CurrentBuildProcessID& System::GetApplicationProcessBuildID() const {
    return impl->build_id;
}

Service::SM::ServiceManager& System::ServiceManager() {
    return *impl->service_manager;
}

const Service::SM::ServiceManager& System::ServiceManager() const {
    return *impl->service_manager;
}

void System::RegisterCoreThread(std::size_t id) {
    impl->kernel.RegisterCoreThread(id);
}

void System::RegisterHostThread() {
    impl->kernel.RegisterHostThread();
}

void System::EnterCPUProfile() {
#if MICROPROFILE_ENABLED == 1
    std::size_t core = impl->kernel.GetCurrentHostThreadID();
    impl->dynarmic_ticks[core] = MicroProfileEnter(impl->microprofile_cpu[core]);
#endif
}

void System::ExitCPUProfile() {
#if MICROPROFILE_ENABLED == 1
    std::size_t core = impl->kernel.GetCurrentHostThreadID();
    MicroProfileLeave(impl->microprofile_cpu[core], impl->dynarmic_ticks[core]);
#endif
}

bool System::IsMulticore() const {
    return impl->is_multicore;
}

bool System::DebuggerEnabled() const {
    return Settings::values.use_gdbstub.GetValue();
}

Core::Debugger& System::GetDebugger() {
    return *impl->debugger;
}

const Core::Debugger& System::GetDebugger() const {
    return *impl->debugger;
}

Network::RoomNetwork& System::GetRoomNetwork() {
    return impl->room_network;
}

const Network::RoomNetwork& System::GetRoomNetwork() const {
    return impl->room_network;
}

Tools::RenderdocAPI& System::GetRenderdocAPI() {
    return *impl->renderdoc_api;
}

void System::RunServer(std::unique_ptr<Service::ServerManager>&& server_manager) {
    return impl->kernel.RunServer(std::move(server_manager));
}

void System::RegisterExecuteProgramCallback(ExecuteProgramCallback&& callback) {
    impl->execute_program_callback = std::move(callback);
}

void System::ExecuteProgram(std::size_t program_index) {
    if (impl->execute_program_callback) {
        impl->execute_program_callback(program_index);
    } else {
        LOG_CRITICAL(Core, "execute_program_callback must be initialized by the frontend");
    }
}

std::deque<std::vector<u8>>& System::GetUserChannel() {
    return impl->user_channel;
}

void System::RegisterExitCallback(ExitCallback&& callback) {
    impl->exit_callback = std::move(callback);
}

void System::Exit() {
    if (impl->exit_callback) {
        impl->exit_callback();
    } else {
        LOG_CRITICAL(Core, "exit_callback must be initialized by the frontend");
    }
}

void System::ApplySettings() {
    impl->RefreshTime(*this);

    if (IsPoweredOn()) {
        Renderer().RefreshBaseSettings();
    }
}

} // namespace Core
