// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/file_sys/fs_filesystem.h"
#include "core/file_sys/savedata_factory.h"
#include "core/hle/service/cmif_serialization.h"
#include "core/hle/service/filesystem/fsp/fs_i_directory.h"

namespace Service::FileSystem {

IDirectory::IDirectory(Core::System& system_, FileSys::VirtualDir directory_,
                       FileSys::OpenDirectoryMode mode)
    : ServiceFramework{system_, "IDirectory"},
      backend(std::make_unique<FileSys::Fsa::IDirectory>(directory_, mode)) {
    static const FunctionInfo functions[] = {
        {0, D<&IDirectory::Read>, "Read"},
        {1, D<&IDirectory::GetEntryCount>, "GetEntryCount"},
    };
    RegisterHandlers(functions);
}

Result IDirectory::Read(
    Out<s64> out_count,
    const OutArray<FileSys::DirectoryEntry, BufferAttr_HipcMapAlias> out_entries) {
    LOG_DEBUG(Service_FS, "called.");

    R_RETURN(backend->Read(out_count, out_entries.data(), out_entries.size()));
}

Result IDirectory::GetEntryCount(Out<s64> out_count) {
    LOG_DEBUG(Service_FS, "called");

    R_RETURN(backend->GetEntryCount(out_count));
}

} // namespace Service::FileSystem
