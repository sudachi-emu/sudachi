// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/file_sys/fssystem/fssystem_nca_header.h"

namespace FileSys {

u8 NcaHeader::GetProperKeyGeneration() const {
    return std::max(this->key_generation, this->key_generation_2);
}

bool NcaPatchInfo::HasIndirectTable() const {
    return this->indirect_size != 0;
}

bool NcaPatchInfo::HasAesCtrExTable() const {
    return this->aes_ctr_ex_size != 0;
}

} // namespace FileSys
