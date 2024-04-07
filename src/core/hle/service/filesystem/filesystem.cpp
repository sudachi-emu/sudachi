// SPDX-FileCopyrightText: Copyright 2018 sudachi Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <utility>

#include "common/assert.h"
#include "common/fs/fs.h"
#include "common/fs/path_util.h"
#include "common/settings.h"
#include "core/core.h"
#include "core/file_sys/bis_factory.h"
#include "core/file_sys/card_image.h"
#include "core/file_sys/control_metadata.h"
#include "core/file_sys/errors.h"
#include "core/file_sys/patch_manager.h"
#include "core/file_sys/registered_cache.h"
#include "core/file_sys/romfs_factory.h"
#include "core/file_sys/savedata_factory.h"
#include "core/file_sys/sdmc_factory.h"
#include "core/file_sys/vfs/vfs.h"
#include "core/file_sys/vfs/vfs_offset.h"
#include "core/hle/service/filesystem/filesystem.h"
#include "core/hle/service/filesystem/fsp/fsp_ldr.h"
#include "core/hle/service/filesystem/fsp/fsp_pr.h"
#include "core/hle/service/filesystem/fsp/fsp_srv.h"
#include "core/hle/service/filesystem/romfs_controller.h"
#include "core/hle/service/filesystem/save_data_controller.h"
#include "core/hle/service/server_manager.h"
#include "core/loader/loader.h"

