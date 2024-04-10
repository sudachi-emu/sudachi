// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/hle/service/nvdrv/core/nvmap.h"
#include "core/hle/service/nvnflinger/ui/graphic_buffer.h"

namespace Service::android {

static NvGraphicBuffer GetBuffer(std::shared_ptr<NvGraphicBuffer>& buffer) {
    if (buffer) {
        return *buffer;
    } else {
        return {};
    }
}

GraphicBuffer::GraphicBuffer(u32 width_, u32 height_, PixelFormat format_, u32 usage_)
    : NvGraphicBuffer(width_, height_, format_, usage_), m_nvmap(nullptr) {}

GraphicBuffer::GraphicBuffer(Service::Nvidia::NvCore::NvMap& nvmap,
                             std::shared_ptr<NvGraphicBuffer> buffer)
    : NvGraphicBuffer(GetBuffer(buffer)), m_nvmap(std::addressof(nvmap)) {
    if (this->BufferId() > 0) {
        m_nvmap->DuplicateHandle(this->BufferId(), true);
        m_nvmap->PinHandle(this->BufferId(), false);
    }
}

GraphicBuffer::~GraphicBuffer() {
    if (m_nvmap != nullptr && this->BufferId() > 0) {
        m_nvmap->UnpinHandle(this->BufferId());
        m_nvmap->FreeHandle(this->BufferId(), true);
    }
}

} // namespace Service::android
