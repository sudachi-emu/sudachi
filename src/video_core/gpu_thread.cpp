// SPDX-FileCopyrightText: Copyright 2019 sudachi Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/assert.h"
#include "common/scope_exit.h"
#include "common/settings.h"
#include "common/thread.h"
#include "core/core.h"
#include "core/frontend/graphics_context.h"
#include "video_core/control/scheduler.h"
#include "video_core/dma_pusher.h"
#include "video_core/gpu.h"
#include "video_core/gpu_thread.h"
#include "video_core/host1x/host1x.h"
#include "video_core/renderer_base.h"

namespace VideoCommon::GPUThread {

/// Runs the GPU thread
static void RunThread(std::stop_token stop_token, Core::System& system,
                      VideoCore::RendererBase& renderer, Core::Frontend::GraphicsContext& context,
                      Tegra::Control::Scheduler& scheduler, SynchState& state) {
    std::string name = "GPU";

    Common::SetCurrentThreadName(name.c_str());
    Common::SetCurrentThreadPriority(Common::ThreadPriority::Critical);
    system.RegisterHostThread();

    auto current_context = context.Acquire();
    VideoCore::RasterizerInterface* const rasterizer = renderer.ReadRasterizer();

    CommandDataContainer next;

    while (!stop_token.stop_requested()) {
        state.queue.PopWait(next, stop_token);
        if (stop_token.stop_requested()) {
            break;
        }
        if (auto* submit_list = std::get_if<SubmitListCommand>(&next.data)) {
            scheduler.Push(submit_list->channel, std::move(submit_list->entries));
        } else if (std::holds_alternative<GPUTickCommand>(next.data)) {
            system.GPU().TickWork();
        } else if (const auto* flush = std::get_if<FlushRegionCommand>(&next.data)) {
            rasterizer->FlushRegion(flush->addr, flush->size);
        } else if (const auto* invalidate = std::get_if<InvalidateRegionCommand>(&next.data)) {
            rasterizer->OnCacheInvalidation(invalidate->addr, invalidate->size);
        } else {
            ASSERT(false);
        }
        state.signaled_fence.store(next.fence);
        if (next.block) {
            // We have to lock the write_lock to ensure that the condition_variable wait not get a
            // race between the check and the lock itself.
            std::scoped_lock lk{state.write_lock};
            state.cv.notify_all();
        }
    }
}

ThreadManager::ThreadManager(Core::System& system_, bool is_async_)
    : system{system_}, is_async{is_async_} {}

ThreadManager::~ThreadManager() = default;

void ThreadManager::StartThread(VideoCore::RendererBase& renderer,
                                Core::Frontend::GraphicsContext& context,
                                Tegra::Control::Scheduler& scheduler) {
    rasterizer = renderer.ReadRasterizer();
    thread = std::jthread(RunThread, std::ref(system), std::ref(renderer), std::ref(context),
                          std::ref(scheduler), std::ref(state));
}

void ThreadManager::SubmitList(s32 channel, Tegra::CommandList&& entries) {
    PushCommand(SubmitListCommand(channel, std::move(entries)));
}

void ThreadManager::FlushRegion(DAddr addr, u64 size) {
    if (!is_async) {
        // Always flush with synchronous GPU mode
        PushCommand(FlushRegionCommand(addr, size));
        return;
    }
    if (!Settings::IsGPULevelExtreme()) {
        return;
    }
    auto& gpu = system.GPU();
    u64 fence = gpu.RequestFlush(addr, size);
    TickGPU();
    gpu.WaitForSyncOperation(fence);
}

void ThreadManager::TickGPU() {
    PushCommand(GPUTickCommand());
}

void ThreadManager::InvalidateRegion(DAddr addr, u64 size) {
    rasterizer->OnCacheInvalidation(addr, size);
}

void ThreadManager::FlushAndInvalidateRegion(DAddr addr, u64 size) {
    // Skip flush on asynch mode, as FlushAndInvalidateRegion is not used for anything too important
    rasterizer->OnCacheInvalidation(addr, size);
}

u64 ThreadManager::PushCommand(CommandData&& command_data, bool block) {
    if (!is_async) {
        // In synchronous GPU mode, block the caller until the command has executed
        block = true;
    }

    std::unique_lock lk(state.write_lock);
    const u64 fence{++state.last_fence};
    state.queue.EmplaceWait(std::move(command_data), fence, block);

    if (block) {
        Common::CondvarWait(state.cv, lk, thread.get_stop_token(), [this, fence] {
            return fence <= state.signaled_fence.load(std::memory_order_relaxed);
        });
    }

    return fence;
}

} // namespace VideoCommon::GPUThread
