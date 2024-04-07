// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "common/overflow.h"
#include "core/file_sys/errors.h"
#include "core/file_sys/vfs/vfs.h"

namespace FileSys {

class IStorage : public VfsFile {
public:
    virtual std::string GetName() const override {
        return {};
    }

    virtual VirtualDir GetContainingDirectory() const override {
        return {};
    }

    virtual bool IsWritable() const override {
        return true;
    }

    virtual bool IsReadable() const override {
        return true;
    }

    virtual bool Resize(size_t size) override {
        return false;
    }

    virtual bool Rename(std::string_view name) override {
        return false;
    }

    static inline Result CheckAccessRange(s64 offset, s64 size, s64 total_size) {
        R_UNLESS(offset >= 0, ResultInvalidOffset);
        R_UNLESS(size >= 0, ResultInvalidSize);
        R_UNLESS(Common::WrappingAdd(offset, size) >= offset, ResultOutOfRange);
        R_UNLESS(offset + size <= total_size, ResultOutOfRange);
        R_SUCCEED();
    }
};

class IReadOnlyStorage : public IStorage {
public:
    virtual bool IsWritable() const override {
        return false;
    }

    virtual size_t Write(const u8* buffer, size_t size, size_t offset) override {
        return 0;
    }
};

} // namespace FileSys
