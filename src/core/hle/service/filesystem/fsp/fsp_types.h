// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "core/hle/service/filesystem/filesystem.h"

namespace Service::FileSystem {

enum class FileSystemProxyType : u8 {
    Code = 0,
    Rom = 1,
    Logo = 2,
    Control = 3,
    Manual = 4,
    Meta = 5,
    Data = 6,
    Package = 7,
    RegisteredUpdate = 8,
};

struct SizeGetter {
    std::function<u64()> get_free_size;
    std::function<u64()> get_total_size;

    static SizeGetter FromStorageId(const FileSystemController& fsc, FileSys::StorageId id) {
        return {
            [&fsc, id] { return fsc.GetFreeSpaceSize(id); },
            [&fsc, id] { return fsc.GetTotalSpaceSize(id); },
        };
    }
};

} // namespace Service::FileSystem
