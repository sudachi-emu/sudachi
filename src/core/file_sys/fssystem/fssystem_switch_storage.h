// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "core/file_sys/fssystem/fs_i_storage.h"

namespace FileSys {

class RegionSwitchStorage : public IReadOnlyStorage {
    SUDACHI_NON_COPYABLE(RegionSwitchStorage);
    SUDACHI_NON_MOVEABLE(RegionSwitchStorage);

public:
    struct Region {
        s64 offset;
        s64 size;
    };

public:
    RegionSwitchStorage(VirtualFile&& i, VirtualFile&& o, Region r)
        : m_inside_region_storage(std::move(i)), m_outside_region_storage(std::move(o)),
          m_region(r) {}

    virtual size_t Read(u8* buffer, size_t size, size_t offset) const override {
        // Process until we're done.
        size_t processed = 0;
        while (processed < size) {
            // Process on the appropriate storage.
            s64 cur_size = 0;
            if (this->CheckRegions(std::addressof(cur_size), offset + processed,
                                   size - processed)) {
                m_inside_region_storage->Read(buffer + processed, cur_size, offset + processed);
            } else {
                m_outside_region_storage->Read(buffer + processed, cur_size, offset + processed);
            }

            // Advance.
            processed += cur_size;
        }

        return size;
    }

    virtual size_t GetSize() const override {
        return m_inside_region_storage->GetSize();
    }

private:
    bool CheckRegions(s64* out_current_size, s64 offset, s64 size) const {
        // Check if our region contains the access.
        if (m_region.offset <= offset) {
            if (offset < m_region.offset + m_region.size) {
                if (m_region.offset + m_region.size <= offset + size) {
                    *out_current_size = m_region.offset + m_region.size - offset;
                } else {
                    *out_current_size = size;
                }
                return true;
            } else {
                *out_current_size = size;
                return false;
            }
        } else {
            if (m_region.offset <= offset + size) {
                *out_current_size = m_region.offset - offset;
            } else {
                *out_current_size = size;
            }
            return false;
        }
    }

private:
    VirtualFile m_inside_region_storage;
    VirtualFile m_outside_region_storage;
    Region m_region;
};

} // namespace FileSys
