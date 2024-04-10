// SPDX-FileCopyrightText: Copyright 2018 sudachi Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>

#include "common/common_types.h"
#include "core/file_sys/vfs/vfs_types.h"
#include "core/hle/result.h"

namespace Loader {
class AppLoader;
} // namespace Loader

namespace Service::FileSystem {
class FileSystemController;
}

namespace FileSys {

class ContentProvider;
class NCA;

enum class ContentRecordType : u8;

enum class StorageId : u8 {
    None = 0,
    Host = 1,
    GameCard = 2,
    NandSystem = 3,
    NandUser = 4,
    SdCard = 5,
    Any = 6
};

/// File system interface to the RomFS archive
class RomFSFactory {
public:
    explicit RomFSFactory(Loader::AppLoader& app_loader, ContentProvider& provider,
                          Service::FileSystem::FileSystemController& controller);
    ~RomFSFactory();

    void SetPackedUpdate(VirtualFile packed_update_raw);
    [[nodiscard]] VirtualFile OpenCurrentProcess(u64 current_process_title_id) const;
    [[nodiscard]] VirtualFile OpenPatchedRomFS(u64 title_id, ContentRecordType type) const;
    [[nodiscard]] VirtualFile OpenPatchedRomFSWithProgramIndex(u64 title_id, u8 program_index,
                                                               ContentRecordType type) const;
    [[nodiscard]] VirtualFile Open(u64 title_id, StorageId storage, ContentRecordType type) const;
    [[nodiscard]] std::shared_ptr<NCA> GetEntry(u64 title_id, StorageId storage,
                                                ContentRecordType type) const;

private:
    VirtualFile file;
    VirtualFile packed_update_raw;

    VirtualFile base;

    bool updatable;

    ContentProvider& content_provider;
    Service::FileSystem::FileSystemController& filesystem_controller;
};

} // namespace FileSys
