// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <random>

#include "core/core.h"
#include "core/hle/kernel/k_process.h"
#include "core/hle/kernel/k_system_resource.h"
#include "core/hle/service/nvdrv/devices/nvmap.h"
#include "core/hle/service/nvdrv/nvdrv.h"
#include "core/hle/service/nvnflinger/buffer_queue_producer.h"
#include "core/hle/service/nvnflinger/fb_share_buffer_manager.h"
#include "core/hle/service/nvnflinger/pixel_format.h"
#include "core/hle/service/nvnflinger/ui/graphic_buffer.h"
#include "core/hle/service/vi/layer/vi_layer.h"
#include "core/hle/service/vi/vi_results.h"
#include "video_core/gpu.h"
#include "video_core/host1x/host1x.h"

namespace Service::Nvnflinger {

namespace {

Result AllocateSharedBufferMemory(std::unique_ptr<Kernel::KPageGroup>* out_page_group,
                                  Core::System& system, u32 size) {
    using Core::Memory::SUDACHI_PAGESIZE;

    // Allocate memory for the system shared buffer.
    // FIXME: This memory belongs to vi's .data section.
    auto& kernel = system.Kernel();

    // Hold a temporary page group reference while we try to map it.
    auto pg = std::make_unique<Kernel::KPageGroup>(
        kernel, std::addressof(kernel.GetSystemSystemResource().GetBlockInfoManager()));

    // Allocate memory from secure pool.
    R_TRY(kernel.MemoryManager().AllocateAndOpen(
        pg.get(), size / SUDACHI_PAGESIZE,
        Kernel::KMemoryManager::EncodeOption(Kernel::KMemoryManager::Pool::Secure,
                                             Kernel::KMemoryManager::Direction::FromBack)));

    // Fill the output data with red.
    for (auto& block : *pg) {
        u32* start = system.DeviceMemory().GetPointer<u32>(block.GetAddress());
        u32* end = system.DeviceMemory().GetPointer<u32>(block.GetAddress() + block.GetSize());

        for (; start < end; start++) {
            *start = 0xFF0000FF;
        }
    }

    // Return the mapped page group.
    *out_page_group = std::move(pg);

    // We succeeded.
    R_SUCCEED();
}

Result MapSharedBufferIntoProcessAddressSpace(Common::ProcessAddress* out_map_address,
                                              std::unique_ptr<Kernel::KPageGroup>& pg,
                                              Kernel::KProcess* process, Core::System& system) {
    using Core::Memory::SUDACHI_PAGESIZE;

    auto& page_table = process->GetPageTable();

    // Get bounds of where mapping is possible.
    const VAddr alias_code_begin = GetInteger(page_table.GetAliasCodeRegionStart());
    const VAddr alias_code_size = page_table.GetAliasCodeRegionSize() / SUDACHI_PAGESIZE;
    const auto state = Kernel::KMemoryState::IoMemory;
    const auto perm = Kernel::KMemoryPermission::UserReadWrite;
    std::mt19937_64 rng{process->GetRandomEntropy(0)};

    // Retry up to 64 times to map into alias code range.
    Result res = ResultSuccess;
    int i;
    for (i = 0; i < 64; i++) {
        *out_map_address = alias_code_begin + ((rng() % alias_code_size) * SUDACHI_PAGESIZE);
        res = page_table.MapPageGroup(*out_map_address, *pg, state, perm);
        if (R_SUCCEEDED(res)) {
            break;
        }
    }

    // Return failure, if necessary
    R_UNLESS(i < 64, res);

    // We succeeded.
    R_SUCCEED();
}

Result CreateNvMapHandle(u32* out_nv_map_handle, Nvidia::Devices::nvmap& nvmap, u32 size) {
    // Create a handle.
    Nvidia::Devices::nvmap::IocCreateParams create_params{
        .size = size,
        .handle = 0,
    };
    R_UNLESS(nvmap.IocCreate(create_params) == Nvidia::NvResult::Success,
             VI::ResultOperationFailed);

    // Assign the output handle.
    *out_nv_map_handle = create_params.handle;

    // We succeeded.
    R_SUCCEED();
}

Result FreeNvMapHandle(Nvidia::Devices::nvmap& nvmap, u32 handle, Nvidia::DeviceFD nvmap_fd) {
    // Free the handle.
    Nvidia::Devices::nvmap::IocFreeParams free_params{
        .handle = handle,
    };
    R_UNLESS(nvmap.IocFree(free_params, nvmap_fd) == Nvidia::NvResult::Success,
             VI::ResultOperationFailed);

    // We succeeded.
    R_SUCCEED();
}

Result AllocNvMapHandle(Nvidia::Devices::nvmap& nvmap, u32 handle, Common::ProcessAddress buffer,
                        u32 size, Nvidia::DeviceFD nvmap_fd) {
    // Assign the allocated memory to the handle.
    Nvidia::Devices::nvmap::IocAllocParams alloc_params{
        .handle = handle,
        .heap_mask = 0,
        .flags = {},
        .align = 0,
        .kind = 0,
        .address = GetInteger(buffer),
    };
    R_UNLESS(nvmap.IocAlloc(alloc_params, nvmap_fd) == Nvidia::NvResult::Success,
             VI::ResultOperationFailed);

    // We succeeded.
    R_SUCCEED();
}

Result AllocateHandleForBuffer(u32* out_handle, Nvidia::Module& nvdrv, Nvidia::DeviceFD nvmap_fd,
                               Common::ProcessAddress buffer, u32 size) {
    // Get the nvmap device.
    auto nvmap = nvdrv.GetDevice<Nvidia::Devices::nvmap>(nvmap_fd);
    ASSERT(nvmap != nullptr);

    // Create a handle.
    R_TRY(CreateNvMapHandle(out_handle, *nvmap, size));

    // Ensure we maintain a clean state on failure.
    ON_RESULT_FAILURE {
        R_ASSERT(FreeNvMapHandle(*nvmap, *out_handle, nvmap_fd));
    };

    // Assign the allocated memory to the handle.
    R_RETURN(AllocNvMapHandle(*nvmap, *out_handle, buffer, size, nvmap_fd));
}

void FreeHandle(u32 handle, Nvidia::Module& nvdrv, Nvidia::DeviceFD nvmap_fd) {
    auto nvmap = nvdrv.GetDevice<Nvidia::Devices::nvmap>(nvmap_fd);
    ASSERT(nvmap != nullptr);

    R_ASSERT(FreeNvMapHandle(*nvmap, handle, nvmap_fd));
}

constexpr auto SharedBufferBlockLinearFormat = android::PixelFormat::Rgba8888;
constexpr u32 SharedBufferBlockLinearBpp = 4;

constexpr u32 SharedBufferBlockLinearWidth = 1280;
constexpr u32 SharedBufferBlockLinearHeight = 768;
constexpr u32 SharedBufferBlockLinearStride =
    SharedBufferBlockLinearWidth * SharedBufferBlockLinearBpp;
constexpr u32 SharedBufferNumSlots = 7;

constexpr u32 SharedBufferWidth = 1280;
constexpr u32 SharedBufferHeight = 720;
constexpr u32 SharedBufferAsync = false;

constexpr u32 SharedBufferSlotSize =
    SharedBufferBlockLinearWidth * SharedBufferBlockLinearHeight * SharedBufferBlockLinearBpp;
constexpr u32 SharedBufferSize = SharedBufferSlotSize * SharedBufferNumSlots;

constexpr SharedMemoryPoolLayout SharedBufferPoolLayout = [] {
    SharedMemoryPoolLayout layout{};
    layout.num_slots = SharedBufferNumSlots;

    for (u32 i = 0; i < SharedBufferNumSlots; i++) {
        layout.slots[i].buffer_offset = i * SharedBufferSlotSize;
        layout.slots[i].size = SharedBufferSlotSize;
        layout.slots[i].width = SharedBufferWidth;
        layout.slots[i].height = SharedBufferHeight;
    }

    return layout;
}();

void MakeGraphicBuffer(android::BufferQueueProducer& producer, u32 slot, u32 handle) {
    auto buffer = std::make_shared<android::NvGraphicBuffer>();
    buffer->width = SharedBufferWidth;
    buffer->height = SharedBufferHeight;
    buffer->stride = SharedBufferBlockLinearStride;
    buffer->format = SharedBufferBlockLinearFormat;
    buffer->external_format = SharedBufferBlockLinearFormat;
    buffer->buffer_id = handle;
    buffer->offset = slot * SharedBufferSlotSize;
    ASSERT(producer.SetPreallocatedBuffer(slot, buffer) == android::Status::NoError);
}

} // namespace

FbShareBufferManager::FbShareBufferManager(Core::System& system, Nvnflinger& flinger,
                                           std::shared_ptr<Nvidia::Module> nvdrv)
    : m_system(system), m_flinger(flinger), m_nvdrv(std::move(nvdrv)) {}

FbShareBufferManager::~FbShareBufferManager() = default;

Result FbShareBufferManager::Initialize(Kernel::KProcess* owner_process, u64* out_buffer_id,
                                        u64* out_layer_handle, u64 display_id,
                                        LayerBlending blending) {
    std::scoped_lock lk{m_guard};

    // Ensure we haven't already created.
    const u64 aruid = owner_process->GetProcessId();
    R_UNLESS(!m_sessions.contains(aruid), VI::ResultPermissionDenied);

    // Allocate memory for the shared buffer if needed.
    if (!m_buffer_page_group) {
        R_TRY(AllocateSharedBufferMemory(std::addressof(m_buffer_page_group), m_system,
                                         SharedBufferSize));

        // Record buffer id.
        m_buffer_id = m_next_buffer_id++;

        // Record display id.
        m_display_id = display_id;
    }

    // Map into process.
    Common::ProcessAddress map_address{};
    R_TRY(MapSharedBufferIntoProcessAddressSpace(std::addressof(map_address), m_buffer_page_group,
                                                 owner_process, m_system));

    // Create new session.
    auto [it, was_emplaced] = m_sessions.emplace(aruid, FbShareSession{});
    auto& session = it->second;

    auto& container = m_nvdrv->GetContainer();
    session.session_id = container.OpenSession(owner_process);
    session.nvmap_fd = m_nvdrv->Open("/dev/nvmap", session.session_id);

    // Create an nvmap handle for the buffer and assign the memory to it.
    R_TRY(AllocateHandleForBuffer(std::addressof(session.buffer_nvmap_handle), *m_nvdrv,
                                  session.nvmap_fd, map_address, SharedBufferSize));

    // Create and open a layer for the display.
    session.layer_id = m_flinger.CreateLayer(m_display_id, blending).value();
    m_flinger.OpenLayer(session.layer_id);

    // Get the layer.
    VI::Layer* layer = m_flinger.FindLayer(m_display_id, session.layer_id);
    ASSERT(layer != nullptr);

    // Get the producer and set preallocated buffers.
    auto& producer = layer->GetBufferQueue();
    MakeGraphicBuffer(producer, 0, session.buffer_nvmap_handle);
    MakeGraphicBuffer(producer, 1, session.buffer_nvmap_handle);

    // Assign outputs.
    *out_buffer_id = m_buffer_id;
    *out_layer_handle = session.layer_id;

    // We succeeded.
    R_SUCCEED();
}

void FbShareBufferManager::Finalize(Kernel::KProcess* owner_process) {
    std::scoped_lock lk{m_guard};

    if (m_buffer_id == 0) {
        return;
    }

    const u64 aruid = owner_process->GetProcessId();
    const auto it = m_sessions.find(aruid);
    if (it == m_sessions.end()) {
        return;
    }

    auto& session = it->second;

    // Destroy the layer.
    m_flinger.DestroyLayer(session.layer_id);

    // Close nvmap handle.
    FreeHandle(session.buffer_nvmap_handle, *m_nvdrv, session.nvmap_fd);

    // Close nvmap device.
    m_nvdrv->Close(session.nvmap_fd);

    // Close session.
    auto& container = m_nvdrv->GetContainer();
    container.CloseSession(session.session_id);

    // Erase.
    m_sessions.erase(it);
}

Result FbShareBufferManager::GetSharedBufferMemoryHandleId(u64* out_buffer_size,
                                                           s32* out_nvmap_handle,
                                                           SharedMemoryPoolLayout* out_pool_layout,
                                                           u64 buffer_id,
                                                           u64 applet_resource_user_id) {
    std::scoped_lock lk{m_guard};

    R_UNLESS(m_buffer_id > 0, VI::ResultNotFound);
    R_UNLESS(buffer_id == m_buffer_id, VI::ResultNotFound);
    R_UNLESS(m_sessions.contains(applet_resource_user_id), VI::ResultNotFound);

    *out_pool_layout = SharedBufferPoolLayout;
    *out_buffer_size = SharedBufferSize;
    *out_nvmap_handle = m_sessions[applet_resource_user_id].buffer_nvmap_handle;

    R_SUCCEED();
}

Result FbShareBufferManager::GetLayerFromId(VI::Layer** out_layer, u64 layer_id) {
    // Ensure the layer id is valid.
    R_UNLESS(layer_id > 0, VI::ResultNotFound);

    // Get the layer.
    VI::Layer* layer = m_flinger.FindLayer(m_display_id, layer_id);
    R_UNLESS(layer != nullptr, VI::ResultNotFound);

    // We succeeded.
    *out_layer = layer;
    R_SUCCEED();
}

Result FbShareBufferManager::AcquireSharedFrameBuffer(android::Fence* out_fence,
                                                      std::array<s32, 4>& out_slot_indexes,
                                                      s64* out_target_slot, u64 layer_id) {
    std::scoped_lock lk{m_guard};

    // Get the layer.
    VI::Layer* layer;
    R_TRY(this->GetLayerFromId(std::addressof(layer), layer_id));

    // Get the producer.
    auto& producer = layer->GetBufferQueue();

    // Get the next buffer from the producer.
    s32 slot;
    R_UNLESS(producer.DequeueBuffer(std::addressof(slot), out_fence, SharedBufferAsync != 0,
                                    SharedBufferWidth, SharedBufferHeight,
                                    SharedBufferBlockLinearFormat, 0) == android::Status::NoError,
             VI::ResultOperationFailed);

    // Assign remaining outputs.
    *out_target_slot = slot;
    out_slot_indexes = {0, 1, -1, -1};

    // We succeeded.
    R_SUCCEED();
}

Result FbShareBufferManager::PresentSharedFrameBuffer(android::Fence fence,
                                                      Common::Rectangle<s32> crop_region,
                                                      u32 transform, s32 swap_interval,
                                                      u64 layer_id, s64 slot) {
    std::scoped_lock lk{m_guard};

    // Get the layer.
    VI::Layer* layer;
    R_TRY(this->GetLayerFromId(std::addressof(layer), layer_id));

    // Get the producer.
    auto& producer = layer->GetBufferQueue();

    // Request to queue the buffer.
    std::shared_ptr<android::GraphicBuffer> buffer;
    R_UNLESS(producer.RequestBuffer(static_cast<s32>(slot), std::addressof(buffer)) ==
                 android::Status::NoError,
             VI::ResultOperationFailed);

    ON_RESULT_FAILURE {
        producer.CancelBuffer(static_cast<s32>(slot), fence);
    };

    // Queue the buffer to the producer.
    android::QueueBufferInput input{};
    android::QueueBufferOutput output{};
    input.crop = crop_region;
    input.fence = fence;
    input.transform = static_cast<android::NativeWindowTransform>(transform);
    input.swap_interval = swap_interval;
    R_UNLESS(producer.QueueBuffer(static_cast<s32>(slot), input, std::addressof(output)) ==
                 android::Status::NoError,
             VI::ResultOperationFailed);

    // We succeeded.
    R_SUCCEED();
}

Result FbShareBufferManager::GetSharedFrameBufferAcquirableEvent(Kernel::KReadableEvent** out_event,
                                                                 u64 layer_id) {
    std::scoped_lock lk{m_guard};

    // Get the layer.
    VI::Layer* layer;
    R_TRY(this->GetLayerFromId(std::addressof(layer), layer_id));

    // Get the producer.
    auto& producer = layer->GetBufferQueue();

    // Set the event.
    *out_event = std::addressof(producer.GetNativeHandle());

    // We succeeded.
    R_SUCCEED();
}

Result FbShareBufferManager::WriteAppletCaptureBuffer(bool* out_was_written, s32* out_layer_index) {
    std::vector<u8> capture_buffer(m_system.GPU().GetAppletCaptureBuffer());
    Common::ScratchBuffer<u32> scratch;

    // TODO: this could be optimized
    s64 e = -1280 * 768 * 4;
    for (auto& block : *m_buffer_page_group) {
        u8* start = m_system.DeviceMemory().GetPointer<u8>(block.GetAddress());
        u8* end = m_system.DeviceMemory().GetPointer<u8>(block.GetAddress() + block.GetSize());

        for (; start < end; start++) {
            *start = 0;

            if (e >= 0 && e < static_cast<s64>(capture_buffer.size())) {
                *start = capture_buffer[e];
            }
            e++;
        }

        m_system.GPU().Host1x().MemoryManager().ApplyOpOnPointer(start, scratch, [&](DAddr addr) {
            m_system.GPU().InvalidateRegion(addr, end - start);
        });
    }

    *out_was_written = true;
    *out_layer_index = 1;
    R_SUCCEED();
}

} // namespace Service::Nvnflinger
