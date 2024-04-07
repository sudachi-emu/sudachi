// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-FileCopyrightText: Copyright 2023 merryhime <https://mary.rs>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <signal.h>
#include <unistd.h>

#include "core/arm/nce/visitor_base.h"

namespace Core {

namespace Memory {
class Memory;
}

class InterpreterVisitor final : public VisitorBase {
public:
    explicit InterpreterVisitor(Core::Memory::Memory& memory, std::span<u64, 31> regs,
                                std::span<u128, 32> fpsimd_regs, u64& sp, const u64& pc)
        : m_memory(memory), m_regs(regs), m_fpsimd_regs(fpsimd_regs), m_sp(sp), m_pc(pc) {}
    ~InterpreterVisitor() override = default;

    enum class MemOp {
        Load,
        Store,
        Prefetch,
    };

    u128 GetVec(Vec v);
    u64 GetReg(Reg r);
    u64 GetSp();
    u64 GetPc();

    void SetVec(Vec v, u128 value);
    void SetReg(Reg r, u64 value);
    void SetSp(u64 value);

    u64 ExtendReg(size_t bitsize, Reg reg, Imm<3> option, u8 shift);

    // Loads and stores - Load/Store Exclusive
    bool Ordered(size_t size, bool L, bool o0, Reg Rn, Reg Rt);
    bool STLLR(Imm<2> size, Reg Rn, Reg Rt) override;
    bool STLR(Imm<2> size, Reg Rn, Reg Rt) override;
    bool LDLAR(Imm<2> size, Reg Rn, Reg Rt) override;
    bool LDAR(Imm<2> size, Reg Rn, Reg Rt) override;

    // Loads and stores - Load register (literal)
    bool LDR_lit_gen(bool opc_0, Imm<19> imm19, Reg Rt) override;
    bool LDR_lit_fpsimd(Imm<2> opc, Imm<19> imm19, Vec Vt) override;

    // Loads and stores - Load/Store register pair
    bool STP_LDP_gen(Imm<2> opc, bool not_postindex, bool wback, Imm<1> L, Imm<7> imm7, Reg Rt2,
                     Reg Rn, Reg Rt) override;
    bool STP_LDP_fpsimd(Imm<2> opc, bool not_postindex, bool wback, Imm<1> L, Imm<7> imm7, Vec Vt2,
                        Reg Rn, Vec Vt) override;

    // Loads and stores - Load/Store register (immediate)
    bool RegisterImmediate(bool wback, bool postindex, size_t scale, u64 offset, Imm<2> size,
                           Imm<2> opc, Reg Rn, Reg Rt);
    bool STRx_LDRx_imm_1(Imm<2> size, Imm<2> opc, Imm<9> imm9, bool not_postindex, Reg Rn,
                         Reg Rt) override;
    bool STRx_LDRx_imm_2(Imm<2> size, Imm<2> opc, Imm<12> imm12, Reg Rn, Reg Rt) override;
    bool STURx_LDURx(Imm<2> size, Imm<2> opc, Imm<9> imm9, Reg Rn, Reg Rt) override;

    bool SIMDImmediate(bool wback, bool postindex, size_t scale, u64 offset, MemOp memop, Reg Rn,
                       Vec Vt);
    bool STR_imm_fpsimd_1(Imm<2> size, Imm<1> opc_1, Imm<9> imm9, bool not_postindex, Reg Rn,
                          Vec Vt) override;
    bool STR_imm_fpsimd_2(Imm<2> size, Imm<1> opc_1, Imm<12> imm12, Reg Rn, Vec Vt) override;
    bool LDR_imm_fpsimd_1(Imm<2> size, Imm<1> opc_1, Imm<9> imm9, bool not_postindex, Reg Rn,
                          Vec Vt) override;
    bool LDR_imm_fpsimd_2(Imm<2> size, Imm<1> opc_1, Imm<12> imm12, Reg Rn, Vec Vt) override;
    bool STUR_fpsimd(Imm<2> size, Imm<1> opc_1, Imm<9> imm9, Reg Rn, Vec Vt) override;
    bool LDUR_fpsimd(Imm<2> size, Imm<1> opc_1, Imm<9> imm9, Reg Rn, Vec Vt) override;

    // Loads and stores - Load/Store register (register offset)
    bool RegisterOffset(size_t scale, u8 shift, Imm<2> size, Imm<1> opc_1, Imm<1> opc_0, Reg Rm,
                        Imm<3> option, Reg Rn, Reg Rt);
    bool STRx_reg(Imm<2> size, Imm<1> opc_1, Reg Rm, Imm<3> option, bool S, Reg Rn,
                  Reg Rt) override;
    bool LDRx_reg(Imm<2> size, Imm<1> opc_1, Reg Rm, Imm<3> option, bool S, Reg Rn,
                  Reg Rt) override;

    bool SIMDOffset(size_t scale, u8 shift, Imm<1> opc_0, Reg Rm, Imm<3> option, Reg Rn, Vec Vt);
    bool STR_reg_fpsimd(Imm<2> size, Imm<1> opc_1, Reg Rm, Imm<3> option, bool S, Reg Rn,
                        Vec Vt) override;
    bool LDR_reg_fpsimd(Imm<2> size, Imm<1> opc_1, Reg Rm, Imm<3> option, bool S, Reg Rn,
                        Vec Vt) override;

private:
    Core::Memory::Memory& m_memory;
    std::span<u64, 31> m_regs;
    std::span<u128, 32> m_fpsimd_regs;
    u64& m_sp;
    const u64& m_pc;
};

std::optional<u64> MatchAndExecuteOneInstruction(Core::Memory::Memory& memory, mcontext_t* context,
                                                 fpsimd_context* fpsimd_context);

} // namespace Core
