// SPDX-FileCopyrightText: Copyright 2023 sudachi Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <codecvt>
#include <locale>
#include <string>
#include <string_view>
#include <dlfcn.h>

#ifdef ARCHITECTURE_arm64
#include <adrenotools/driver.h>
#endif

#include <android/api-level.h>
#include <android/native_window_jni.h>
#include <common/fs/fs.h>
#include <core/file_sys/patch_manager.h>
#include <core/file_sys/savedata_factory.h>
#include <core/loader/nro.h>
#include <frontend_common/content_manager.h>
#include <jni.h>

#include "common/android/android_common.h"
#include "common/android/id_cache.h"
#include "common/detached_tasks.h"
#include "common/dynamic_library.h"
#include "common/fs/path_util.h"
#include "common/logging/backend.h"
#include "common/logging/log.h"
#include "common/scm_rev.h"
#include "common/scope_exit.h"
#include "common/settings.h"
#include "common/string_util.h"
#include "core/core.h"
#include "core/cpu_manager.h"
#include "core/crypto/key_manager.h"
#include "core/file_sys/card_image.h"
#include "core/file_sys/content_archive.h"
#include "core/file_sys/fs_filesystem.h"
#include "core/file_sys/submission_package.h"
#include "core/file_sys/vfs/vfs.h"
#include "core/file_sys/vfs/vfs_real.h"
#include "core/frontend/applets/cabinet.h"
#include "core/frontend/applets/controller.h"
#include "core/frontend/applets/error.h"
#include "core/frontend/applets/general.h"
#include "core/frontend/applets/mii_edit.h"
#include "core/frontend/applets/profile_select.h"
#include "core/frontend/applets/software_keyboard.h"
#include "core/frontend/applets/web_browser.h"
#include "core/hle/service/am/applet_manager.h"
#include "core/hle/service/am/frontend/applets.h"
#include "core/hle/service/filesystem/filesystem.h"
#include "core/loader/loader.h"
#include "frontend_common/config.h"
#include "hid_core/frontend/emulated_controller.h"
#include "hid_core/hid_core.h"
#include "hid_core/hid_types.h"
#include "jni/native.h"
#include "video_core/renderer_base.h"
#include "video_core/renderer_vulkan/renderer_vulkan.h"
#include "video_core/vulkan_common/vulkan_instance.h"
#include "video_core/vulkan_common/vulkan_surface.h"

#define jconst [[maybe_unused]] const auto
#define jauto [[maybe_unused]] auto

static EmulationSession s_instance;

EmulationSession::EmulationSession() {
    m_vfs = std::make_shared<FileSys::RealVfsFilesystem>();
}

EmulationSession& EmulationSession::GetInstance() {
    return s_instance;
}

const Core::System& EmulationSession::System() const {
    return m_system;
}

Core::System& EmulationSession::System() {
    return m_system;
}

FileSys::ManualContentProvider* EmulationSession::GetContentProvider() {
    return m_manual_provider.get();
}

InputCommon::InputSubsystem& EmulationSession::GetInputSubsystem() {
    return m_input_subsystem;
}

const EmuWindow_Android& EmulationSession::Window() const {
    return *m_window;
}

EmuWindow_Android& EmulationSession::Window() {
    return *m_window;
}

ANativeWindow* EmulationSession::NativeWindow() const {
    return m_native_window;
}

void EmulationSession::SetNativeWindow(ANativeWindow* native_window) {
    m_native_window = native_window;
}

void EmulationSession::InitializeGpuDriver(const std::string& hook_lib_dir,
                                           const std::string& custom_driver_dir,
                                           const std::string& custom_driver_name,
                                           const std::string& file_redirect_dir) {
#ifdef ARCHITECTURE_arm64
    void* handle{};
    const char* file_redirect_dir_{};
    int featureFlags{};

    // Enable driver file redirection when renderer debugging is enabled.
    if (Settings::values.renderer_debug && file_redirect_dir.size()) {
        featureFlags |= ADRENOTOOLS_DRIVER_FILE_REDIRECT;
        file_redirect_dir_ = file_redirect_dir.c_str();
    }

    // Try to load a custom driver.
    if (custom_driver_name.size()) {
        handle = adrenotools_open_libvulkan(
            RTLD_NOW, featureFlags | ADRENOTOOLS_DRIVER_CUSTOM, nullptr, hook_lib_dir.c_str(),
            custom_driver_dir.c_str(), custom_driver_name.c_str(), file_redirect_dir_, nullptr);
    }

    // Try to load the system driver.
    if (!handle) {
        handle = adrenotools_open_libvulkan(RTLD_NOW, featureFlags, nullptr, hook_lib_dir.c_str(),
                                            nullptr, nullptr, file_redirect_dir_, nullptr);
    }

    m_vulkan_library = std::make_shared<Common::DynamicLibrary>(handle);
#endif
}

