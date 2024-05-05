// SPDX-FileCopyrightText: Copyright 2022 sudachi Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <chrono>

#include "audio_core/adsp/adsp.h"
#include "audio_core/audio_core.h"
#include "audio_core/renderer/system_manager.h"
#include "common/profiling.h"
#include "common/thread.h"
#include "core/core.h"
#include "core/core_timing.h"

namespace AudioCore::Renderer {

SystemManager::SystemManager(Core::System& core_)
    : core{core_}, audio_renderer{core.AudioCore().ADSP().AudioRenderer()} {}

SystemManager::~SystemManager() {
    Stop();
}

void SystemManager::InitializeUnsafe() {
    if (!active) {
        active = true;
        audio_renderer.Start();
        thread = std::jthread([this](std::stop_token stop_token) { ThreadFunc(stop_token); });
    }
}

void SystemManager::Stop() {
    if (!active) {
        return;
    }
    active = false;
    thread.request_stop();
    thread.join();
    audio_renderer.Stop();
}

bool SystemManager::Add(System& system_) {
    std::scoped_lock l2{mutex2};

    if (systems.size() + 1 > MaxRendererSessions) {
        LOG_ERROR(Service_Audio, "Maximum AudioRenderer Systems active, cannot add more!");
        return false;
    }

    {
        std::scoped_lock l{mutex1};
        if (systems.empty()) {
            InitializeUnsafe();
        }
    }

    systems.push_back(&system_);
    return true;
}

bool SystemManager::Remove(System& system_) {
    std::scoped_lock l2{mutex2};

    {
        std::scoped_lock l{mutex1};
        if (systems.remove(&system_) == 0) {
            LOG_ERROR(Service_Audio,
                      "Failed to remove a render system, it was not found in the list!");
            return false;
        }
    }

    if (systems.empty()) {
        Stop();
    }
    return true;
}

void SystemManager::ThreadFunc(std::stop_token stop_token) {
    static constexpr char name[]{"AudioRenderSystemManager"};
    Common::SetCurrentThreadName(name);
    Common::SetCurrentThreadPriority(Common::ThreadPriority::High);
    while (active && !stop_token.stop_requested()) {
        {
            std::scoped_lock l{mutex1};

            SUDACHI_PROFILE("Audio", "Render System Manager");

            for (auto system : systems) {
                system->SendCommandToDsp();
            }
        }

        audio_renderer.Signal();
        audio_renderer.Wait();
    }
}

} // namespace AudioCore::Renderer
