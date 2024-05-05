// SPDX-FileCopyrightText: Copyright 2021 sudachi Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <array>
#include <memory>
#include <span>

#include <glad/glad.h>

#include "common/alignment.h"
#include "common/assert.h"
#include "common/bit_util.h"
#include "common/profiling.h"
#include "video_core/renderer_opengl/gl_staging_buffer_pool.h"

namespace OpenGL {

StagingBufferMap::~StagingBufferMap() {
    if (sync) {
        sync->Create();
    }
}

StagingBuffers::StagingBuffers(GLenum storage_flags_, GLenum map_flags_)
    : storage_flags{storage_flags_}, map_flags{map_flags_} {}

StagingBuffers::~StagingBuffers() = default;

StagingBufferMap StagingBuffers::RequestMap(size_t requested_size, bool insert_fence,
                                            bool deferred) {
    SUDACHI_PROFILE("OpenGL", "Buffer Request");

    const size_t index = RequestBuffer(requested_size);
    OGLSync* const sync = insert_fence ? &allocs[index].sync : nullptr;
    allocs[index].sync_index = insert_fence ? ++current_sync_index : 0;
    allocs[index].deferred = deferred;
    return StagingBufferMap{
        .mapped_span = std::span(allocs[index].map, requested_size),
        .sync = sync,
        .buffer = allocs[index].buffer.handle,
        .index = index,
    };
}

void StagingBuffers::FreeDeferredStagingBuffer(size_t index) {
    ASSERT(allocs[index].deferred);
    allocs[index].deferred = false;
}

size_t StagingBuffers::RequestBuffer(size_t requested_size) {
    if (const std::optional<size_t> index = FindBuffer(requested_size); index) {
        return *index;
    }
    StagingBufferAlloc alloc;
    alloc.buffer.Create();
    const auto next_pow2_size = Common::NextPow2(requested_size);
    glNamedBufferStorage(alloc.buffer.handle, next_pow2_size, nullptr,
                         storage_flags | GL_MAP_PERSISTENT_BIT);
    alloc.map = static_cast<u8*>(glMapNamedBufferRange(alloc.buffer.handle, 0, next_pow2_size,
                                                       map_flags | GL_MAP_PERSISTENT_BIT));
    alloc.size = next_pow2_size;
    allocs.emplace_back(std::move(alloc));
    return allocs.size() - 1;
}

std::optional<size_t> StagingBuffers::FindBuffer(size_t requested_size) {
    size_t known_unsignaled_index = current_sync_index + 1;
    size_t smallest_buffer = std::numeric_limits<size_t>::max();
    std::optional<size_t> found;
    const size_t num_buffers = allocs.size();
    for (size_t index = 0; index < num_buffers; ++index) {
        StagingBufferAlloc& alloc = allocs[index];
        const size_t buffer_size = alloc.size;
        if (buffer_size < requested_size || buffer_size >= smallest_buffer) {
            continue;
        }
        if (alloc.deferred) {
            continue;
        }
        if (alloc.sync.handle != 0) {
            if (alloc.sync_index >= known_unsignaled_index) {
                // This fence is later than a fence that is known to not be signaled
                continue;
            }
            if (!alloc.sync.IsSignaled()) {
                // Since this fence hasn't been signaled, it's safe to assume all later
                // fences haven't been signaled either
                known_unsignaled_index = std::min(known_unsignaled_index, alloc.sync_index);
                continue;
            }
            alloc.sync.Release();
        }
        smallest_buffer = buffer_size;
        found = index;
    }
    return found;
}

StreamBuffer::StreamBuffer() {
    static constexpr GLenum flags = GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT;
    buffer.Create();
    glObjectLabel(GL_BUFFER, buffer.handle, -1, "Stream Buffer");
    glNamedBufferStorage(buffer.handle, STREAM_BUFFER_SIZE, nullptr, flags);
    mapped_pointer =
        static_cast<u8*>(glMapNamedBufferRange(buffer.handle, 0, STREAM_BUFFER_SIZE, flags));
    for (OGLSync& sync : fences) {
        sync.Create();
    }
}

std::pair<std::span<u8>, size_t> StreamBuffer::Request(size_t size) noexcept {
    ASSERT(size < REGION_SIZE);
    for (size_t region = Region(used_iterator), region_end = Region(iterator); region < region_end;
         ++region) {
        fences[region].Create();
    }
    used_iterator = iterator;

    for (size_t region = Region(free_iterator) + 1,
                region_end = std::min(Region(iterator + size) + 1, NUM_SYNCS);
         region < region_end; ++region) {
        glClientWaitSync(fences[region].handle, 0, GL_TIMEOUT_IGNORED);
        fences[region].Release();
    }
    if (iterator + size >= free_iterator) {
        free_iterator = iterator + size;
    }
    if (iterator + size > STREAM_BUFFER_SIZE) {
        for (size_t region = Region(used_iterator); region < NUM_SYNCS; ++region) {
            fences[region].Create();
        }
        used_iterator = 0;
        iterator = 0;
        free_iterator = size;

        for (size_t region = 0, region_end = Region(size); region <= region_end; ++region) {
            glClientWaitSync(fences[region].handle, 0, GL_TIMEOUT_IGNORED);
            fences[region].Release();
        }
    }
    const size_t offset = iterator;
    iterator = Common::AlignUp(iterator + size, MAX_ALIGNMENT);
    return {std::span(mapped_pointer + offset, size), offset};
}

StagingBufferMap StagingBufferPool::RequestUploadBuffer(size_t size) {
    return upload_buffers.RequestMap(size, true);
}

StagingBufferMap StagingBufferPool::RequestDownloadBuffer(size_t size, bool deferred) {
    return download_buffers.RequestMap(size, false, deferred);
}

void StagingBufferPool::FreeDeferredStagingBuffer(StagingBufferMap& buffer) {
    download_buffers.FreeDeferredStagingBuffer(buffer.index);
}

} // namespace OpenGL