bool EmulationSession::IsRunning() const {
    return m_is_running;
}

bool EmulationSession::IsPaused() const {
    return m_is_running && m_is_paused;
}

const Core::PerfStatsResults& EmulationSession::PerfStats() {
    m_perf_stats = m_system.GetAndResetPerfStats();
    return m_perf_stats;
}

void EmulationSession::SurfaceChanged() {
    if (!IsRunning()) {
        return;
    }
    m_window->OnSurfaceChanged(m_native_window);
}

void EmulationSession::ConfigureFilesystemProvider(const std::string& filepath) {
    const auto file = m_system.GetFilesystem()->OpenFile(filepath, FileSys::OpenMode::Read);
    if (!file) {
        return;
    }

    auto loader = Loader::GetLoader(m_system, file);
    if (!loader) {
        return;
    }

    const auto file_type = loader->GetFileType();
    if (file_type == Loader::FileType::Unknown || file_type == Loader::FileType::Error) {
        return;
    }

    u64 program_id = 0;
    const auto res2 = loader->ReadProgramId(program_id);
    if (res2 == Loader::ResultStatus::Success && file_type == Loader::FileType::NCA) {
        m_manual_provider->AddEntry(FileSys::TitleType::Application,
                                    FileSys::GetCRTypeFromNCAType(FileSys::NCA{file}.GetType()),
                                    program_id, file);
    } else if (res2 == Loader::ResultStatus::Success &&
               (file_type == Loader::FileType::XCI || file_type == Loader::FileType::NSP)) {
        const auto nsp = file_type == Loader::FileType::NSP
                             ? std::make_shared<FileSys::NSP>(file)
                             : FileSys::XCI{file}.GetSecurePartitionNSP();
        for (const auto& title : nsp->GetNCAs()) {
            for (const auto& entry : title.second) {
                m_manual_provider->AddEntry(entry.first.first, entry.first.second, title.first,
                                            entry.second->GetBaseFile());
            }
        }
    }
}

void EmulationSession::InitializeSystem(bool reload) {
    if (!reload) {
        // Initialize logging system
        Common::Log::Initialize();
        Common::Log::SetColorConsoleBackendEnabled(true);
        Common::Log::Start();

        m_input_subsystem.Initialize();
    }

    // Initialize filesystem.
    m_system.SetFilesystem(m_vfs);
    m_system.GetUserChannel().clear();
    m_manual_provider = std::make_unique<FileSys::ManualContentProvider>();
    m_system.SetContentProvider(std::make_unique<FileSys::ContentProviderUnion>());
    m_system.RegisterContentProvider(FileSys::ContentProviderUnionSlot::FrontendManual,
                                     m_manual_provider.get());
    m_system.GetFileSystemController().CreateFactories(*m_vfs);
}

void EmulationSession::SetAppletId(int applet_id) {
    m_applet_id = applet_id;
    m_system.GetFrontendAppletHolder().SetCurrentAppletId(
        static_cast<Service::AM::AppletId>(m_applet_id));
}

Core::SystemResultStatus EmulationSession::InitializeEmulation(const std::string& filepath,
                                                               const std::size_t program_index,
                                                               const bool frontend_initiated) {
    std::scoped_lock lock(m_mutex);

    // Create the render window.
    m_window = std::make_unique<EmuWindow_Android>(m_native_window, m_vulkan_library);

    // Initialize system.
    jauto android_keyboard = std::make_unique<Common::Android::SoftwareKeyboard::AndroidKeyboard>();
    m_software_keyboard = android_keyboard.get();
    m_system.SetShuttingDown(false);
    m_system.ApplySettings();
    Settings::LogSettings();
    m_system.HIDCore().ReloadInputDevices();
    m_system.SetFrontendAppletSet({
        nullptr,                     // Amiibo Settings
        nullptr,                     // Controller Selector
        nullptr,                     // Error Display
        nullptr,                     // Mii Editor
        nullptr,                     // Parental Controls
        nullptr,                     // Photo Viewer
        nullptr,                     // Profile Selector
        std::move(android_keyboard), // Software Keyboard
        nullptr,                     // Web Browser
    });

    // Initialize filesystem.
    ConfigureFilesystemProvider(filepath);

    // Load the ROM.
    Service::AM::FrontendAppletParameters params{
        .applet_id = static_cast<Service::AM::AppletId>(m_applet_id),
        .launch_type = frontend_initiated ? Service::AM::LaunchType::FrontendInitiated
                                          : Service::AM::LaunchType::ApplicationInitiated,
        .program_index = static_cast<s32>(program_index),
    };
    m_load_result = m_system.Load(EmulationSession::GetInstance().Window(), filepath, params);
    if (m_load_result != Core::SystemResultStatus::Success) {
        return m_load_result;
    }

    // Complete initialization.
    m_system.GPU().Start();
    m_system.GetCpuManager().OnGpuReady();
    m_system.RegisterExitCallback([&] { HaltEmulation(); });

    // Register an ExecuteProgram callback such that Core can execute a sub-program
    m_system.RegisterExecuteProgramCallback([&](std::size_t program_index_) {
        m_next_program_index = program_index_;
        EmulationSession::GetInstance().HaltEmulation();
    });

    OnEmulationStarted();
    return Core::SystemResultStatus::Success;
}

