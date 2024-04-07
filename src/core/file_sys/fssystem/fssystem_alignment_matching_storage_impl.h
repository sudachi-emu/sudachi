// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "core/file_sys/errors.h"
#include "core/file_sys/fssystem/fs_i_storage.h"

namespace FileSys {

class AlignmentMatchingStorageImpl {
public:
    static size_t Read(VirtualFile base_storage, char* work_buf, size_t work_buf_size,
                       size_t data_alignment, size_t buffer_alignment, s64 offset, u8* buffer,
                       size_t size);
    static size_t Write(VirtualFile base_storage, char* work_buf, size_t work_buf_size,
                        size_t data_alignment, size_t buffer_alignment, s64 offset,
                        const u8* buffer, size_t size);
};

} // namespace FileSys