namespace Service::FileSystem {

static FileSys::VirtualDir GetDirectoryRelativeWrapped(FileSys::VirtualDir base,
                                                       std::string_view dir_name_) {
    std::string dir_name(Common::FS::SanitizePath(dir_name_));
    if (dir_name.empty() || dir_name == "." || dir_name == "/" || dir_name == "\\")
        return base;

    return base->GetDirectoryRelative(dir_name);
}

VfsDirectoryServiceWrapper::VfsDirectoryServiceWrapper(FileSys::VirtualDir backing_)
    : backing(std::move(backing_)) {}

VfsDirectoryServiceWrapper::~VfsDirectoryServiceWrapper() = default;

std::string VfsDirectoryServiceWrapper::GetName() const {
    return backing->GetName();
}

Result VfsDirectoryServiceWrapper::CreateFile(const std::string& path_, u64 size) const {
    std::string path(Common::FS::SanitizePath(path_));
    auto dir = GetDirectoryRelativeWrapped(backing, Common::FS::GetParentPath(path));
    if (dir == nullptr) {
        return FileSys::ResultPathNotFound;
    }

    FileSys::DirectoryEntryType entry_type{};
    if (GetEntryType(&entry_type, path) == ResultSuccess) {
        return FileSys::ResultPathAlreadyExists;
    }

    auto file = dir->CreateFile(Common::FS::GetFilename(path));
    if (file == nullptr) {
        // TODO(DarkLordZach): Find a better error code for this
        return ResultUnknown;
    }
    if (!file->Resize(size)) {
        // TODO(DarkLordZach): Find a better error code for this
        return ResultUnknown;
    }
    return ResultSuccess;
}

Result VfsDirectoryServiceWrapper::DeleteFile(const std::string& path_) const {
    std::string path(Common::FS::SanitizePath(path_));
    if (path.empty()) {
        // TODO(DarkLordZach): Why do games call this and what should it do? Works as is but...
        return ResultSuccess;
    }

    auto dir = GetDirectoryRelativeWrapped(backing, Common::FS::GetParentPath(path));
    if (dir == nullptr || dir->GetFile(Common::FS::GetFilename(path)) == nullptr) {
        return FileSys::ResultPathNotFound;
    }
    if (!dir->DeleteFile(Common::FS::GetFilename(path))) {
        // TODO(DarkLordZach): Find a better error code for this
        return ResultUnknown;
    }

    return ResultSuccess;
}

Result VfsDirectoryServiceWrapper::CreateDirectory(const std::string& path_) const {
    std::string path(Common::FS::SanitizePath(path_));

    // NOTE: This is inaccurate behavior. CreateDirectory is not recursive.
    // CreateDirectory should return PathNotFound if the parent directory does not exist.
    // This is here temporarily in order to have UMM "work" in the meantime.
    // TODO (Morph): Remove this when a hardware test verifies the correct behavior.
    const auto components = Common::FS::SplitPathComponents(path);
    std::string relative_path;
    for (const auto& component : components) {
        relative_path = Common::FS::SanitizePath(fmt::format("{}/{}", relative_path, component));
        auto new_dir = backing->CreateSubdirectory(relative_path);
        if (new_dir == nullptr) {
            // TODO(DarkLordZach): Find a better error code for this
            return ResultUnknown;
        }
    }
    return ResultSuccess;
}

Result VfsDirectoryServiceWrapper::DeleteDirectory(const std::string& path_) const {
    std::string path(Common::FS::SanitizePath(path_));
    auto dir = GetDirectoryRelativeWrapped(backing, Common::FS::GetParentPath(path));
    if (!dir->DeleteSubdirectory(Common::FS::GetFilename(path))) {
        // TODO(DarkLordZach): Find a better error code for this
        return ResultUnknown;
    }
    return ResultSuccess;
}

Result VfsDirectoryServiceWrapper::DeleteDirectoryRecursively(const std::string& path_) const {
    std::string path(Common::FS::SanitizePath(path_));
    auto dir = GetDirectoryRelativeWrapped(backing, Common::FS::GetParentPath(path));
    if (!dir->DeleteSubdirectoryRecursive(Common::FS::GetFilename(path))) {
        // TODO(DarkLordZach): Find a better error code for this
        return ResultUnknown;
    }
    return ResultSuccess;
}

Result VfsDirectoryServiceWrapper::CleanDirectoryRecursively(const std::string& path) const {
    const std::string sanitized_path(Common::FS::SanitizePath(path));
    auto dir = GetDirectoryRelativeWrapped(backing, Common::FS::GetParentPath(sanitized_path));

    if (!dir->CleanSubdirectoryRecursive(Common::FS::GetFilename(sanitized_path))) {
        // TODO(DarkLordZach): Find a better error code for this
        return ResultUnknown;
    }

    return ResultSuccess;
}

Result VfsDirectoryServiceWrapper::RenameFile(const std::string& src_path_,
                                              const std::string& dest_path_) const {
    std::string src_path(Common::FS::SanitizePath(src_path_));
    std::string dest_path(Common::FS::SanitizePath(dest_path_));
    auto src = backing->GetFileRelative(src_path);
    auto dst = backing->GetFileRelative(dest_path);
    if (Common::FS::GetParentPath(src_path) == Common::FS::GetParentPath(dest_path)) {
        // Use more-optimized vfs implementation rename.
        if (src == nullptr) {
            return FileSys::ResultPathNotFound;
        }

        if (dst && Common::FS::Exists(dst->GetFullPath())) {
            LOG_ERROR(Service_FS, "File at new_path={} already exists", dst->GetFullPath());
            return FileSys::ResultPathAlreadyExists;
        }

        if (!src->Rename(Common::FS::GetFilename(dest_path))) {
            // TODO(DarkLordZach): Find a better error code for this
            return ResultUnknown;
        }
        return ResultSuccess;
    }

    // Move by hand -- TODO(DarkLordZach): Optimize
    auto c_res = CreateFile(dest_path, src->GetSize());
    if (c_res != ResultSuccess)
        return c_res;

    auto dest = backing->GetFileRelative(dest_path);
    ASSERT_MSG(dest != nullptr, "Newly created file with success cannot be found.");

    ASSERT_MSG(dest->WriteBytes(src->ReadAllBytes()) == src->GetSize(),
               "Could not write all of the bytes but everything else has succeeded.");

    if (!src->GetContainingDirectory()->DeleteFile(Common::FS::GetFilename(src_path))) {
        // TODO(DarkLordZach): Find a better error code for this
        return ResultUnknown;
    }

    return ResultSuccess;
}

Result VfsDirectoryServiceWrapper::RenameDirectory(const std::string& src_path_,
                                                   const std::string& dest_path_) const {
    std::string src_path(Common::FS::SanitizePath(src_path_));
    std::string dest_path(Common::FS::SanitizePath(dest_path_));
    auto src = GetDirectoryRelativeWrapped(backing, src_path);
    if (Common::FS::GetParentPath(src_path) == Common::FS::GetParentPath(dest_path)) {
        // Use more-optimized vfs implementation rename.
        if (src == nullptr)
            return FileSys::ResultPathNotFound;
        if (!src->Rename(Common::FS::GetFilename(dest_path))) {
            // TODO(DarkLordZach): Find a better error code for this
            return ResultUnknown;
        }
        return ResultSuccess;
    }

    // TODO(DarkLordZach): Implement renaming across the tree (move).
    ASSERT_MSG(false,
               "Could not rename directory with path \"{}\" to new path \"{}\" because parent dirs "
               "don't match -- UNIMPLEMENTED",
               src_path, dest_path);

    // TODO(DarkLordZach): Find a better error code for this
    return ResultUnknown;
}

Result VfsDirectoryServiceWrapper::OpenFile(FileSys::VirtualFile* out_file,
                                            const std::string& path_,
                                            FileSys::OpenMode mode) const {
    const std::string path(Common::FS::SanitizePath(path_));
    std::string_view npath = path;
    while (!npath.empty() && (npath[0] == '/' || npath[0] == '\\')) {
        npath.remove_prefix(1);
    }

    auto file = backing->GetFileRelative(npath);
    if (file == nullptr) {
        return FileSys::ResultPathNotFound;
    }

    if (mode == FileSys::OpenMode::AllowAppend) {
        *out_file = std::make_shared<FileSys::OffsetVfsFile>(file, 0, file->GetSize());
    } else {
        *out_file = file;
    }

    return ResultSuccess;
}

Result VfsDirectoryServiceWrapper::OpenDirectory(FileSys::VirtualDir* out_directory,
                                                 const std::string& path_) {
    std::string path(Common::FS::SanitizePath(path_));
    auto dir = GetDirectoryRelativeWrapped(backing, path);
    if (dir == nullptr) {
        // TODO(DarkLordZach): Find a better error code for this
        return FileSys::ResultPathNotFound;
    }
    *out_directory = dir;
    return ResultSuccess;
}

Result VfsDirectoryServiceWrapper::GetEntryType(FileSys::DirectoryEntryType* out_entry_type,
                                                const std::string& path_) const {
    std::string path(Common::FS::SanitizePath(path_));
    auto dir = GetDirectoryRelativeWrapped(backing, Common::FS::GetParentPath(path));
    if (dir == nullptr) {
        return FileSys::ResultPathNotFound;
    }

    auto filename = Common::FS::GetFilename(path);
    // TODO(Subv): Some games use the '/' path, find out what this means.
    if (filename.empty()) {
        *out_entry_type = FileSys::DirectoryEntryType::Directory;
        return ResultSuccess;
    }

    if (dir->GetFile(filename) != nullptr) {
        *out_entry_type = FileSys::DirectoryEntryType::File;
        return ResultSuccess;
    }

    if (dir->GetSubdirectory(filename) != nullptr) {
        *out_entry_type = FileSys::DirectoryEntryType::Directory;
        return ResultSuccess;
    }

    return FileSys::ResultPathNotFound;
}

Result VfsDirectoryServiceWrapper::GetFileTimeStampRaw(
    FileSys::FileTimeStampRaw* out_file_time_stamp_raw, const std::string& path) const {
    auto dir = GetDirectoryRelativeWrapped(backing, Common::FS::GetParentPath(path));
    if (dir == nullptr) {
        return FileSys::ResultPathNotFound;
    }

    FileSys::DirectoryEntryType entry_type;
    if (GetEntryType(&entry_type, path) != ResultSuccess) {
        return FileSys::ResultPathNotFound;
    }

    *out_file_time_stamp_raw = dir->GetFileTimeStamp(Common::FS::GetFilename(path));
    return ResultSuccess;
}

FileSystemController::FileSystemController(Core::System& system_) : system{system_} {}

FileSystemController::~FileSystemController() = default;

Result FileSystemController::RegisterProcess(
    ProcessId process_id, ProgramId program_id,
    std::shared_ptr<FileSys::RomFSFactory>&& romfs_factory) {
    std::scoped_lock lk{registration_lock};

    registrations.emplace(process_id, Registration{
                                          .program_id = program_id,
                                          .romfs_factory = std::move(romfs_factory),
                                          .save_data_factory = CreateSaveDataFactory(program_id),
                                      });

    LOG_DEBUG(Service_FS, "Registered for process {}", process_id);
    return ResultSuccess;
}

Result FileSystemController::OpenProcess(
    ProgramId* out_program_id, std::shared_ptr<SaveDataController>* out_save_data_controller,
    std::shared_ptr<RomFsController>* out_romfs_controller, ProcessId process_id) {
    std::scoped_lock lk{registration_lock};

    const auto it = registrations.find(process_id);
    if (it == registrations.end()) {
        return FileSys::ResultTargetNotFound;
    }

    *out_program_id = it->second.program_id;
    *out_save_data_controller =
        std::make_shared<SaveDataController>(system, it->second.save_data_factory);
    *out_romfs_controller =
        std::make_shared<RomFsController>(it->second.romfs_factory, it->second.program_id);
    return ResultSuccess;
}

void FileSystemController::SetPackedUpdate(ProcessId process_id, FileSys::VirtualFile update_raw) {
    LOG_TRACE(Service_FS, "Setting packed update for romfs");

    std::scoped_lock lk{registration_lock};
    const auto it = registrations.find(process_id);
    if (it == registrations.end()) {
        return;
    }

    it->second.romfs_factory->SetPackedUpdate(std::move(update_raw));
}

std::shared_ptr<SaveDataController> FileSystemController::OpenSaveDataController() {
    return std::make_shared<SaveDataController>(system, CreateSaveDataFactory(ProgramId{}));
}

std::shared_ptr<FileSys::SaveDataFactory> FileSystemController::CreateSaveDataFactory(
    ProgramId program_id) {
    using SudachiPath = Common::FS::SudachiPath;
    const auto rw_mode = FileSys::OpenMode::ReadWrite;

    auto vfs = system.GetFilesystem();
    const auto nand_directory =
        vfs->OpenDirectory(Common::FS::GetSudachiPathString(SudachiPath::NANDDir), rw_mode);
    return std::make_shared<FileSys::SaveDataFactory>(system, program_id,
                                                      std::move(nand_directory));
}

Result FileSystemController::OpenSDMC(FileSys::VirtualDir* out_sdmc) const {
    LOG_TRACE(Service_FS, "Opening SDMC");

    if (sdmc_factory == nullptr) {
        return FileSys::ResultPortSdCardNoDevice;
    }

    auto sdmc = sdmc_factory->Open();
    if (sdmc == nullptr) {
        return FileSys::ResultPortSdCardNoDevice;
    }

    *out_sdmc = sdmc;
    return ResultSuccess;
}

Result FileSystemController::OpenBISPartition(FileSys::VirtualDir* out_bis_partition,
                                              FileSys::BisPartitionId id) const {
    LOG_TRACE(Service_FS, "Opening BIS Partition with id={:08X}", id);

    if (bis_factory == nullptr) {
        return FileSys::ResultTargetNotFound;
    }

    auto part = bis_factory->OpenPartition(id);
    if (part == nullptr) {
        return FileSys::ResultInvalidArgument;
    }

    *out_bis_partition = part;
    return ResultSuccess;
}

Result FileSystemController::OpenBISPartitionStorage(
    FileSys::VirtualFile* out_bis_partition_storage, FileSys::BisPartitionId id) const {
    LOG_TRACE(Service_FS, "Opening BIS Partition Storage with id={:08X}", id);

    if (bis_factory == nullptr) {
        return FileSys::ResultTargetNotFound;
    }

    auto part = bis_factory->OpenPartitionStorage(id, system.GetFilesystem());
    if (part == nullptr) {
        return FileSys::ResultInvalidArgument;
    }

    *out_bis_partition_storage = part;
    return ResultSuccess;
}

u64 FileSystemController::GetFreeSpaceSize(FileSys::StorageId id) const {
    switch (id) {
    case FileSys::StorageId::None:
    case FileSys::StorageId::GameCard:
    case FileSys::StorageId::Any:
        return 0;
    case FileSys::StorageId::SdCard:
        if (sdmc_factory == nullptr)
            return 0;
        return sdmc_factory->GetSDMCFreeSpace();
    case FileSys::StorageId::Host:
        if (bis_factory == nullptr)
            return 0;
        return bis_factory->GetSystemNANDFreeSpace() + bis_factory->GetUserNANDFreeSpace();
    case FileSys::StorageId::NandSystem:
        if (bis_factory == nullptr)
            return 0;
        return bis_factory->GetSystemNANDFreeSpace();
    case FileSys::StorageId::NandUser:
        if (bis_factory == nullptr)
            return 0;
        return bis_factory->GetUserNANDFreeSpace();
    }

    return 0;
}

u64 FileSystemController::GetTotalSpaceSize(FileSys::StorageId id) const {
    switch (id) {
    case FileSys::StorageId::None:
    case FileSys::StorageId::GameCard:
    case FileSys::StorageId::Any:
        return 0;
    case FileSys::StorageId::SdCard:
        if (sdmc_factory == nullptr)
            return 0;
        return sdmc_factory->GetSDMCTotalSpace();
    case FileSys::StorageId::Host:
        if (bis_factory == nullptr)
            return 0;
        return bis_factory->GetFullNANDTotalSpace();
    case FileSys::StorageId::NandSystem:
        if (bis_factory == nullptr)
            return 0;
        return bis_factory->GetSystemNANDTotalSpace();
    case FileSys::StorageId::NandUser:
        if (bis_factory == nullptr)
            return 0;
        return bis_factory->GetUserNANDTotalSpace();
    }

    return 0;
}

void FileSystemController::SetGameCard(FileSys::VirtualFile file) {
    gamecard = std::make_unique<FileSys::XCI>(file);
    const auto dir = gamecard->ConcatenatedPseudoDirectory();
    gamecard_registered = std::make_unique<FileSys::RegisteredCache>(dir);
    gamecard_placeholder = std::make_unique<FileSys::PlaceholderCache>(dir);
}

FileSys::XCI* FileSystemController::GetGameCard() const {
    return gamecard.get();
}

FileSys::RegisteredCache* FileSystemController::GetGameCardContents() const {
    return gamecard_registered.get();
}

FileSys::PlaceholderCache* FileSystemController::GetGameCardPlaceholder() const {
    return gamecard_placeholder.get();
}

FileSys::RegisteredCache* FileSystemController::GetSystemNANDContents() const {
    LOG_TRACE(Service_FS, "Opening System NAND Contents");

    if (bis_factory == nullptr)
        return nullptr;

    return bis_factory->GetSystemNANDContents();
}

FileSys::RegisteredCache* FileSystemController::GetUserNANDContents() const {
    LOG_TRACE(Service_FS, "Opening User NAND Contents");

    if (bis_factory == nullptr)
        return nullptr;

    return bis_factory->GetUserNANDContents();
}

FileSys::RegisteredCache* FileSystemController::GetSDMCContents() const {
    LOG_TRACE(Service_FS, "Opening SDMC Contents");

    if (sdmc_factory == nullptr)
        return nullptr;

    return sdmc_factory->GetSDMCContents();
}

FileSys::PlaceholderCache* FileSystemController::GetSystemNANDPlaceholder() const {
    LOG_TRACE(Service_FS, "Opening System NAND Placeholder");

    if (bis_factory == nullptr)
        return nullptr;

    return bis_factory->GetSystemNANDPlaceholder();
}

FileSys::PlaceholderCache* FileSystemController::GetUserNANDPlaceholder() const {
    LOG_TRACE(Service_FS, "Opening User NAND Placeholder");

    if (bis_factory == nullptr)
        return nullptr;

    return bis_factory->GetUserNANDPlaceholder();
}

FileSys::PlaceholderCache* FileSystemController::GetSDMCPlaceholder() const {
    LOG_TRACE(Service_FS, "Opening SDMC Placeholder");

    if (sdmc_factory == nullptr)
        return nullptr;

    return sdmc_factory->GetSDMCPlaceholder();
}

FileSys::RegisteredCache* FileSystemController::GetRegisteredCacheForStorage(
    FileSys::StorageId id) const {
    switch (id) {
    case FileSys::StorageId::None:
    case FileSys::StorageId::Host:
    case FileSys::StorageId::Any:
        UNIMPLEMENTED();
        return nullptr;
    case FileSys::StorageId::GameCard:
        return GetGameCardContents();
    case FileSys::StorageId::NandSystem:
        return GetSystemNANDContents();
    case FileSys::StorageId::NandUser:
        return GetUserNANDContents();
    case FileSys::StorageId::SdCard:
        return GetSDMCContents();
    }

    return nullptr;
}

FileSys::PlaceholderCache* FileSystemController::GetPlaceholderCacheForStorage(
    FileSys::StorageId id) const {
    switch (id) {
    case FileSys::StorageId::None:
    case FileSys::StorageId::Host:
    case FileSys::StorageId::Any:
        UNIMPLEMENTED();
        return nullptr;
    case FileSys::StorageId::GameCard:
        return GetGameCardPlaceholder();
    case FileSys::StorageId::NandSystem:
        return GetSystemNANDPlaceholder();
    case FileSys::StorageId::NandUser:
        return GetUserNANDPlaceholder();
    case FileSys::StorageId::SdCard:
        return GetSDMCPlaceholder();
    }

    return nullptr;
}

FileSys::VirtualDir FileSystemController::GetSystemNANDContentDirectory() const {
    LOG_TRACE(Service_FS, "Opening system NAND content directory");

    if (bis_factory == nullptr)
        return nullptr;

    return bis_factory->GetSystemNANDContentDirectory();
}

FileSys::VirtualDir FileSystemController::GetUserNANDContentDirectory() const {
    LOG_TRACE(Service_FS, "Opening user NAND content directory");

    if (bis_factory == nullptr)
        return nullptr;

    return bis_factory->GetUserNANDContentDirectory();
}

FileSys::VirtualDir FileSystemController::GetSDMCContentDirectory() const {
    LOG_TRACE(Service_FS, "Opening SDMC content directory");

    if (sdmc_factory == nullptr)
        return nullptr;

    return sdmc_factory->GetSDMCContentDirectory();
}

FileSys::VirtualDir FileSystemController::GetNANDImageDirectory() const {
    LOG_TRACE(Service_FS, "Opening NAND image directory");

    if (bis_factory == nullptr)
        return nullptr;

    return bis_factory->GetImageDirectory();
}

FileSys::VirtualDir FileSystemController::GetSDMCImageDirectory() const {
    LOG_TRACE(Service_FS, "Opening SDMC image directory");

    if (sdmc_factory == nullptr)
        return nullptr;

    return sdmc_factory->GetImageDirectory();
}

FileSys::VirtualDir FileSystemController::GetContentDirectory(ContentStorageId id) const {
    switch (id) {
    case ContentStorageId::System:
        return GetSystemNANDContentDirectory();
    case ContentStorageId::User:
        return GetUserNANDContentDirectory();
    case ContentStorageId::SdCard:
        return GetSDMCContentDirectory();
    }

    return nullptr;
}

FileSys::VirtualDir FileSystemController::GetImageDirectory(ImageDirectoryId id) const {
    switch (id) {
    case ImageDirectoryId::NAND:
        return GetNANDImageDirectory();
    case ImageDirectoryId::SdCard:
        return GetSDMCImageDirectory();
    }

    return nullptr;
}

FileSys::VirtualDir FileSystemController::GetModificationLoadRoot(u64 title_id) const {
    LOG_TRACE(Service_FS, "Opening mod load root for tid={:016X}", title_id);

    if (bis_factory == nullptr)
        return nullptr;

    return bis_factory->GetModificationLoadRoot(title_id);
}

FileSys::VirtualDir FileSystemController::GetSDMCModificationLoadRoot(u64 title_id) const {
    LOG_TRACE(Service_FS, "Opening SDMC mod load root for tid={:016X}", title_id);

    if (sdmc_factory == nullptr) {
        return nullptr;
    }

    return sdmc_factory->GetSDMCModificationLoadRoot(title_id);
}

FileSys::VirtualDir FileSystemController::GetModificationDumpRoot(u64 title_id) const {
    LOG_TRACE(Service_FS, "Opening mod dump root for tid={:016X}", title_id);

    if (bis_factory == nullptr)
        return nullptr;

    return bis_factory->GetModificationDumpRoot(title_id);
}

FileSys::VirtualDir FileSystemController::GetBCATDirectory(u64 title_id) const {
    LOG_TRACE(Service_FS, "Opening BCAT root for tid={:016X}", title_id);

    if (bis_factory == nullptr)
        return nullptr;

    return bis_factory->GetBCATDirectory(title_id);
}

void FileSystemController::CreateFactories(FileSys::VfsFilesystem& vfs, bool overwrite) {
    if (overwrite) {
        bis_factory = nullptr;
        sdmc_factory = nullptr;
    }

    using SudachiPath = Common::FS::SudachiPath;
    const auto sdmc_dir_path = Common::FS::GetSudachiPath(SudachiPath::SDMCDir);
    const auto sdmc_load_dir_path = sdmc_dir_path / "atmosphere/contents";
    const auto rw_mode = FileSys::OpenMode::ReadWrite;

    auto nand_directory =
        vfs.OpenDirectory(Common::FS::GetSudachiPathString(SudachiPath::NANDDir), rw_mode);
    auto sd_directory = vfs.OpenDirectory(Common::FS::PathToUTF8String(sdmc_dir_path), rw_mode);
    auto load_directory = vfs.OpenDirectory(Common::FS::GetSudachiPathString(SudachiPath::LoadDir),
                                            FileSys::OpenMode::Read);
    auto sd_load_directory = vfs.OpenDirectory(Common::FS::PathToUTF8String(sdmc_load_dir_path),
                                               FileSys::OpenMode::Read);
    auto dump_directory =
        vfs.OpenDirectory(Common::FS::GetSudachiPathString(SudachiPath::DumpDir), rw_mode);

    if (bis_factory == nullptr) {
        bis_factory = std::make_unique<FileSys::BISFactory>(
            nand_directory, std::move(load_directory), std::move(dump_directory));
        system.RegisterContentProvider(FileSys::ContentProviderUnionSlot::SysNAND,
                                       bis_factory->GetSystemNANDContents());
        system.RegisterContentProvider(FileSys::ContentProviderUnionSlot::UserNAND,
                                       bis_factory->GetUserNANDContents());
    }

    if (sdmc_factory == nullptr) {
        sdmc_factory = std::make_unique<FileSys::SDMCFactory>(std::move(sd_directory),
                                                              std::move(sd_load_directory));
        system.RegisterContentProvider(FileSys::ContentProviderUnionSlot::SDMC,
                                       sdmc_factory->GetSDMCContents());
    }
}

void FileSystemController::Reset() {
    std::scoped_lock lk{registration_lock};
    registrations.clear();
}

void LoopProcess(Core::System& system) {
    auto server_manager = std::make_unique<ServerManager>(system);

    const auto FileSystemProxyFactory = [&] { return std::make_shared<FSP_SRV>(system); };

    server_manager->RegisterNamedService("fsp-ldr", std::make_shared<FSP_LDR>(system));
    server_manager->RegisterNamedService("fsp:pr", std::make_shared<FSP_PR>(system));
    server_manager->RegisterNamedService("fsp-srv", std::move(FileSystemProxyFactory));
    ServerManager::RunServer(std::move(server_manager));
}

} // namespace Service::FileSystem