void EmulationSession::ShutdownEmulation() {
    std::scoped_lock lock(m_mutex);

    if (m_next_program_index != -1) {
        ChangeProgram(m_next_program_index);
        m_next_program_index = -1;
    }

    m_is_running = false;

    // Unload user input.
    m_system.HIDCore().UnloadInputDevices();

    // Enable all controllers
    m_system.HIDCore().SetSupportedStyleTag({Core::HID::NpadStyleSet::All});

    // Shutdown the main emulated process
    if (m_load_result == Core::SystemResultStatus::Success) {
        m_system.DetachDebugger();
        m_system.ShutdownMainProcess();
        m_detached_tasks.WaitForAllTasks();
        m_load_result = Core::SystemResultStatus::ErrorNotInitialized;
        m_window.reset();
        OnEmulationStopped(Core::SystemResultStatus::Success);
        return;
    }

    // Tear down the render window.
    m_window.reset();
}

void EmulationSession::PauseEmulation() {
    std::scoped_lock lock(m_mutex);
    m_system.Pause();
    m_is_paused = true;
}

void EmulationSession::UnPauseEmulation() {
    std::scoped_lock lock(m_mutex);
    m_system.Run();
    m_is_paused = false;
}

void EmulationSession::HaltEmulation() {
    std::scoped_lock lock(m_mutex);
    m_is_running = false;
    m_cv.notify_one();
}

void EmulationSession::RunEmulation() {
    {
        std::scoped_lock lock(m_mutex);
        m_is_running = true;
    }

    // Load the disk shader cache.
    if (Settings::values.use_disk_shader_cache.GetValue()) {
        LoadDiskCacheProgress(VideoCore::LoadCallbackStage::Prepare, 0, 0);
        m_system.Renderer().ReadRasterizer()->LoadDiskResources(
            m_system.GetApplicationProcessProgramID(), std::stop_token{}, LoadDiskCacheProgress);
        LoadDiskCacheProgress(VideoCore::LoadCallbackStage::Complete, 0, 0);
    }

    void(m_system.Run());

    if (m_system.DebuggerEnabled()) {
        m_system.InitializeDebugger();
    }

    while (true) {
        {
            [[maybe_unused]] std::unique_lock lock(m_mutex);
            if (m_cv.wait_for(lock, std::chrono::milliseconds(800),
                              [&]() { return !m_is_running; })) {
                // Emulation halted.
                break;
            }
        }
    }

    // Reset current applet ID.
    m_applet_id = static_cast<int>(Service::AM::AppletId::Application);
}

Common::Android::SoftwareKeyboard::AndroidKeyboard* EmulationSession::SoftwareKeyboard() {
    return m_software_keyboard;
}

void EmulationSession::LoadDiskCacheProgress(VideoCore::LoadCallbackStage stage, int progress,
                                             int max) {
    JNIEnv* env = Common::Android::GetEnvForThread();
    env->CallStaticVoidMethod(Common::Android::GetDiskCacheProgressClass(),
                              Common::Android::GetDiskCacheLoadProgress(), static_cast<jint>(stage),
                              static_cast<jint>(progress), static_cast<jint>(max));
}

void EmulationSession::OnEmulationStarted() {
    JNIEnv* env = Common::Android::GetEnvForThread();
    env->CallStaticVoidMethod(Common::Android::GetNativeLibraryClass(),
                              Common::Android::GetOnEmulationStarted());
}

void EmulationSession::OnEmulationStopped(Core::SystemResultStatus result) {
    JNIEnv* env = Common::Android::GetEnvForThread();
    env->CallStaticVoidMethod(Common::Android::GetNativeLibraryClass(),
                              Common::Android::GetOnEmulationStopped(), static_cast<jint>(result));
}

