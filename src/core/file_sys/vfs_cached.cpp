// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/file_sys/vfs_cached.h"
#include "core/file_sys/vfs_types.h"

namespace FileSys {

CachedVfsDirectory::CachedVfsDirectory(VirtualDir&& source_dir)
    : name(source_dir->GetName()), parent(source_dir->GetParentDirectory()) {
    for (auto& dir : source_dir->GetSubdirectories()) {
        dirs.emplace(dir->GetName(), std::make_shared<CachedVfsDirectory>(std::move(dir)));
    }
    for (auto& file : source_dir->GetFiles()) {
        files.emplace(file->GetName(), std::move(file));
    }
}

CachedVfsDirectory::~CachedVfsDirectory() = default;

VirtualFile CachedVfsDirectory::GetFile(std::string_view file_name) const {
    auto it = files.find(file_name);
    if (it != files.end()) {
        return it->second;
    }

    return nullptr;
}

VirtualDir CachedVfsDirectory::GetSubdirectory(std::string_view dir_name) const {
    auto it = dirs.find(dir_name);
    if (it != dirs.end()) {
        return it->second;
    }

    return nullptr;
}

std::vector<VirtualFile> CachedVfsDirectory::GetFiles() const {
    std::vector<VirtualFile> out;
    for (auto& [file_name, file] : files) {
        out.push_back(file);
    }
    return out;
}

std::vector<VirtualDir> CachedVfsDirectory::GetSubdirectories() const {
    std::vector<VirtualDir> out;
    for (auto& [dir_name, dir] : dirs) {
        out.push_back(dir);
    }
    return out;
}

std::string CachedVfsDirectory::GetName() const {
    return name;
}

VirtualDir CachedVfsDirectory::GetParentDirectory() const {
    return parent;
}

} // namespace FileSys
