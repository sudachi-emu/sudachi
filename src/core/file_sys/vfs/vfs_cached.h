// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string_view>
#include <vector>
#include "core/file_sys/vfs/vfs.h"

namespace FileSys {

class CachedVfsDirectory : public ReadOnlyVfsDirectory {
public:
    CachedVfsDirectory(VirtualDir&& source_directory);

    ~CachedVfsDirectory() override;
    VirtualFile GetFile(std::string_view file_name) const override;
    VirtualDir GetSubdirectory(std::string_view dir_name) const override;
    std::vector<VirtualFile> GetFiles() const override;
    std::vector<VirtualDir> GetSubdirectories() const override;
    std::string GetName() const override;
    VirtualDir GetParentDirectory() const override;

private:
    std::string name;
    VirtualDir parent;
    std::map<std::string, VirtualDir, std::less<>> dirs;
    std::map<std::string, VirtualFile, std::less<>> files;
};

} // namespace FileSys
