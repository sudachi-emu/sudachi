// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/assert.h"
#include "common/common_funcs.h"
#include "common/common_types.h"

namespace Service::RO {

enum class NrrKind : u8 {
    User = 0,
    JitPlugin = 1,
    Count,
};

static constexpr size_t ModuleIdSize = 0x20;
struct ModuleId {
    std::array<u8, ModuleIdSize> data;
};
static_assert(sizeof(ModuleId) == ModuleIdSize);

struct NrrCertification {
    static constexpr size_t RsaKeySize = 0x100;
    static constexpr size_t SignedSize = 0x120;

    u64 program_id_mask;
    u64 program_id_pattern;
    std::array<u8, 0x10> reserved_10;
    std::array<u8, RsaKeySize> modulus;
    std::array<u8, RsaKeySize> signature;
};
static_assert(sizeof(NrrCertification) ==
              NrrCertification::RsaKeySize + NrrCertification::SignedSize);

class NrrHeader {
public:
    static constexpr u32 Magic = Common::MakeMagic('N', 'R', 'R', '0');

public:
    bool IsMagicValid() const {
        return m_magic == Magic;
    }

    bool IsProgramIdValid() const {
        return (m_program_id & m_certification.program_id_mask) ==
               m_certification.program_id_pattern;
    }

    NrrKind GetNrrKind() const {
        const NrrKind kind = static_cast<NrrKind>(m_nrr_kind);
        ASSERT(kind < NrrKind::Count);
        return kind;
    }

    u64 GetProgramId() const {
        return m_program_id;
    }

    u32 GetSize() const {
        return m_size;
    }

    u32 GetNumHashes() const {
        return m_num_hashes;
    }

    size_t GetHashesOffset() const {
        return m_hashes_offset;
    }

    u32 GetKeyGeneration() const {
        return m_key_generation;
    }

    const u8* GetCertificationSignature() const {
        return m_certification.signature.data();
    }

    const u8* GetCertificationSignedArea() const {
        return reinterpret_cast<const u8*>(std::addressof(m_certification));
    }

    const u8* GetCertificationModulus() const {
        return m_certification.modulus.data();
    }

    const u8* GetSignature() const {
        return m_signature.data();
    }

    size_t GetSignedAreaSize() const {
        return m_size - GetSignedAreaOffset();
    }

    static constexpr size_t GetSignedAreaOffset() {
        return offsetof(NrrHeader, m_program_id);
    }

private:
    u32 m_magic;
    u32 m_key_generation;
    INSERT_PADDING_BYTES_NOINIT(8);
    NrrCertification m_certification;
    std::array<u8, 0x100> m_signature;
    u64 m_program_id;
    u32 m_size;
    u8 m_nrr_kind; // 7.0.0+
    INSERT_PADDING_BYTES_NOINIT(3);
    u32 m_hashes_offset;
    u32 m_num_hashes;
    INSERT_PADDING_BYTES_NOINIT(8);
};
static_assert(sizeof(NrrHeader) == 0x350, "NrrHeader has wrong size");

class NroHeader {
public:
    static constexpr u32 Magic = Common::MakeMagic('N', 'R', 'O', '0');

public:
    bool IsMagicValid() const {
        return m_magic == Magic;
    }

    u32 GetSize() const {
        return m_size;
    }

    u32 GetTextOffset() const {
        return m_text_offset;
    }

    u32 GetTextSize() const {
        return m_text_size;
    }

    u32 GetRoOffset() const {
        return m_ro_offset;
    }

    u32 GetRoSize() const {
        return m_ro_size;
    }

    u32 GetRwOffset() const {
        return m_rw_offset;
    }

    u32 GetRwSize() const {
        return m_rw_size;
    }

    u32 GetBssSize() const {
        return m_bss_size;
    }

    const ModuleId* GetModuleId() const {
        return std::addressof(m_module_id);
    }

private:
    u32 m_entrypoint_insn;
    u32 m_mod_offset;
    INSERT_PADDING_BYTES_NOINIT(0x8);
    u32 m_magic;
    INSERT_PADDING_BYTES_NOINIT(0x4);
    u32 m_size;
    INSERT_PADDING_BYTES_NOINIT(0x4);
    u32 m_text_offset;
    u32 m_text_size;
    u32 m_ro_offset;
    u32 m_ro_size;
    u32 m_rw_offset;
    u32 m_rw_size;
    u32 m_bss_size;
    INSERT_PADDING_BYTES_NOINIT(0x4);
    ModuleId m_module_id;
    INSERT_PADDING_BYTES_NOINIT(0x20);
};
static_assert(sizeof(NroHeader) == 0x80, "NroHeader has wrong size");

} // namespace Service::RO