void EmulationSession::ChangeProgram(std::size_t program_index) {
    JNIEnv* env = Common::Android::GetEnvForThread();
    env->CallStaticVoidMethod(Common::Android::GetNativeLibraryClass(),
                              Common::Android::GetOnProgramChanged(),
                              static_cast<jint>(program_index));
}

u64 EmulationSession::GetProgramId(JNIEnv* env, jstring jprogramId) {
    auto program_id_string = Common::Android::GetJString(env, jprogramId);
    try {
        return std::stoull(program_id_string);
    } catch (...) {
        return 0;
    }
}

static Core::SystemResultStatus RunEmulation(const std::string& filepath,
                                             const size_t program_index,
                                             const bool frontend_initiated) {
    LOG_INFO(Frontend, "starting");

    if (filepath.empty()) {
        LOG_CRITICAL(Frontend, "failed to load: filepath empty!");
        return Core::SystemResultStatus::ErrorLoader;
    }

    SCOPE_EXIT {
        EmulationSession::GetInstance().ShutdownEmulation();
    };

    jconst result = EmulationSession::GetInstance().InitializeEmulation(filepath, program_index,
                                                                        frontend_initiated);
    if (result != Core::SystemResultStatus::Success) {
        return result;
    }

    EmulationSession::GetInstance().RunEmulation();

    return Core::SystemResultStatus::Success;
}

