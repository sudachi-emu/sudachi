// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/arm64/native_clock.h"
#include "common/bit_cast.h"
#include "common/literals.h"
#include "core/arm/nce/arm_nce.h"
#include "core/arm/nce/guest_context.h"
#include "core/arm/nce/instructions.h"
#include "core/arm/nce/patch.h"
#include "core/core.h"
#include "core/core_timing.h"
#include "core/hle/kernel/svc.h"

namespace Core::NCE {

using namespace Common::Literals;
using namespace oaknut::util;

using NativeExecutionParameters = Kernel::KThread::NativeExecutionParameters;

constexpr size_t MaxRelativeBranch = 128_MiB;
constexpr u32 ModuleCodeIndex = 0x24 / sizeof(u32);

Patcher::Patcher() : c(m_patch_instructions) {}

Patcher::~Patcher() = default;

void Patcher::PatchText(const Kernel::PhysicalMemory& program_image,
                        const Kernel::CodeSet::Segment& code) {

    // Write save context helper function.
    c.l(m_save_context);
    WriteSaveContext();

    // Write load context helper function.
    c.l(m_load_context);
    WriteLoadContext();

    // Retrieve text segment data.
    const auto text = std::span{program_image}.subspan(code.offset, code.size);
    const auto text_words =
        std::span<const u32>{reinterpret_cast<const u32*>(text.data()), text.size() / sizeof(u32)};

    // Loop through instructions, patching as needed.
    for (u32 i = ModuleCodeIndex; i < static_cast<u32>(text_words.size()); i++) {
        const u32 inst = text_words[i];

        const auto AddRelocations = [&] {
            const uintptr_t this_offset = i * sizeof(u32);
            const uintptr_t next_offset = this_offset + sizeof(u32);

            // Relocate from here to patch.
            this->BranchToPatch(this_offset);

            // Relocate from patch to next instruction.
            return next_offset;
        };

        // SVC
        if (auto svc = SVC{inst}; svc.Verify()) {
            WriteSvcTrampoline(AddRelocations(), svc.GetValue());
            continue;
        }

        // MRS Xn, TPIDR_EL0
        // MRS Xn, TPIDRRO_EL0
        if (auto mrs = MRS{inst};
            mrs.Verify() && (mrs.GetSystemReg() == TpidrroEl0 || mrs.GetSystemReg() == TpidrEl0)) {
            const auto src_reg = mrs.GetSystemReg() == TpidrroEl0 ? oaknut::SystemReg::TPIDRRO_EL0
                                                                  : oaknut::SystemReg::TPIDR_EL0;
            const auto dest_reg = oaknut::XReg{static_cast<int>(mrs.GetRt())};
            WriteMrsHandler(AddRelocations(), dest_reg, src_reg);
            continue;
        }

        // MRS Xn, CNTPCT_EL0
        if (auto mrs = MRS{inst}; mrs.Verify() && mrs.GetSystemReg() == CntpctEl0) {
            WriteCntpctHandler(AddRelocations(), oaknut::XReg{static_cast<int>(mrs.GetRt())});
            continue;
        }

        // MRS Xn, CNTFRQ_EL0
        if (auto mrs = MRS{inst}; mrs.Verify() && mrs.GetSystemReg() == CntfrqEl0) {
            UNREACHABLE();
        }

        // MSR TPIDR_EL0, Xn
        if (auto msr = MSR{inst}; msr.Verify() && msr.GetSystemReg() == TpidrEl0) {
            WriteMsrHandler(AddRelocations(), oaknut::XReg{static_cast<int>(msr.GetRt())});
            continue;
        }

        if (auto exclusive = Exclusive{inst}; exclusive.Verify()) {
            m_exclusives.push_back(i);
        }
    }

    // Determine patching mode for the final relocation step
    const size_t image_size = program_image.size();
    this->mode = image_size > MaxRelativeBranch ? PatchMode::PreText : PatchMode::PostData;
}

void Patcher::RelocateAndCopy(Common::ProcessAddress load_base,
                              const Kernel::CodeSet::Segment& code,
                              Kernel::PhysicalMemory& program_image,
                              EntryTrampolines* out_trampolines) {
    const size_t patch_size = GetSectionSize();
    const size_t image_size = program_image.size();

    // Retrieve text segment data.
    const auto text = std::span{program_image}.subspan(code.offset, code.size);
    const auto text_words =
        std::span<u32>{reinterpret_cast<u32*>(text.data()), text.size() / sizeof(u32)};

    const auto ApplyBranchToPatchRelocation = [&](u32* target, const Relocation& rel) {
        oaknut::CodeGenerator rc{target};
        if (mode == PatchMode::PreText) {
            rc.B(rel.patch_offset - patch_size - rel.module_offset);
        } else {
            rc.B(image_size - rel.module_offset + rel.patch_offset);
        }
    };

    const auto ApplyBranchToModuleRelocation = [&](u32* target, const Relocation& rel) {
        oaknut::CodeGenerator rc{target};
        if (mode == PatchMode::PreText) {
            rc.B(patch_size - rel.patch_offset + rel.module_offset);
        } else {
            rc.B(rel.module_offset - image_size - rel.patch_offset);
        }
    };

    const auto RebasePatch = [&](ptrdiff_t patch_offset) {
        if (mode == PatchMode::PreText) {
            return GetInteger(load_base) + patch_offset;
        } else {
            return GetInteger(load_base) + image_size + patch_offset;
        }
    };

    const auto RebasePc = [&](uintptr_t module_offset) {
        if (mode == PatchMode::PreText) {
            return GetInteger(load_base) + patch_size + module_offset;
        } else {
            return GetInteger(load_base) + module_offset;
        }
    };

    // We are now ready to relocate!
    for (const Relocation& rel : m_branch_to_patch_relocations) {
        ApplyBranchToPatchRelocation(text_words.data() + rel.module_offset / sizeof(u32), rel);
    }
    for (const Relocation& rel : m_branch_to_module_relocations) {
        ApplyBranchToModuleRelocation(m_patch_instructions.data() + rel.patch_offset / sizeof(u32),
                                      rel);
    }

    // Rewrite PC constants and record post trampolines
    for (const Relocation& rel : m_write_module_pc_relocations) {
        oaknut::CodeGenerator rc{m_patch_instructions.data() + rel.patch_offset / sizeof(u32)};
        rc.dx(RebasePc(rel.module_offset));
    }
    for (const Trampoline& rel : m_trampolines) {
        out_trampolines->insert({RebasePc(rel.module_offset), RebasePatch(rel.patch_offset)});
    }

    // Cortex-A57 seems to treat all exclusives as ordered, but newer processors do not.
    // Convert to ordered to preserve this assumption.
    for (const ModuleTextAddress i : m_exclusives) {
        auto exclusive = Exclusive{text_words[i]};
        text_words[i] = exclusive.AsOrdered();
    }

    // Copy to program image
    if (this->mode == PatchMode::PreText) {
        std::memcpy(program_image.data(), m_patch_instructions.data(),
                    m_patch_instructions.size() * sizeof(u32));
    } else {
        program_image.resize(image_size + patch_size);
        std::memcpy(program_image.data() + image_size, m_patch_instructions.data(),
                    m_patch_instructions.size() * sizeof(u32));
    }
}

size_t Patcher::GetSectionSize() const noexcept {
    return Common::AlignUp(m_patch_instructions.size() * sizeof(u32),
                           Core::Memory::SUDACHI_PAGESIZE);
}

void Patcher::WriteLoadContext() {
    // This function was called, which modifies X30, so use that as a scratch register.
    // SP contains the guest X30, so save our return X30 to SP + 8, since we have allocated 16 bytes
    // of stack.
    c.STR(X30, SP, 8);
    c.MRS(X30, oaknut::SystemReg::TPIDR_EL0);
    c.LDR(X30, X30, offsetof(NativeExecutionParameters, native_context));

    // Load system registers.
    c.LDR(W0, X30, offsetof(GuestContext, fpsr));
    c.MSR(oaknut::SystemReg::FPSR, X0);
    c.LDR(W0, X30, offsetof(GuestContext, fpcr));
    c.MSR(oaknut::SystemReg::FPCR, X0);
    c.LDR(W0, X30, offsetof(GuestContext, nzcv));
    c.MSR(oaknut::SystemReg::NZCV, X0);

    // Load all vector registers.
    static constexpr size_t VEC_OFF = offsetof(GuestContext, vector_registers);
    for (int i = 0; i <= 30; i += 2) {
        c.LDP(oaknut::QReg{i}, oaknut::QReg{i + 1}, X30, VEC_OFF + 16 * i);
    }

    // Load all general-purpose registers except X30.
    for (int i = 0; i <= 28; i += 2) {
        c.LDP(oaknut::XReg{i}, oaknut::XReg{i + 1}, X30, 8 * i);
    }

    // Reload our return X30 from the stack and return.
    // The patch code will reload the guest X30 for us.
    c.LDR(X30, SP, 8);
    c.RET();
}

void Patcher::WriteSaveContext() {
    // This function was called, which modifies X30, so use that as a scratch register.
    // SP contains the guest X30, so save our X30 to SP + 8, since we have allocated 16 bytes of
    // stack.
    c.STR(X30, SP, 8);
    c.MRS(X30, oaknut::SystemReg::TPIDR_EL0);
    c.LDR(X30, X30, offsetof(NativeExecutionParameters, native_context));

    // Store all general-purpose registers except X30.
    for (int i = 0; i <= 28; i += 2) {
        c.STP(oaknut::XReg{i}, oaknut::XReg{i + 1}, X30, 8 * i);
    }

    // Store all vector registers.
    static constexpr size_t VEC_OFF = offsetof(GuestContext, vector_registers);
    for (int i = 0; i <= 30; i += 2) {
        c.STP(oaknut::QReg{i}, oaknut::QReg{i + 1}, X30, VEC_OFF + 16 * i);
    }

    // Store guest system registers, X30 and SP, using X0 as a scratch register.
    c.STR(X0, SP, PRE_INDEXED, -16);
    c.LDR(X0, SP, 16);
    c.STR(X0, X30, 8 * 30);
    c.ADD(X0, SP, 32);
    c.STR(X0, X30, offsetof(GuestContext, sp));
    c.MRS(X0, oaknut::SystemReg::FPSR);
    c.STR(W0, X30, offsetof(GuestContext, fpsr));
    c.MRS(X0, oaknut::SystemReg::FPCR);
    c.STR(W0, X30, offsetof(GuestContext, fpcr));
    c.MRS(X0, oaknut::SystemReg::NZCV);
    c.STR(W0, X30, offsetof(GuestContext, nzcv));
    c.LDR(X0, SP, POST_INDEXED, 16);

    // Reload our return X30 from the stack, and return.
    c.LDR(X30, SP, 8);
    c.RET();
}

void Patcher::WriteSvcTrampoline(ModuleDestLabel module_dest, u32 svc_id) {
    // We are about to start saving state, so we need to lock the context.
    this->LockContext();

    // Store guest X30 to the stack. Then, save the context and restore the stack.
    // This will save all registers except PC, but we know PC at patch time.
    c.STR(X30, SP, PRE_INDEXED, -16);
    c.BL(m_save_context);
    c.LDR(X30, SP, POST_INDEXED, 16);

    // Now that we've saved all registers, we can use any registers as scratch.
    // Store PC + 4 to arm interface, since we know the instruction offset from the entry point.
    oaknut::Label pc_after_svc;
    c.MRS(X1, oaknut::SystemReg::TPIDR_EL0);
    c.LDR(X1, X1, offsetof(NativeExecutionParameters, native_context));
    c.LDR(X2, pc_after_svc);
    c.STR(X2, X1, offsetof(GuestContext, pc));

    // Store SVC number to execute when we return
    c.MOV(X2, svc_id);
    c.STR(W2, X1, offsetof(GuestContext, svc_swi));

    // We are calling a SVC. Clear esr_el1 and return it.
    static_assert(std::is_same_v<std::underlying_type_t<HaltReason>, u64>);
    oaknut::Label retry;
    c.ADD(X2, X1, offsetof(GuestContext, esr_el1));
    c.l(retry);
    c.LDAXR(X0, X2);
    c.STLXR(W3, XZR, X2);
    c.CBNZ(W3, retry);

    // Add "calling SVC" flag. Since this is X0, this is now our return value.
    c.ORR(X0, X0, static_cast<u64>(HaltReason::SupervisorCall));

    // Offset the GuestContext pointer to the HostContext member.
    // STP has limited range of [-512, 504] which we can't reach otherwise
    // NB: Due to this all offsets below are from the start of HostContext.
    c.ADD(X1, X1, offsetof(GuestContext, host_ctx));

    // Reload host TPIDR_EL0 and SP.
    static_assert(offsetof(HostContext, host_sp) + 8 == offsetof(HostContext, host_tpidr_el0));
    c.LDP(X2, X3, X1, offsetof(HostContext, host_sp));
    c.MOV(SP, X2);
    c.MSR(oaknut::SystemReg::TPIDR_EL0, X3);

    // Load callee-saved host registers and return to host.
    static constexpr size_t HOST_REGS_OFF = offsetof(HostContext, host_saved_regs);
    static constexpr size_t HOST_VREGS_OFF = offsetof(HostContext, host_saved_vregs);
    c.LDP(X19, X20, X1, HOST_REGS_OFF);
    c.LDP(X21, X22, X1, HOST_REGS_OFF + 2 * sizeof(u64));
    c.LDP(X23, X24, X1, HOST_REGS_OFF + 4 * sizeof(u64));
    c.LDP(X25, X26, X1, HOST_REGS_OFF + 6 * sizeof(u64));
    c.LDP(X27, X28, X1, HOST_REGS_OFF + 8 * sizeof(u64));
    c.LDP(X29, X30, X1, HOST_REGS_OFF + 10 * sizeof(u64));
    c.LDP(Q8, Q9, X1, HOST_VREGS_OFF);
    c.LDP(Q10, Q11, X1, HOST_VREGS_OFF + 2 * sizeof(u128));
    c.LDP(Q12, Q13, X1, HOST_VREGS_OFF + 4 * sizeof(u128));
    c.LDP(Q14, Q15, X1, HOST_VREGS_OFF + 6 * sizeof(u128));
    c.RET();

    // Write the post-SVC trampoline address, which will jump back to the guest after restoring its
    // state.
    m_trampolines.push_back({c.offset(), module_dest});

    // Host called this location. Save the return address so we can
    // unwind the stack properly when jumping back.
    c.MRS(X2, oaknut::SystemReg::TPIDR_EL0);
    c.LDR(X2, X2, offsetof(NativeExecutionParameters, native_context));
    c.ADD(X0, X2, offsetof(GuestContext, host_ctx));
    c.STR(X30, X0, offsetof(HostContext, host_saved_regs) + 11 * sizeof(u64));

    // Reload all guest registers except X30 and PC.
    // The function also expects 16 bytes of stack already allocated.
    c.STR(X30, SP, PRE_INDEXED, -16);
    c.BL(m_load_context);
    c.LDR(X30, SP, POST_INDEXED, 16);

    // Use X1 as a scratch register to restore X30.
    c.STR(X1, SP, PRE_INDEXED, -16);
    c.MRS(X1, oaknut::SystemReg::TPIDR_EL0);
    c.LDR(X1, X1, offsetof(NativeExecutionParameters, native_context));
    c.LDR(X30, X1, offsetof(GuestContext, cpu_registers) + sizeof(u64) * 30);
    c.LDR(X1, SP, POST_INDEXED, 16);

    // Unlock the context.
    this->UnlockContext();

    // Jump back to the instruction after the emulated SVC.
    this->BranchToModule(module_dest);

    // Store PC after call.
    c.l(pc_after_svc);
    this->WriteModulePc(module_dest);
}

void Patcher::WriteMrsHandler(ModuleDestLabel module_dest, oaknut::XReg dest_reg,
                              oaknut::SystemReg src_reg) {
    // Retrieve emulated TLS register from GuestContext.
    c.MRS(dest_reg, oaknut::SystemReg::TPIDR_EL0);
    if (src_reg == oaknut::SystemReg::TPIDRRO_EL0) {
        c.LDR(dest_reg, dest_reg, offsetof(NativeExecutionParameters, tpidrro_el0));
    } else {
        c.LDR(dest_reg, dest_reg, offsetof(NativeExecutionParameters, tpidr_el0));
    }

    // Jump back to the instruction after the emulated MRS.
    this->BranchToModule(module_dest);
}

void Patcher::WriteMsrHandler(ModuleDestLabel module_dest, oaknut::XReg src_reg) {
    const auto scratch_reg = src_reg.index() == 0 ? X1 : X0;
    c.STR(scratch_reg, SP, PRE_INDEXED, -16);

    // Save guest value to NativeExecutionParameters::tpidr_el0.
    c.MRS(scratch_reg, oaknut::SystemReg::TPIDR_EL0);
    c.STR(src_reg, scratch_reg, offsetof(NativeExecutionParameters, tpidr_el0));

    // Restore scratch register.
    c.LDR(scratch_reg, SP, POST_INDEXED, 16);

    // Jump back to the instruction after the emulated MSR.
    this->BranchToModule(module_dest);
}

void Patcher::WriteCntpctHandler(ModuleDestLabel module_dest, oaknut::XReg dest_reg) {
    static Common::Arm64::NativeClock clock{};
    const auto factor = clock.GetGuestCNTFRQFactor();
    const auto raw_factor = Common::BitCast<std::array<u64, 2>>(factor);

    const auto use_x2_x3 = dest_reg.index() == 0 || dest_reg.index() == 1;
    oaknut::XReg scratch0 = use_x2_x3 ? X2 : X0;
    oaknut::XReg scratch1 = use_x2_x3 ? X3 : X1;

    oaknut::Label factorlo;
    oaknut::Label factorhi;

    // Save scratches.
    c.STP(scratch0, scratch1, SP, PRE_INDEXED, -16);

    // Load counter value.
    c.MRS(dest_reg, oaknut::SystemReg::CNTVCT_EL0);

    // Load scaling factor.
    c.LDR(scratch0, factorlo);
    c.LDR(scratch1, factorhi);

    // Multiply low bits and get result.
    c.UMULH(scratch0, dest_reg, scratch0);

    // Multiply high bits and add low bit result.
    c.MADD(dest_reg, dest_reg, scratch1, scratch0);

    // Reload scratches.
    c.LDP(scratch0, scratch1, SP, POST_INDEXED, 16);

    // Jump back to the instruction after the emulated MRS.
    this->BranchToModule(module_dest);

    // Scaling factor constant values.
    c.l(factorlo);
    c.dx(raw_factor[0]);
    c.l(factorhi);
    c.dx(raw_factor[1]);
}

void Patcher::LockContext() {
    oaknut::Label retry;

    // Save scratches.
    c.STP(X0, X1, SP, PRE_INDEXED, -16);

    // Reload lock pointer.
    c.l(retry);
    c.CLREX();
    c.MRS(X0, oaknut::SystemReg::TPIDR_EL0);
    c.ADD(X0, X0, offsetof(NativeExecutionParameters, lock));

    static_assert(SpinLockLocked == 0);

    // Load-linked with acquire ordering.
    c.LDAXR(W1, X0);

    // If the value was SpinLockLocked, clear monitor and retry.
    c.CBZ(W1, retry);

    // Store-conditional SpinLockLocked with relaxed ordering.
    c.STXR(W1, WZR, X0);

    // If we failed to store, retry.
    c.CBNZ(W1, retry);

    // We succeeded! Reload scratches.
    c.LDP(X0, X1, SP, POST_INDEXED, 16);
}

void Patcher::UnlockContext() {
    // Save scratches.
    c.STP(X0, X1, SP, PRE_INDEXED, -16);

    // Load lock pointer.
    c.MRS(X0, oaknut::SystemReg::TPIDR_EL0);
    c.ADD(X0, X0, offsetof(NativeExecutionParameters, lock));

    // Load SpinLockUnlocked.
    c.MOV(W1, SpinLockUnlocked);

    // Store value with release ordering.
    c.STLR(W1, X0);

    // Load scratches.
    c.LDP(X0, X1, SP, POST_INDEXED, 16);
}

} // namespace Core::NCE
