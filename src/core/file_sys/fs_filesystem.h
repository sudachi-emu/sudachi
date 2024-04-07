// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "common/common_funcs.h"
#include "common/common_types.h"

namespace FileSys {

enum class OpenMode : u32 {
    Read = (1 << 0),
    Write = (1 << 1),
    AllowAppend = (1 << 2),

    ReadWrite = (Read | Write),
    All = (ReadWrite | AllowAppend),
};
DECLARE_ENUM_FLAG_OPERATORS(OpenMode)

enum class OpenDirectoryMode : u64 {
    Directory = (1 << 0),
    File = (1 << 1),

    All = (Directory | File),

    NotRequireFileSize = (1ULL << 31),
};
DECLARE_ENUM_FLAG_OPERATORS(OpenDirectoryMode)

enum class DirectoryEntryType : u8 {
    Directory = 0,
    File = 1,
};

enum class CreateOption : u8 {
    None = (0 << 0),
    BigFile = (1 << 0),
};

struct FileSystemAttribute {
    u8 dir_entry_name_length_max_defined;
    u8 file_entry_name_length_max_defined;
    u8 dir_path_name_length_max_defined;
    u8 file_path_name_length_max_defined;
    INSERT_PADDING_BYTES_NOINIT(0x5);
    u8 utf16_dir_entry_name_length_max_defined;
    u8 utf16_file_entry_name_length_max_defined;
    u8 utf16_dir_path_name_length_max_defined;
    u8 utf16_file_path_name_length_max_defined;
    INSERT_PADDING_BYTES_NOINIT(0x18);
    s32 dir_entry_name_length_max;
    s32 file_entry_name_length_max;
    s32 dir_path_name_length_max;
    s32 file_path_name_length_max;
    INSERT_PADDING_WORDS_NOINIT(0x5);
    s32 utf16_dir_entry_name_length_max;
    s32 utf16_file_entry_name_length_max;
    s32 utf16_dir_path_name_length_max;
    s32 utf16_file_path_name_length_max;
    INSERT_PADDING_WORDS_NOINIT(0x18);
    INSERT_PADDING_WORDS_NOINIT(0x1);
};
static_assert(sizeof(FileSystemAttribute) == 0xC0, "FileSystemAttribute has incorrect size");

} // namespace FileSys