extern "C" {

void Java_org_sudachi_sudachi_1emu_NativeLibrary_surfaceChanged(JNIEnv* env, jobject instance,
                                                                [[maybe_unused]] jobject surf) {
    EmulationSession::GetInstance().SetNativeWindow(ANativeWindow_fromSurface(env, surf));
    EmulationSession::GetInstance().SurfaceChanged();
}

void Java_org_sudachi_sudachi_1emu_NativeLibrary_surfaceDestroyed(JNIEnv* env, jobject instance) {
    ANativeWindow_release(EmulationSession::GetInstance().NativeWindow());
    EmulationSession::GetInstance().SetNativeWindow(nullptr);
    EmulationSession::GetInstance().SurfaceChanged();
}

void Java_org_sudachi_sudachi_1emu_NativeLibrary_setAppDirectory(
    JNIEnv* env, jobject instance, [[maybe_unused]] jstring j_directory) {
    Common::FS::SetAppDirectory(Common::Android::GetJString(env, j_directory));
}

int Java_org_sudachi_sudachi_1emu_NativeLibrary_installFileToNand(JNIEnv* env, jobject instance,
                                                                  jstring j_file,
                                                                  jobject jcallback) {
    auto jlambdaClass = env->GetObjectClass(jcallback);
    auto jlambdaInvokeMethod = env->GetMethodID(
        jlambdaClass, "invoke", "(Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;");
    const auto callback = [env, jcallback, jlambdaInvokeMethod](size_t max, size_t progress) {
        auto jwasCancelled = env->CallObjectMethod(jcallback, jlambdaInvokeMethod,
                                                   Common::Android::ToJDouble(env, max),
                                                   Common::Android::ToJDouble(env, progress));
        return Common::Android::GetJBoolean(env, jwasCancelled);
    };

    return static_cast<int>(
        ContentManager::InstallNSP(EmulationSession::GetInstance().System(),
                                   *EmulationSession::GetInstance().System().GetFilesystem(),
                                   Common::Android::GetJString(env, j_file), callback));
}

jboolean Java_org_sudachi_sudachi_1emu_NativeLibrary_doesUpdateMatchProgram(JNIEnv* env,
                                                                            jobject jobj,
                                                                            jstring jprogramId,
                                                                            jstring jupdatePath) {
    u64 program_id = EmulationSession::GetProgramId(env, jprogramId);
    std::string updatePath = Common::Android::GetJString(env, jupdatePath);
    std::shared_ptr<FileSys::NSP> nsp = std::make_shared<FileSys::NSP>(
        EmulationSession::GetInstance().System().GetFilesystem()->OpenFile(
            updatePath, FileSys::OpenMode::Read));
    for (const auto& item : nsp->GetNCAs()) {
        for (const auto& nca_details : item.second) {
            if (nca_details.second->GetName().ends_with(".cnmt.nca")) {
                auto update_id = nca_details.second->GetTitleId() & ~0xFFFULL;
                if (update_id == program_id) {
                    return true;
                }
            }
        }
    }
    return false;
}

void JNICALL Java_org_sudachi_sudachi_1emu_NativeLibrary_initializeGpuDriver(
    JNIEnv* env, jclass clazz, jstring hook_lib_dir, jstring custom_driver_dir,
    jstring custom_driver_name, jstring file_redirect_dir) {
    EmulationSession::GetInstance().InitializeGpuDriver(
        Common::Android::GetJString(env, hook_lib_dir),
        Common::Android::GetJString(env, custom_driver_dir),
        Common::Android::GetJString(env, custom_driver_name),
        Common::Android::GetJString(env, file_redirect_dir));
}

[[maybe_unused]] static bool CheckKgslPresent() {
    constexpr auto KgslPath{"/dev/kgsl-3d0"};

    return access(KgslPath, F_OK) == 0;
}

[[maybe_unused]] bool SupportsCustomDriver() {
    return android_get_device_api_level() >= 28 && CheckKgslPresent();
}

jboolean JNICALL Java_org_sudachi_sudachi_1emu_utils_GpuDriverHelper_supportsCustomDriverLoading(
    JNIEnv* env, jobject instance) {
#ifdef ARCHITECTURE_arm64
    // If the KGSL device exists custom drivers can be loaded using adrenotools
    return SupportsCustomDriver();
#else
    return false;
#endif
}

jobjectArray Java_org_sudachi_sudachi_1emu_utils_GpuDriverHelper_getSystemDriverInfo(
    JNIEnv* env, jobject j_obj, jobject j_surf, jstring j_hook_lib_dir) {
    const char* file_redirect_dir_{};
    int featureFlags{};
    std::string hook_lib_dir = Common::Android::GetJString(env, j_hook_lib_dir);
    auto handle = adrenotools_open_libvulkan(RTLD_NOW, featureFlags, nullptr, hook_lib_dir.c_str(),
                                             nullptr, nullptr, file_redirect_dir_, nullptr);
    auto driver_library = std::make_shared<Common::DynamicLibrary>(handle);
    InputCommon::InputSubsystem input_subsystem;
    auto window =
        std::make_unique<EmuWindow_Android>(ANativeWindow_fromSurface(env, j_surf), driver_library);

    Vulkan::vk::InstanceDispatch dld;
    Vulkan::vk::Instance vk_instance = Vulkan::CreateInstance(
        *driver_library, dld, VK_API_VERSION_1_1, Core::Frontend::WindowSystemType::Android);

    auto surface = Vulkan::CreateSurface(vk_instance, window->GetWindowInfo());

    auto device = Vulkan::CreateDevice(vk_instance, dld, *surface);

    auto driver_version = device.GetDriverVersion();
    auto version_string =
        fmt::format("{}.{}.{}", VK_API_VERSION_MAJOR(driver_version),
                    VK_API_VERSION_MINOR(driver_version), VK_API_VERSION_PATCH(driver_version));

    jobjectArray j_driver_info = env->NewObjectArray(
        2, Common::Android::GetStringClass(), Common::Android::ToJString(env, version_string));
    env->SetObjectArrayElement(j_driver_info, 1,
                               Common::Android::ToJString(env, device.GetDriverName()));
    return j_driver_info;
}

jboolean Java_org_sudachi_sudachi_1emu_NativeLibrary_reloadKeys(JNIEnv* env, jclass clazz) {
    Core::Crypto::KeyManager::Instance().ReloadKeys();
    return static_cast<jboolean>(Core::Crypto::KeyManager::Instance().AreKeysLoaded());
}

void Java_org_sudachi_sudachi_1emu_NativeLibrary_unpauseEmulation(JNIEnv* env, jclass clazz) {
    EmulationSession::GetInstance().UnPauseEmulation();
}

void Java_org_sudachi_sudachi_1emu_NativeLibrary_pauseEmulation(JNIEnv* env, jclass clazz) {
    EmulationSession::GetInstance().PauseEmulation();
}

void Java_org_sudachi_sudachi_1emu_NativeLibrary_stopEmulation(JNIEnv* env, jclass clazz) {
    EmulationSession::GetInstance().HaltEmulation();
}

jboolean Java_org_sudachi_sudachi_1emu_NativeLibrary_isRunning(JNIEnv* env, jclass clazz) {
    return static_cast<jboolean>(EmulationSession::GetInstance().IsRunning());
}

jboolean Java_org_sudachi_sudachi_1emu_NativeLibrary_isPaused(JNIEnv* env, jclass clazz) {
    return static_cast<jboolean>(EmulationSession::GetInstance().IsPaused());
}

void Java_org_sudachi_sudachi_1emu_NativeLibrary_initializeSystem(JNIEnv* env, jclass clazz,
                                                                  jboolean reload) {
    // Initialize the emulated system.
    if (!reload) {
        EmulationSession::GetInstance().System().Initialize();
    }
    EmulationSession::GetInstance().InitializeSystem(reload);
}

jdoubleArray Java_org_sudachi_sudachi_1emu_NativeLibrary_getPerfStats(JNIEnv* env, jclass clazz) {
    jdoubleArray j_stats = env->NewDoubleArray(4);

    if (EmulationSession::GetInstance().IsRunning()) {
        jconst results = EmulationSession::GetInstance().PerfStats();

        // Converting the structure into an array makes it easier to pass it to the frontend
        double stats[4] = {results.system_fps, results.average_game_fps, results.frametime,
                           results.emulation_speed};

        env->SetDoubleArrayRegion(j_stats, 0, 4, stats);
    }

    return j_stats;
}

jstring Java_org_sudachi_sudachi_1emu_NativeLibrary_getCpuBackend(JNIEnv* env, jclass clazz) {
    if (Settings::IsNceEnabled()) {
        return Common::Android::ToJString(env, "NCE");
    }

    return Common::Android::ToJString(env, "JIT");
}

jstring Java_org_sudachi_sudachi_1emu_NativeLibrary_getGpuDriver(JNIEnv* env, jobject jobj) {
    return Common::Android::ToJString(
        env, EmulationSession::GetInstance().System().GPU().Renderer().GetDeviceVendor());
}

void Java_org_sudachi_sudachi_1emu_NativeLibrary_applySettings(JNIEnv* env, jobject jobj) {
    EmulationSession::GetInstance().System().ApplySettings();
    EmulationSession::GetInstance().System().HIDCore().ReloadInputDevices();
}

void Java_org_sudachi_sudachi_1emu_NativeLibrary_logSettings(JNIEnv* env, jobject jobj) {
    Settings::LogSettings();
}

void Java_org_sudachi_sudachi_1emu_NativeLibrary_run(JNIEnv* env, jobject jobj, jstring j_path,
                                                     jint j_program_index,
                                                     jboolean j_frontend_initiated) {
    const std::string path = Common::Android::GetJString(env, j_path);

    const Core::SystemResultStatus result{
        RunEmulation(path, j_program_index, j_frontend_initiated)};
    if (result != Core::SystemResultStatus::Success) {
        env->CallStaticVoidMethod(Common::Android::GetNativeLibraryClass(),
                                  Common::Android::GetExitEmulationActivity(),
                                  static_cast<int>(result));
    }
}

void Java_org_sudachi_sudachi_1emu_NativeLibrary_logDeviceInfo(JNIEnv* env, jclass clazz) {
    LOG_INFO(Frontend, "sudachi Version: {}-{}", Common::g_scm_branch, Common::g_scm_desc);
    LOG_INFO(Frontend, "Host OS: Android API level {}", android_get_device_api_level());
}

void Java_org_sudachi_sudachi_1emu_NativeLibrary_submitInlineKeyboardText(JNIEnv* env, jclass clazz,
                                                                          jstring j_text) {
    const std::u16string input = Common::UTF8ToUTF16(Common::Android::GetJString(env, j_text));
    EmulationSession::GetInstance().SoftwareKeyboard()->SubmitInlineKeyboardText(input);
}

void Java_org_sudachi_sudachi_1emu_NativeLibrary_submitInlineKeyboardInput(JNIEnv* env,
                                                                           jclass clazz,
                                                                           jint j_key_code) {
    EmulationSession::GetInstance().SoftwareKeyboard()->SubmitInlineKeyboardInput(j_key_code);
}

void Java_org_sudachi_sudachi_1emu_NativeLibrary_initializeEmptyUserDirectory(JNIEnv* env,
                                                                              jobject instance) {
    const auto nand_dir = Common::FS::GetSudachiPath(Common::FS::SudachiPath::NANDDir);
    auto vfs_nand_dir = EmulationSession::GetInstance().System().GetFilesystem()->OpenDirectory(
        Common::FS::PathToUTF8String(nand_dir), FileSys::OpenMode::Read);

    const auto user_id = EmulationSession::GetInstance().System().GetProfileManager().GetUser(
        static_cast<std::size_t>(0));
    ASSERT(user_id);

    const auto user_save_data_path = FileSys::SaveDataFactory::GetFullPath(
        {}, vfs_nand_dir, FileSys::SaveDataSpaceId::User, FileSys::SaveDataType::Account, 1,
        user_id->AsU128(), 0);

    const auto full_path = Common::FS::ConcatPathSafe(nand_dir, user_save_data_path);
    if (!Common::FS::CreateParentDirs(full_path)) {
        LOG_WARNING(Frontend, "Failed to create full path of the default user's save directory");
    }
}

jstring Java_org_sudachi_sudachi_1emu_NativeLibrary_getAppletLaunchPath(JNIEnv* env, jclass clazz,
                                                                        jlong jid) {
    auto bis_system =
        EmulationSession::GetInstance().System().GetFileSystemController().GetSystemNANDContents();
    if (!bis_system) {
        return Common::Android::ToJString(env, "");
    }

    auto applet_nca =
        bis_system->GetEntry(static_cast<u64>(jid), FileSys::ContentRecordType::Program);
    if (!applet_nca) {
        return Common::Android::ToJString(env, "");
    }

    return Common::Android::ToJString(env, applet_nca->GetFullPath());
}

void Java_org_sudachi_sudachi_1emu_NativeLibrary_setCurrentAppletId(JNIEnv* env, jclass clazz,
                                                                    jint jappletId) {
    EmulationSession::GetInstance().SetAppletId(jappletId);
}

void Java_org_sudachi_sudachi_1emu_NativeLibrary_setCabinetMode(JNIEnv* env, jclass clazz,
                                                                jint jcabinetMode) {
    EmulationSession::GetInstance().System().GetFrontendAppletHolder().SetCabinetMode(
        static_cast<Service::NFP::CabinetMode>(jcabinetMode));
}

jboolean Java_org_sudachi_sudachi_1emu_NativeLibrary_isFirmwareAvailable(JNIEnv* env,
                                                                         jclass clazz) {
    auto bis_system =
        EmulationSession::GetInstance().System().GetFileSystemController().GetSystemNANDContents();
    if (!bis_system) {
        return false;
    }

    // Query an applet to see if it's available
    auto applet_nca =
        bis_system->GetEntry(0x010000000000100Dull, FileSys::ContentRecordType::Program);
    if (!applet_nca) {
        return false;
    }
    return true;
}

jobjectArray Java_org_sudachi_sudachi_1emu_NativeLibrary_getPatchesForFile(JNIEnv* env,
                                                                           jobject jobj,
                                                                           jstring jpath,
                                                                           jstring jprogramId) {
    const auto path = Common::Android::GetJString(env, jpath);
    const auto vFile =
        Core::GetGameFileFromPath(EmulationSession::GetInstance().System().GetFilesystem(), path);
    if (vFile == nullptr) {
        return nullptr;
    }

    auto& system = EmulationSession::GetInstance().System();
    auto program_id = EmulationSession::GetProgramId(env, jprogramId);
    const FileSys::PatchManager pm{program_id, system.GetFileSystemController(),
                                   system.GetContentProvider()};
    const auto loader = Loader::GetLoader(system, vFile);

    FileSys::VirtualFile update_raw;
    loader->ReadUpdateRaw(update_raw);

    auto patches = pm.GetPatches(update_raw);
    jobjectArray jpatchArray =
        env->NewObjectArray(patches.size(), Common::Android::GetPatchClass(), nullptr);
    int i = 0;
    for (const auto& patch : patches) {
        jobject jpatch = env->NewObject(
            Common::Android::GetPatchClass(), Common::Android::GetPatchConstructor(), patch.enabled,
            Common::Android::ToJString(env, patch.name),
            Common::Android::ToJString(env, patch.version), static_cast<jint>(patch.type),
            Common::Android::ToJString(env, std::to_string(patch.program_id)),
            Common::Android::ToJString(env, std::to_string(patch.title_id)));
        env->SetObjectArrayElement(jpatchArray, i, jpatch);
        ++i;
    }
    return jpatchArray;
}

void Java_org_sudachi_sudachi_1emu_NativeLibrary_removeUpdate(JNIEnv* env, jobject jobj,
                                                              jstring jprogramId) {
    auto program_id = EmulationSession::GetProgramId(env, jprogramId);
    ContentManager::RemoveUpdate(EmulationSession::GetInstance().System().GetFileSystemController(),
                                 program_id);
}

void Java_org_sudachi_sudachi_1emu_NativeLibrary_removeDLC(JNIEnv* env, jobject jobj,
                                                           jstring jprogramId) {
    auto program_id = EmulationSession::GetProgramId(env, jprogramId);
    ContentManager::RemoveAllDLC(EmulationSession::GetInstance().System(), program_id);
}

void Java_org_sudachi_sudachi_1emu_NativeLibrary_removeMod(JNIEnv* env, jobject jobj,
                                                           jstring jprogramId, jstring jname) {
    auto program_id = EmulationSession::GetProgramId(env, jprogramId);
    ContentManager::RemoveMod(EmulationSession::GetInstance().System().GetFileSystemController(),
                              program_id, Common::Android::GetJString(env, jname));
}

jobjectArray Java_org_sudachi_sudachi_1emu_NativeLibrary_verifyInstalledContents(
    JNIEnv* env, jobject jobj, jobject jcallback) {
    auto jlambdaClass = env->GetObjectClass(jcallback);
    auto jlambdaInvokeMethod = env->GetMethodID(
        jlambdaClass, "invoke", "(Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;");
    const auto callback = [env, jcallback, jlambdaInvokeMethod](size_t max, size_t progress) {
        auto jwasCancelled = env->CallObjectMethod(jcallback, jlambdaInvokeMethod,
                                                   Common::Android::ToJDouble(env, max),
                                                   Common::Android::ToJDouble(env, progress));
        return Common::Android::GetJBoolean(env, jwasCancelled);
    };

    auto& session = EmulationSession::GetInstance();
    std::vector<std::string> result = ContentManager::VerifyInstalledContents(
        session.System(), *session.GetContentProvider(), callback);
    jobjectArray jresult = env->NewObjectArray(result.size(), Common::Android::GetStringClass(),
                                               Common::Android::ToJString(env, ""));
    for (size_t i = 0; i < result.size(); ++i) {
        env->SetObjectArrayElement(jresult, i, Common::Android::ToJString(env, result[i]));
    }
    return jresult;
}

jint Java_org_sudachi_sudachi_1emu_NativeLibrary_verifyGameContents(JNIEnv* env, jobject jobj,
                                                                    jstring jpath,
                                                                    jobject jcallback) {
    auto jlambdaClass = env->GetObjectClass(jcallback);
    auto jlambdaInvokeMethod = env->GetMethodID(
        jlambdaClass, "invoke", "(Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;");
    const auto callback = [env, jcallback, jlambdaInvokeMethod](size_t max, size_t progress) {
        auto jwasCancelled = env->CallObjectMethod(jcallback, jlambdaInvokeMethod,
                                                   Common::Android::ToJDouble(env, max),
                                                   Common::Android::ToJDouble(env, progress));
        return Common::Android::GetJBoolean(env, jwasCancelled);
    };
    auto& session = EmulationSession::GetInstance();
    return static_cast<jint>(ContentManager::VerifyGameContents(
        session.System(), Common::Android::GetJString(env, jpath), callback));
}

jstring Java_org_sudachi_sudachi_1emu_NativeLibrary_getSavePath(JNIEnv* env, jobject jobj,
                                                                jstring jprogramId) {
    auto program_id = EmulationSession::GetProgramId(env, jprogramId);
    if (program_id == 0) {
        return Common::Android::ToJString(env, "");
    }

    auto& system = EmulationSession::GetInstance().System();

    Service::Account::ProfileManager manager;
    // TODO: Pass in a selected user once we get the relevant UI working
    const auto user_id = manager.GetUser(static_cast<std::size_t>(0));
    ASSERT(user_id);

    const auto nandDir = Common::FS::GetSudachiPath(Common::FS::SudachiPath::NANDDir);
    auto vfsNandDir = system.GetFilesystem()->OpenDirectory(Common::FS::PathToUTF8String(nandDir),
                                                            FileSys::OpenMode::Read);

    const auto user_save_data_path = FileSys::SaveDataFactory::GetFullPath(
        {}, vfsNandDir, FileSys::SaveDataSpaceId::User, FileSys::SaveDataType::Account, program_id,
        user_id->AsU128(), 0);
    return Common::Android::ToJString(env, user_save_data_path);
}

jstring Java_org_sudachi_sudachi_1emu_NativeLibrary_getDefaultProfileSaveDataRoot(
    JNIEnv* env, jobject jobj, jboolean jfuture) {
    Service::Account::ProfileManager manager;
    // TODO: Pass in a selected user once we get the relevant UI working
    const auto user_id = manager.GetUser(static_cast<std::size_t>(0));
    ASSERT(user_id);

    const auto user_save_data_root =
        FileSys::SaveDataFactory::GetUserGameSaveDataRoot(user_id->AsU128(), jfuture);
    return Common::Android::ToJString(env, user_save_data_root);
}

void Java_org_sudachi_sudachi_1emu_NativeLibrary_addFileToFilesystemProvider(JNIEnv* env,
                                                                             jobject jobj,
                                                                             jstring jpath) {
    EmulationSession::GetInstance().ConfigureFilesystemProvider(
        Common::Android::GetJString(env, jpath));
}

void Java_org_sudachi_sudachi_1emu_NativeLibrary_clearFilesystemProvider(JNIEnv* env,
                                                                         jobject jobj) {
    EmulationSession::GetInstance().GetContentProvider()->ClearAllEntries();
}

jboolean Java_org_sudachi_sudachi_1emu_NativeLibrary_areKeysPresent(JNIEnv* env, jobject jobj) {
    auto& system = EmulationSession::GetInstance().System();
    system.GetFileSystemController().CreateFactories(*system.GetFilesystem());
    return ContentManager::AreKeysPresent();
}

} // extern "C"
