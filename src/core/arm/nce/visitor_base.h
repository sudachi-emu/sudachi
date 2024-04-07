// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-FileCopyrightText: Copyright 2023 merryhime <https://mary.rs>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wshadow"

#include <dynarmic/frontend/A64/a64_types.h>
#include <dynarmic/frontend/A64/decoder/a64.h>
#include <dynarmic/frontend/imm.h>

#pragma GCC diagnostic pop

namespace Core {

class VisitorBase {
public:
    using instruction_return_type = bool;

    template <size_t BitSize>
    using Imm = Dynarmic::Imm<BitSize>;
    using Reg = Dynarmic::A64::Reg;
    using Vec = Dynarmic::A64::Vec;
    using Cond = Dynarmic::A64::Cond;

    virtual ~VisitorBase() {}

    virtual bool UnallocatedEncoding() {
        return false;
    }

    // Data processing - Immediate - PC relative addressing
    virtual bool ADR(Imm<2> immlo, Imm<19> immhi, Reg Rd) {
        return false;
    }
    virtual bool ADRP(Imm<2> immlo, Imm<19> immhi, Reg Rd) {
        return false;
    }

    // Data processing - Immediate - Add/Sub (with tag)
    virtual bool ADDG(Imm<6> offset_imm, Imm<4> tag_offset, Reg Rn, Reg Rd) {
        return false;
    }
    virtual bool SUBG(Imm<6> offset_imm, Imm<4> tag_offset, Reg Rn, Reg Rd) {
        return false;
    }

    // Data processing - Immediate - Add/Sub
    virtual bool ADD_imm(bool sf, Imm<2> shift, Imm<12> imm12, Reg Rn, Reg Rd) {
        return false;
    }
    virtual bool ADDS_imm(bool sf, Imm<2> shift, Imm<12> imm12, Reg Rn, Reg Rd) {
        return false;
    }
    virtual bool SUB_imm(bool sf, Imm<2> shift, Imm<12> imm12, Reg Rn, Reg Rd) {
        return false;
    }
    virtual bool SUBS_imm(bool sf, Imm<2> shift, Imm<12> imm12, Reg Rn, Reg Rd) {
        return false;
    }

    // Data processing - Immediate - Logical
    virtual bool AND_imm(bool sf, bool N, Imm<6> immr, Imm<6> imms, Reg Rn, Reg Rd) {
        return false;
    }
    virtual bool ORR_imm(bool sf, bool N, Imm<6> immr, Imm<6> imms, Reg Rn, Reg Rd) {
        return false;
    }
    virtual bool EOR_imm(bool sf, bool N, Imm<6> immr, Imm<6> imms, Reg Rn, Reg Rd) {
        return false;
    }
    virtual bool ANDS_imm(bool sf, bool N, Imm<6> immr, Imm<6> imms, Reg Rn, Reg Rd) {
        return false;
    }

    // Data processing - Immediate - Move Wide
    virtual bool MOVN(bool sf, Imm<2> hw, Imm<16> imm16, Reg Rd) {
        return false;
    }
    virtual bool MOVZ(bool sf, Imm<2> hw, Imm<16> imm16, Reg Rd) {
        return false;
    }
    virtual bool MOVK(bool sf, Imm<2> hw, Imm<16> imm16, Reg Rd) {
        return false;
    }

    // Data processing - Immediate - Bitfield
    virtual bool SBFM(bool sf, bool N, Imm<6> immr, Imm<6> imms, Reg Rn, Reg Rd) {
        return false;
    }
    virtual bool BFM(bool sf, bool N, Imm<6> immr, Imm<6> imms, Reg Rn, Reg Rd) {
        return false;
    }
    virtual bool UBFM(bool sf, bool N, Imm<6> immr, Imm<6> imms, Reg Rn, Reg Rd) {
        return false;
    }
    virtual bool ASR_1(Imm<5> immr, Reg Rn, Reg Rd) {
        return false;
    }
    virtual bool ASR_2(Imm<6> immr, Reg Rn, Reg Rd) {
        return false;
    }
    virtual bool SXTB_1(Reg Rn, Reg Rd) {
        return false;
    }
    virtual bool SXTB_2(Reg Rn, Reg Rd) {
        return false;
    }
    virtual bool SXTH_1(Reg Rn, Reg Rd) {
        return false;
    }
    virtual bool SXTH_2(Reg Rn, Reg Rd) {
        return false;
    }
    virtual bool SXTW(Reg Rn, Reg Rd) {
        return false;
    }

    // Data processing - Immediate - Extract
    virtual bool EXTR(bool sf, bool N, Reg Rm, Imm<6> imms, Reg Rn, Reg Rd) {
        return false;
    }

    // Conditional branch
    virtual bool B_cond(Imm<19> imm19, Cond cond) {
        return false;
    }

    // Exception generation
    virtual bool SVC(Imm<16> imm16) {
        return false;
    }
    virtual bool HVC(Imm<16> imm16) {
        return false;
    }
    virtual bool SMC(Imm<16> imm16) {
        return false;
    }
    virtual bool BRK(Imm<16> imm16) {
        return false;
    }
    virtual bool HLT(Imm<16> imm16) {
        return false;
    }
    virtual bool DCPS1(Imm<16> imm16) {
        return false;
    }
    virtual bool DCPS2(Imm<16> imm16) {
        return false;
    }
    virtual bool DCPS3(Imm<16> imm16) {
        return false;
    }

    // System
    virtual bool MSR_imm(Imm<3> op1, Imm<4> CRm, Imm<3> op2) {
        return false;
    }
    virtual bool HINT(Imm<4> CRm, Imm<3> op2) {
        return false;
    }
    virtual bool NOP() {
        return false;
    }
    virtual bool YIELD() {
        return false;
    }
    virtual bool WFE() {
        return false;
    }
    virtual bool WFI() {
        return false;
    }
    virtual bool SEV() {
        return false;
    }
    virtual bool SEVL() {
        return false;
    }
    virtual bool XPAC_1(bool D, Reg Rd) {
        return false;
    }
    virtual bool XPAC_2() {
        return false;
    }
    virtual bool PACIA_1(bool Z, Reg Rn, Reg Rd) {
        return false;
    }
    virtual bool PACIA_2() {
        return false;
    }
    virtual bool PACIB_1(bool Z, Reg Rn, Reg Rd) {
        return false;
    }
    virtual bool PACIB_2() {
        return false;
    }
    virtual bool AUTIA_1(bool Z, Reg Rn, Reg Rd) {
        return false;
    }
    virtual bool AUTIA_2() {
        return false;
    }
    virtual bool AUTIB_1(bool Z, Reg Rn, Reg Rd) {
        return false;
    }
    virtual bool AUTIB_2() {
        return false;
    }
    virtual bool BTI(Imm<2> upper_op2) {
        return false;
    }
    virtual bool ESB() {
        return false;
    }
    virtual bool PSB() {
        return false;
    }
    virtual bool TSB() {
        return false;
    }
    virtual bool CSDB() {
        return false;
    }
    virtual bool CLREX(Imm<4> CRm) {
        return false;
    }
    virtual bool DSB(Imm<4> CRm) {
        return false;
    }
    virtual bool SSBB() {
        return false;
    }
    virtual bool PSSBB() {
        return false;
    }
    virtual bool DMB(Imm<4> CRm) {
        return false;
    }
    virtual bool ISB(Imm<4> CRm) {
        return false;
    }
    virtual bool SYS(Imm<3> op1, Imm<4> CRn, Imm<4> CRm, Imm<3> op2, Reg Rt) {
        return false;
    }
    virtual bool SB() {
        return false;
    }
    virtual bool MSR_reg(Imm<1> o0, Imm<3> op1, Imm<4> CRn, Imm<4> CRm, Imm<3> op2, Reg Rt) {
        return false;
    }
    virtual bool SYSL(Imm<3> op1, Imm<4> CRn, Imm<4> CRm, Imm<3> op2, Reg Rt) {
        return false;
    }
    virtual bool MRS(Imm<1> o0, Imm<3> op1, Imm<4> CRn, Imm<4> CRm, Imm<3> op2, Reg Rt) {
        return false;
    }

    // System - Flag manipulation instructions
    virtual bool CFINV() {
        return false;
    }
    virtual bool RMIF(Imm<6> lsb, Reg Rn, Imm<4> mask) {
        return false;
    }
    virtual bool SETF8(Reg Rn) {
        return false;
    }
    virtual bool SETF16(Reg Rn) {
        return false;
    }

    // System - Flag format instructions
    virtual bool XAFlag() {
        return false;
    }
    virtual bool AXFlag() {
        return false;
    }

    // SYS: Data Cache
    virtual bool DC_IVAC(Reg Rt) {
        return false;
    }
    virtual bool DC_ISW(Reg Rt) {
        return false;
    }
    virtual bool DC_CSW(Reg Rt) {
        return false;
    }
    virtual bool DC_CISW(Reg Rt) {
        return false;
    }
    virtual bool DC_ZVA(Reg Rt) {
        return false;
    }
    virtual bool DC_CVAC(Reg Rt) {
        return false;
    }
    virtual bool DC_CVAU(Reg Rt) {
        return false;
    }
    virtual bool DC_CVAP(Reg Rt) {
        return false;
    }
    virtual bool DC_CIVAC(Reg Rt) {
        return false;
    }

    // SYS: Instruction Cache
    virtual bool IC_IALLU() {
        return false;
    }
    virtual bool IC_IALLUIS() {
        return false;
    }
    virtual bool IC_IVAU(Reg Rt) {
        return false;
    }

    // Unconditional branch (Register)
    virtual bool BR(Reg Rn) {
        return false;
    }
    virtual bool BRA(bool Z, bool M, Reg Rn, Reg Rm) {
        return false;
    }
    virtual bool BLR(Reg Rn) {
        return false;
    }
    virtual bool BLRA(bool Z, bool M, Reg Rn, Reg Rm) {
        return false;
    }
    virtual bool RET(Reg Rn) {
        return false;
    }
    virtual bool RETA(bool M) {
        return false;
    }
    virtual bool ERET() {
        return false;
    }
    virtual bool ERETA(bool M) {
        return false;
    }
    virtual bool DRPS() {
        return false;
    }

    // Unconditional branch (immediate)
    virtual bool B_uncond(Imm<26> imm26) {
        return false;
    }
    virtual bool BL(Imm<26> imm26) {
        return false;
    }

    // Compare and branch (immediate)
    virtual bool CBZ(bool sf, Imm<19> imm19, Reg Rt) {
        return false;
    }
    virtual bool CBNZ(bool sf, Imm<19> imm19, Reg Rt) {
        return false;
    }
    virtual bool TBZ(Imm<1> b5, Imm<5> b40, Imm<14> imm14, Reg Rt) {
        return false;
    }
    virtual bool TBNZ(Imm<1> b5, Imm<5> b40, Imm<14> imm14, Reg Rt) {
        return false;
    }

    // Loads and stores - Advanced SIMD Load/Store multiple structures
    virtual bool STx_mult_1(bool Q, Imm<4> opcode, Imm<2> size, Reg Rn, Vec Vt) {
        return false;
    }
    virtual bool STx_mult_2(bool Q, Reg Rm, Imm<4> opcode, Imm<2> size, Reg Rn, Vec Vt) {
        return false;
    }
    virtual bool LDx_mult_1(bool Q, Imm<4> opcode, Imm<2> size, Reg Rn, Vec Vt) {
        return false;
    }
    virtual bool LDx_mult_2(bool Q, Reg Rm, Imm<4> opcode, Imm<2> size, Reg Rn, Vec Vt) {
        return false;
    }

    // Loads and stores - Advanced SIMD Load/Store single structures
    virtual bool ST1_sngl_1(bool Q, Imm<2> upper_opcode, bool S, Imm<2> size, Reg Rn, Vec Vt) {
        return false;
    }
    virtual bool ST1_sngl_2(bool Q, Reg Rm, Imm<2> upper_opcode, bool S, Imm<2> size, Reg Rn,
                            Vec Vt) {
        return false;
    }
    virtual bool ST3_sngl_1(bool Q, Imm<2> upper_opcode, bool S, Imm<2> size, Reg Rn, Vec Vt) {
        return false;
    }
    virtual bool ST3_sngl_2(bool Q, Reg Rm, Imm<2> upper_opcode, bool S, Imm<2> size, Reg Rn,
                            Vec Vt) {
        return false;
    }
    virtual bool ST2_sngl_1(bool Q, Imm<2> upper_opcode, bool S, Imm<2> size, Reg Rn, Vec Vt) {
        return false;
    }
    virtual bool ST2_sngl_2(bool Q, Reg Rm, Imm<2> upper_opcode, bool S, Imm<2> size, Reg Rn,
                            Vec Vt) {
        return false;
    }
    virtual bool ST4_sngl_1(bool Q, Imm<2> upper_opcode, bool S, Imm<2> size, Reg Rn, Vec Vt) {
        return false;
    }
    virtual bool ST4_sngl_2(bool Q, Reg Rm, Imm<2> upper_opcode, bool S, Imm<2> size, Reg Rn,
                            Vec Vt) {
        return false;
    }
    virtual bool LD1_sngl_1(bool Q, Imm<2> upper_opcode, bool S, Imm<2> size, Reg Rn, Vec Vt) {
        return false;
    }
    virtual bool LD1_sngl_2(bool Q, Reg Rm, Imm<2> upper_opcode, bool S, Imm<2> size, Reg Rn,
                            Vec Vt) {
        return false;
    }
    virtual bool LD3_sngl_1(bool Q, Imm<2> upper_opcode, bool S, Imm<2> size, Reg Rn, Vec Vt) {
        return false;
    }
    virtual bool LD3_sngl_2(bool Q, Reg Rm, Imm<2> upper_opcode, bool S, Imm<2> size, Reg Rn,
                            Vec Vt) {
        return false;
    }
    virtual bool LD1R_1(bool Q, Imm<2> size, Reg Rn, Vec Vt) {
        return false;
    }
    virtual bool LD1R_2(bool Q, Reg Rm, Imm<2> size, Reg Rn, Vec Vt) {
        return false;
    }
    virtual bool LD3R_1(bool Q, Imm<2> size, Reg Rn, Vec Vt) {
        return false;
    }
    virtual bool LD3R_2(bool Q, Reg Rm, Imm<2> size, Reg Rn, Vec Vt) {
        return false;
    }
    virtual bool LD2_sngl_1(bool Q, Imm<2> upper_opcode, bool S, Imm<2> size, Reg Rn, Vec Vt) {
        return false;
    }
    virtual bool LD2_sngl_2(bool Q, Reg Rm, Imm<2> upper_opcode, bool S, Imm<2> size, Reg Rn,
                            Vec Vt) {
        return false;
    }
    virtual bool LD4_sngl_1(bool Q, Imm<2> upper_opcode, bool S, Imm<2> size, Reg Rn, Vec Vt) {
        return false;
    }
    virtual bool LD4_sngl_2(bool Q, Reg Rm, Imm<2> upper_opcode, bool S, Imm<2> size, Reg Rn,
                            Vec Vt) {
        return false;
    }
    virtual bool LD2R_1(bool Q, Imm<2> size, Reg Rn, Vec Vt) {
        return false;
    }
    virtual bool LD2R_2(bool Q, Reg Rm, Imm<2> size, Reg Rn, Vec Vt) {
        return false;
    }
    virtual bool LD4R_1(bool Q, Imm<2> size, Reg Rn, Vec Vt) {
        return false;
    }
    virtual bool LD4R_2(bool Q, Reg Rm, Imm<2> size, Reg Rn, Vec Vt) {
        return false;
    }

    // Loads and stores - Load/Store Exclusive
    virtual bool STXR(Imm<2> size, Reg Rs, Reg Rn, Reg Rt) {
        return false;
    }
    virtual bool STLXR(Imm<2> size, Reg Rs, Reg Rn, Reg Rt) {
        return false;
    }
    virtual bool STXP(Imm<1> size, Reg Rs, Reg Rt2, Reg Rn, Reg Rt) {
        return false;
    }
    virtual bool STLXP(Imm<1> size, Reg Rs, Reg Rt2, Reg Rn, Reg Rt) {
        return false;
    }
    virtual bool LDXR(Imm<2> size, Reg Rn, Reg Rt) {
        return false;
    }
    virtual bool LDAXR(Imm<2> size, Reg Rn, Reg Rt) {
        return false;
    }
    virtual bool LDXP(Imm<1> size, Reg Rt2, Reg Rn, Reg Rt) {
        return false;
    }
    virtual bool LDAXP(Imm<1> size, Reg Rt2, Reg Rn, Reg Rt) {
        return false;
    }
    virtual bool STLLR(Imm<2> size, Reg Rn, Reg Rt) {
        return false;
    }
    virtual bool STLR(Imm<2> size, Reg Rn, Reg Rt) {
        return false;
    }
    virtual bool LDLAR(Imm<2> size, Reg Rn, Reg Rt) {
        return false;
    }
    virtual bool LDAR(Imm<2> size, Reg Rn, Reg Rt) {
        return false;
    }
    virtual bool CASP(bool sz, bool L, Reg Rs, bool o0, Reg Rn, Reg Rt) {
        return false;
    }
    virtual bool CASB(bool L, Reg Rs, bool o0, Reg Rn, Reg Rt) {
        return false;
    }
    virtual bool CASH(bool L, Reg Rs, bool o0, Reg Rn, Reg Rt) {
        return false;
    }
    virtual bool CAS(bool sz, bool L, Reg Rs, bool o0, Reg Rn, Reg Rt) {
        return false;
    }

    // Loads and stores - Load register (literal)
    virtual bool LDR_lit_gen(bool opc_0, Imm<19> imm19, Reg Rt) {
        return false;
    }
    virtual bool LDR_lit_fpsimd(Imm<2> opc, Imm<19> imm19, Vec Vt) {
        return false;
    }
    virtual bool LDRSW_lit(Imm<19> imm19, Reg Rt) {
        return false;
    }
    virtual bool PRFM_lit(Imm<19> imm19, Imm<5> prfop) {
        return false;
    }

    // Loads and stores - Load/Store no-allocate pair
    virtual bool STNP_LDNP_gen(Imm<1> upper_opc, Imm<1> L, Imm<7> imm7, Reg Rt2, Reg Rn, Reg Rt) {
        return false;
    }
    virtual bool STNP_LDNP_fpsimd(Imm<2> opc, Imm<1> L, Imm<7> imm7, Vec Vt2, Reg Rn, Vec Vt) {
        return false;
    }

    // Loads and stores - Load/Store register pair
    virtual bool STP_LDP_gen(Imm<2> opc, bool not_postindex, bool wback, Imm<1> L, Imm<7> imm7,
                             Reg Rt2, Reg Rn, Reg Rt) {
        return false;
    }
    virtual bool STP_LDP_fpsimd(Imm<2> opc, bool not_postindex, bool wback, Imm<1> L, Imm<7> imm7,
                                Vec Vt2, Reg Rn, Vec Vt) {
        return false;
    }
    virtual bool STGP_1(Imm<7> offset_imm, Reg Rt2, Reg Rn, Reg Rt) {
        return false;
    }
    virtual bool STGP_2(Imm<7> offset_imm, Reg Rt2, Reg Rn, Reg Rt) {
        return false;
    }
    virtual bool STGP_3(Imm<7> offset_imm, Reg Rt2, Reg Rn, Reg Rt) {
        return false;
    }

    // Loads and stores - Load/Store register (immediate)
    virtual bool STRx_LDRx_imm_1(Imm<2> size, Imm<2> opc, Imm<9> imm9, bool not_postindex, Reg Rn,
                                 Reg Rt) {
        return false;
    }
    virtual bool STRx_LDRx_imm_2(Imm<2> size, Imm<2> opc, Imm<12> imm12, Reg Rn, Reg Rt) {
        return false;
    }
    virtual bool STURx_LDURx(Imm<2> size, Imm<2> opc, Imm<9> imm9, Reg Rn, Reg Rt) {
        return false;
    }
    virtual bool PRFM_imm(Imm<12> imm12, Reg Rn, Reg Rt) {
        return false;
    }
    virtual bool PRFM_unscaled_imm(Imm<9> imm9, Reg Rn, Reg Rt) {
        return false;
    }
    virtual bool STR_imm_fpsimd_1(Imm<2> size, Imm<1> opc_1, Imm<9> imm9, bool not_postindex,
                                  Reg Rn, Vec Vt) {
        return false;
    }
    virtual bool STR_imm_fpsimd_2(Imm<2> size, Imm<1> opc_1, Imm<12> imm12, Reg Rn, Vec Vt) {
        return false;
    }
    virtual bool LDR_imm_fpsimd_1(Imm<2> size, Imm<1> opc_1, Imm<9> imm9, bool not_postindex,
                                  Reg Rn, Vec Vt) {
        return false;
    }
    virtual bool LDR_imm_fpsimd_2(Imm<2> size, Imm<1> opc_1, Imm<12> imm12, Reg Rn, Vec Vt) {
        return false;
    }
    virtual bool STUR_fpsimd(Imm<2> size, Imm<1> opc_1, Imm<9> imm9, Reg Rn, Vec Vt) {
        return false;
    }
    virtual bool LDUR_fpsimd(Imm<2> size, Imm<1> opc_1, Imm<9> imm9, Reg Rn, Vec Vt) {
        return false;
    }

    // Loads and stores - Load/Store register (unprivileged)
    virtual bool STTRB(Imm<9> imm9, Reg Rn, Reg Rt) {
        return false;
    }
    virtual bool LDTRB(Imm<9> imm9, Reg Rn, Reg Rt) {
        return false;
    }
    virtual bool LDTRSB(Imm<2> opc, Imm<9> imm9, Reg Rn, Reg Rt) {
        return false;
    }
    virtual bool STTRH(Imm<9> imm9, Reg Rn, Reg Rt) {
        return false;
    }
    virtual bool LDTRH(Imm<9> imm9, Reg Rn, Reg Rt) {
        return false;
    }
    virtual bool LDTRSH(Imm<2> opc, Imm<9> imm9, Reg Rn, Reg Rt) {
        return false;
    }
    virtual bool STTR(Imm<2> size, Imm<9> imm9, Reg Rn, Reg Rt) {
        return false;
    }
    virtual bool LDTR(Imm<2> size, Imm<9> imm9, Reg Rn, Reg Rt) {
        return false;
    }
    virtual bool LDTRSW(Imm<9> imm9, Reg Rn, Reg Rt) {
        return false;
    }

    // Loads and stores - Atomic memory options
    virtual bool LDADDB(bool A, bool R, Reg Rs, Reg Rn, Reg Rt) {
        return false;
    }
    virtual bool LDCLRB(bool A, bool R, Reg Rs, Reg Rn, Reg Rt) {
        return false;
    }
    virtual bool LDEORB(bool A, bool R, Reg Rs, Reg Rn, Reg Rt) {
        return false;
    }
    virtual bool LDSETB(bool A, bool R, Reg Rs, Reg Rn, Reg Rt) {
        return false;
    }
    virtual bool LDSMAXB(bool A, bool R, Reg Rs, Reg Rn, Reg Rt) {
        return false;
    }
    virtual bool LDSMINB(bool A, bool R, Reg Rs, Reg Rn, Reg Rt) {
        return false;
    }
    virtual bool LDUMAXB(bool A, bool R, Reg Rs, Reg Rn, Reg Rt) {
        return false;
    }
    virtual bool LDUMINB(bool A, bool R, Reg Rs, Reg Rn, Reg Rt) {
        return false;
    }
    virtual bool SWPB(bool A, bool R, Reg Rs, Reg Rn, Reg Rt) {
        return false;
    }
    virtual bool LDAPRB(Reg Rn, Reg Rt) {
        return false;
    }
    virtual bool LDADDH(bool A, bool R, Reg Rs, Reg Rn, Reg Rt) {
        return false;
    }
    virtual bool LDCLRH(bool A, bool R, Reg Rs, Reg Rn, Reg Rt) {
        return false;
    }
    virtual bool LDEORH(bool A, bool R, Reg Rs, Reg Rn, Reg Rt) {
        return false;
    }
    virtual bool LDSETH(bool A, bool R, Reg Rs, Reg Rn, Reg Rt) {
        return false;
    }
    virtual bool LDSMAXH(bool A, bool R, Reg Rs, Reg Rn, Reg Rt) {
        return false;
    }
    virtual bool LDSMINH(bool A, bool R, Reg Rs, Reg Rn, Reg Rt) {
        return false;
    }
    virtual bool LDUMAXH(bool A, bool R, Reg Rs, Reg Rn, Reg Rt) {
        return false;
    }
    virtual bool LDUMINH(bool A, bool R, Reg Rs, Reg Rn, Reg Rt) {
        return false;
    }
    virtual bool SWPH(bool A, bool R, Reg Rs, Reg Rn, Reg Rt) {
        return false;
    }
    virtual bool LDAPRH(Reg Rn, Reg Rt) {
        return false;
    }
    virtual bool LDADD(bool A, bool R, Reg Rs, Reg Rn, Reg Rt) {
        return false;
    }
    virtual bool LDCLR(bool A, bool R, Reg Rs, Reg Rn, Reg Rt) {
        return false;
    }
    virtual bool LDEOR(bool A, bool R, Reg Rs, Reg Rn, Reg Rt) {
        return false;
    }
    virtual bool LDSET(bool A, bool R, Reg Rs, Reg Rn, Reg Rt) {
        return false;
    }
    virtual bool LDSMAX(bool A, bool R, Reg Rs, Reg Rn, Reg Rt) {
        return false;
    }
    virtual bool LDSMIN(bool A, bool R, Reg Rs, Reg Rn, Reg Rt) {
        return false;
    }
    virtual bool LDUMAX(bool A, bool R, Reg Rs, Reg Rn, Reg Rt) {
        return false;
    }
    virtual bool LDUMIN(bool A, bool R, Reg Rs, Reg Rn, Reg Rt) {
        return false;
    }
    virtual bool SWP(bool A, bool R, Reg Rs, Reg Rn, Reg Rt) {
        return false;
    }
    virtual bool LDAPR(Reg Rn, Reg Rt) {
        return false;
    }

    // Loads and stores - Load/Store register (register offset)
    virtual bool STRx_reg(Imm<2> size, Imm<1> opc_1, Reg Rm, Imm<3> option, bool S, Reg Rn,
                          Reg Rt) {
        return false;
    }
    virtual bool LDRx_reg(Imm<2> size, Imm<1> opc_1, Reg Rm, Imm<3> option, bool S, Reg Rn,
                          Reg Rt) {
        return false;
    }
    virtual bool STR_reg_fpsimd(Imm<2> size, Imm<1> opc_1, Reg Rm, Imm<3> option, bool S, Reg Rn,
                                Vec Vt) {
        return false;
    }
    virtual bool LDR_reg_fpsimd(Imm<2> size, Imm<1> opc_1, Reg Rm, Imm<3> option, bool S, Reg Rn,
                                Vec Vt) {
        return false;
    }

    // Loads and stores - Load/Store memory tags
    virtual bool STG_1(Imm<9> imm9, Reg Rn) {
        return false;
    }
    virtual bool STG_2(Imm<9> imm9, Reg Rn) {
        return false;
    }
    virtual bool STG_3(Imm<9> imm9, Reg Rn) {
        return false;
    }
    virtual bool LDG(Imm<9> offset_imm, Reg Rn, Reg Rt) {
        return false;
    }
    virtual bool STZG_1(Imm<9> offset_imm, Reg Rn) {
        return false;
    }
    virtual bool STZG_2(Imm<9> offset_imm, Reg Rn) {
        return false;
    }
    virtual bool STZG_3(Imm<9> offset_imm, Reg Rn) {
        return false;
    }
    virtual bool ST2G_1(Imm<9> offset_imm, Reg Rn) {
        return false;
    }
    virtual bool ST2G_2(Imm<9> offset_imm, Reg Rn) {
        return false;
    }
    virtual bool ST2G_3(Imm<9> offset_imm, Reg Rn) {
        return false;
    }
    virtual bool STGV(Reg Rn, Reg Rt) {
        return false;
    }
    virtual bool STZ2G_1(Imm<9> offset_imm, Reg Rn) {
        return false;
    }
    virtual bool STZ2G_2(Imm<9> offset_imm, Reg Rn) {
        return false;
    }
    virtual bool STZ2G_3(Imm<9> offset_imm, Reg Rn) {
        return false;
    }
    virtual bool LDGV(Reg Rn, Reg Rt) {
        return false;
    }

    // Loads and stores - Load/Store register (pointer authentication)
    virtual bool LDRA(bool M, bool S, Imm<9> imm9, bool W, Reg Rn, Reg Rt) {
        return false;
    }

    // Data Processing - Register - 2 source
    virtual bool UDIV(bool sf, Reg Rm, Reg Rn, Reg Rd) {
        return false;
    }
    virtual bool SDIV(bool sf, Reg Rm, Reg Rn, Reg Rd) {
        return false;
    }
    virtual bool LSLV(bool sf, Reg Rm, Reg Rn, Reg Rd) {
        return false;
    }
    virtual bool LSRV(bool sf, Reg Rm, Reg Rn, Reg Rd) {
        return false;
    }
    virtual bool ASRV(bool sf, Reg Rm, Reg Rn, Reg Rd) {
        return false;
    }
    virtual bool RORV(bool sf, Reg Rm, Reg Rn, Reg Rd) {
        return false;
    }
    virtual bool CRC32(bool sf, Reg Rm, Imm<2> sz, Reg Rn, Reg Rd) {
        return false;
    }
    virtual bool CRC32C(bool sf, Reg Rm, Imm<2> sz, Reg Rn, Reg Rd) {
        return false;
    }
    virtual bool PACGA(Reg Rm, Reg Rn, Reg Rd) {
        return false;
    }
    virtual bool SUBP(Reg Rm, Reg Rn, Reg Rd) {
        return false;
    }
    virtual bool IRG(Reg Rm, Reg Rn, Reg Rd) {
        return false;
    }
    virtual bool GMI(Reg Rm, Reg Rn, Reg Rd) {
        return false;
    }
    virtual bool SUBPS(Reg Rm, Reg Rn, Reg Rd) {
        return false;
    }

    // Data Processing - Register - 1 source
    virtual bool RBIT_int(bool sf, Reg Rn, Reg Rd) {
        return false;
    }
    virtual bool REV16_int(bool sf, Reg Rn, Reg Rd) {
        return false;
    }
    virtual bool REV(bool sf, bool opc_0, Reg Rn, Reg Rd) {
        return false;
    }
    virtual bool CLZ_int(bool sf, Reg Rn, Reg Rd) {
        return false;
    }
    virtual bool CLS_int(bool sf, Reg Rn, Reg Rd) {
        return false;
    }
    virtual bool REV32_int(Reg Rn, Reg Rd) {
        return false;
    }
    virtual bool PACDA(bool Z, Reg Rn, Reg Rd) {
        return false;
    }
    virtual bool PACDB(bool Z, Reg Rn, Reg Rd) {
        return false;
    }
    virtual bool AUTDA(bool Z, Reg Rn, Reg Rd) {
        return false;
    }
    virtual bool AUTDB(bool Z, Reg Rn, Reg Rd) {
        return false;
    }

    // Data Processing - Register - Logical (shifted register)
    virtual bool AND_shift(bool sf, Imm<2> shift, Reg Rm, Imm<6> imm6, Reg Rn, Reg Rd) {
        return false;
    }
    virtual bool BIC_shift(bool sf, Imm<2> shift, Reg Rm, Imm<6> imm6, Reg Rn, Reg Rd) {
        return false;
    }
    virtual bool ORR_shift(bool sf, Imm<2> shift, Reg Rm, Imm<6> imm6, Reg Rn, Reg Rd) {
        return false;
    }
    virtual bool ORN_shift(bool sf, Imm<2> shift, Reg Rm, Imm<6> imm6, Reg Rn, Reg Rd) {
        return false;
    }
    virtual bool EOR_shift(bool sf, Imm<2> shift, Reg Rm, Imm<6> imm6, Reg Rn, Reg Rd) {
        return false;
    }
    virtual bool EON(bool sf, Imm<2> shift, Reg Rm, Imm<6> imm6, Reg Rn, Reg Rd) {
        return false;
    }
    virtual bool ANDS_shift(bool sf, Imm<2> shift, Reg Rm, Imm<6> imm6, Reg Rn, Reg Rd) {
        return false;
    }
    virtual bool BICS(bool sf, Imm<2> shift, Reg Rm, Imm<6> imm6, Reg Rn, Reg Rd) {
        return false;
    }

    // Data Processing - Register - Add/Sub (shifted register)
    virtual bool ADD_shift(bool sf, Imm<2> shift, Reg Rm, Imm<6> imm6, Reg Rn, Reg Rd) {
        return false;
    }
    virtual bool ADDS_shift(bool sf, Imm<2> shift, Reg Rm, Imm<6> imm6, Reg Rn, Reg Rd) {
        return false;
    }
    virtual bool SUB_shift(bool sf, Imm<2> shift, Reg Rm, Imm<6> imm6, Reg Rn, Reg Rd) {
        return false;
    }
    virtual bool SUBS_shift(bool sf, Imm<2> shift, Reg Rm, Imm<6> imm6, Reg Rn, Reg Rd) {
        return false;
    }

    // Data Processing - Register - Add/Sub (shifted register)
    virtual bool ADD_ext(bool sf, Reg Rm, Imm<3> option, Imm<3> imm3, Reg Rn, Reg Rd) {
        return false;
    }
    virtual bool ADDS_ext(bool sf, Reg Rm, Imm<3> option, Imm<3> imm3, Reg Rn, Reg Rd) {
        return false;
    }
    virtual bool SUB_ext(bool sf, Reg Rm, Imm<3> option, Imm<3> imm3, Reg Rn, Reg Rd) {
        return false;
    }
    virtual bool SUBS_ext(bool sf, Reg Rm, Imm<3> option, Imm<3> imm3, Reg Rn, Reg Rd) {
        return false;
    }

    // Data Processing - Register - Add/Sub (with carry)
    virtual bool ADC(bool sf, Reg Rm, Reg Rn, Reg Rd) {
        return false;
    }
    virtual bool ADCS(bool sf, Reg Rm, Reg Rn, Reg Rd) {
        return false;
    }
    virtual bool SBC(bool sf, Reg Rm, Reg Rn, Reg Rd) {
        return false;
    }
    virtual bool SBCS(bool sf, Reg Rm, Reg Rn, Reg Rd) {
        return false;
    }

    // Data Processing - Register - Conditional compare
    virtual bool CCMN_reg(bool sf, Reg Rm, Cond cond, Reg Rn, Imm<4> nzcv) {
        return false;
    }
    virtual bool CCMP_reg(bool sf, Reg Rm, Cond cond, Reg Rn, Imm<4> nzcv) {
        return false;
    }
    virtual bool CCMN_imm(bool sf, Imm<5> imm5, Cond cond, Reg Rn, Imm<4> nzcv) {
        return false;
    }
    virtual bool CCMP_imm(bool sf, Imm<5> imm5, Cond cond, Reg Rn, Imm<4> nzcv) {
        return false;
    }

    // Data Processing - Register - Conditional select
    virtual bool CSEL(bool sf, Reg Rm, Cond cond, Reg Rn, Reg Rd) {
        return false;
    }
    virtual bool CSINC(bool sf, Reg Rm, Cond cond, Reg Rn, Reg Rd) {
        return false;
    }
    virtual bool CSINV(bool sf, Reg Rm, Cond cond, Reg Rn, Reg Rd) {
        return false;
    }
    virtual bool CSNEG(bool sf, Reg Rm, Cond cond, Reg Rn, Reg Rd) {
        return false;
    }

    // Data Processing - Register - 3 source
    virtual bool MADD(bool sf, Reg Rm, Reg Ra, Reg Rn, Reg Rd) {
        return false;
    }
    virtual bool MSUB(bool sf, Reg Rm, Reg Ra, Reg Rn, Reg Rd) {
        return false;
    }
    virtual bool SMADDL(Reg Rm, Reg Ra, Reg Rn, Reg Rd) {
        return false;
    }
    virtual bool SMSUBL(Reg Rm, Reg Ra, Reg Rn, Reg Rd) {
        return false;
    }
    virtual bool SMULH(Reg Rm, Reg Rn, Reg Rd) {
        return false;
    }
    virtual bool UMADDL(Reg Rm, Reg Ra, Reg Rn, Reg Rd) {
        return false;
    }
    virtual bool UMSUBL(Reg Rm, Reg Ra, Reg Rn, Reg Rd) {
        return false;
    }
    virtual bool UMULH(Reg Rm, Reg Rn, Reg Rd) {
        return false;
    }

    // Data Processing - FP and SIMD - AES
    virtual bool AESE(Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool AESD(Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool AESMC(Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool AESIMC(Vec Vn, Vec Vd) {
        return false;
    }

    // Data Processing - FP and SIMD - SHA
    virtual bool SHA1C(Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool SHA1P(Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool SHA1M(Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool SHA1SU0(Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool SHA256H(Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool SHA256H2(Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool SHA256SU1(Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool SHA1H(Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool SHA1SU1(Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool SHA256SU0(Vec Vn, Vec Vd) {
        return false;
    }

    // Data Processing - FP and SIMD - Scalar copy
    virtual bool DUP_elt_1(Imm<5> imm5, Vec Vn, Vec Vd) {
        return false;
    }

    // Data Processing - FP and SIMD - Scalar three
    virtual bool FMULX_vec_1(Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FMULX_vec_2(bool sz, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FCMEQ_reg_1(Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FCMEQ_reg_2(bool sz, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FRECPS_1(Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FRECPS_2(bool sz, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FRSQRTS_1(Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FRSQRTS_2(bool sz, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FCMGE_reg_1(Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FCMGE_reg_2(bool sz, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FACGE_1(Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FACGE_2(bool sz, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FABD_1(Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FABD_2(bool sz, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FCMGT_reg_1(Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FCMGT_reg_2(bool sz, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FACGT_1(Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FACGT_2(bool sz, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }

    // Data Processing - FP and SIMD - Two register misc FP16
    virtual bool FCVTNS_1(Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FCVTMS_1(Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FCVTAS_1(Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool SCVTF_int_1(Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FCMGT_zero_1(Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FCMEQ_zero_1(Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FCMLT_1(Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FCVTPS_1(Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FCVTZS_int_1(Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FRECPE_1(Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FRECPX_1(Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FCVTNU_1(Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FCVTMU_1(Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FCVTAU_1(Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool UCVTF_int_1(Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FCMGE_zero_1(Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FCMLE_1(Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FCVTPU_1(Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FCVTZU_int_1(Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FRSQRTE_1(Vec Vn, Vec Vd) {
        return false;
    }

    // Data Processing - FP and SIMD - Two register misc
    virtual bool FCVTNS_2(bool sz, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FCVTMS_2(bool sz, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FCVTAS_2(bool sz, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool SCVTF_int_2(bool sz, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FCMGT_zero_2(bool sz, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FCMEQ_zero_2(bool sz, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FCMLT_2(bool sz, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FCVTPS_2(bool sz, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FCVTZS_int_2(bool sz, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FRECPE_2(bool sz, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FRECPX_2(bool sz, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FCVTNU_2(bool sz, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FCVTMU_2(bool sz, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FCVTAU_2(bool sz, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool UCVTF_int_2(bool sz, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FCMGE_zero_2(bool sz, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FCMLE_2(bool sz, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FCVTPU_2(bool sz, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FCVTZU_int_2(bool sz, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FRSQRTE_2(bool sz, Vec Vn, Vec Vd) {
        return false;
    }

    // Data Processing - FP and SIMD - Scalar two register misc FP16
    virtual bool FCVTNS_3(bool Q, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FCVTMS_3(bool Q, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FCVTAS_3(bool Q, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool SCVTF_int_3(bool Q, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FCMGT_zero_3(bool Q, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FCMEQ_zero_3(bool Q, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FCMLT_3(bool Q, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FCVTPS_3(bool Q, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FCVTZS_int_3(bool Q, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FRECPE_3(bool Q, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FCVTNU_3(bool Q, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FCVTMU_3(bool Q, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FCVTAU_3(bool Q, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool UCVTF_int_3(bool Q, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FCMGE_zero_3(bool Q, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FCMLE_3(bool Q, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FCVTPU_3(bool Q, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FCVTZU_int_3(bool Q, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FRSQRTE_3(bool Q, Vec Vn, Vec Vd) {
        return false;
    }

    // Data Processing - FP and SIMD - Scalar two register misc
    virtual bool FCVTNS_4(bool Q, bool sz, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FCVTMS_4(bool Q, bool sz, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FCVTAS_4(bool Q, bool sz, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool SCVTF_int_4(bool Q, bool sz, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FCMGT_zero_4(bool Q, bool sz, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FCMEQ_zero_4(bool Q, bool sz, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FCMLT_4(bool Q, bool sz, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FCVTPS_4(bool Q, bool sz, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FCVTZS_int_4(bool Q, bool sz, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FRECPE_4(bool Q, bool sz, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FCVTNU_4(bool Q, bool sz, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FCVTMU_4(bool Q, bool sz, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FCVTAU_4(bool Q, bool sz, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool UCVTF_int_4(bool Q, bool sz, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FCMGE_zero_4(bool Q, bool sz, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FCMLE_4(bool Q, bool sz, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FCVTPU_4(bool Q, bool sz, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FCVTZU_int_4(bool Q, bool sz, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FRSQRTE_4(bool Q, bool sz, Vec Vn, Vec Vd) {
        return false;
    }

    // Data Processing - FP and SIMD - Scalar three same extra
    virtual bool SQRDMLAH_vec_1(Imm<2> size, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool SQRDMLAH_vec_2(bool Q, Imm<2> size, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool SQRDMLSH_vec_1(Imm<2> size, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool SQRDMLSH_vec_2(bool Q, Imm<2> size, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }

    // Data Processing - FP and SIMD - Scalar two-register misc
    virtual bool SUQADD_1(Imm<2> size, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool SQABS_1(Imm<2> size, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool CMGT_zero_1(Imm<2> size, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool CMEQ_zero_1(Imm<2> size, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool CMLT_1(Imm<2> size, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool ABS_1(Imm<2> size, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool SQXTN_1(Imm<2> size, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool USQADD_1(Imm<2> size, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool SQNEG_1(Imm<2> size, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool CMGE_zero_1(Imm<2> size, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool CMLE_1(Imm<2> size, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool NEG_1(Imm<2> size, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool SQXTUN_1(Imm<2> size, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool UQXTN_1(Imm<2> size, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FCVTXN_1(bool sz, Vec Vn, Vec Vd) {
        return false;
    }

    // Data Processing - FP and SIMD - SIMD Scalar pairwise
    virtual bool ADDP_pair(Imm<2> size, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FMAXNMP_pair_1(Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FMAXNMP_pair_2(bool sz, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FADDP_pair_1(Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FADDP_pair_2(bool sz, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FMAXP_pair_1(Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FMAXP_pair_2(bool sz, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FMINNMP_pair_1(Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FMINNMP_pair_2(bool sz, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FMINP_pair_1(Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FMINP_pair_2(bool sz, Vec Vn, Vec Vd) {
        return false;
    }

    // Data Processing - FP and SIMD - SIMD Scalar three different
    virtual bool SQDMLAL_vec_1(Imm<2> size, Reg Rm, Reg Rn, Vec Vd) {
        return false;
    }
    virtual bool SQDMLSL_vec_1(Imm<2> size, Reg Rm, Reg Rn, Vec Vd) {
        return false;
    }
    virtual bool SQDMULL_vec_1(Imm<2> size, Reg Rm, Reg Rn, Vec Vd) {
        return false;
    }

    // Data Processing - FP and SIMD - SIMD Scalar three same
    virtual bool SQADD_1(Imm<2> size, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool SQSUB_1(Imm<2> size, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool CMGT_reg_1(Imm<2> size, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool CMGE_reg_1(Imm<2> size, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool SSHL_1(Imm<2> size, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool SQSHL_reg_1(Imm<2> size, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool SRSHL_1(Imm<2> size, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool SQRSHL_1(Imm<2> size, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool ADD_1(Imm<2> size, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool CMTST_1(Imm<2> size, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool SQDMULH_vec_1(Imm<2> size, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool UQADD_1(Imm<2> size, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool UQSUB_1(Imm<2> size, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool CMHI_1(Imm<2> size, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool CMHS_1(Imm<2> size, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool USHL_1(Imm<2> size, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool UQSHL_reg_1(Imm<2> size, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool URSHL_1(Imm<2> size, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool UQRSHL_1(Imm<2> size, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool SUB_1(Imm<2> size, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool CMEQ_reg_1(Imm<2> size, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool SQRDMULH_vec_1(Imm<2> size, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }

    // Data Processing - FP and SIMD - SIMD Scalar shift by immediate
    virtual bool SSHR_1(Imm<4> immh, Imm<3> immb, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool SSRA_1(Imm<4> immh, Imm<3> immb, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool SRSHR_1(Imm<4> immh, Imm<3> immb, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool SRSRA_1(Imm<4> immh, Imm<3> immb, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool SHL_1(Imm<4> immh, Imm<3> immb, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool SQSHL_imm_1(Imm<4> immh, Imm<3> immb, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool SQSHRN_1(Imm<4> immh, Imm<3> immb, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool SQRSHRN_1(Imm<4> immh, Imm<3> immb, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool SCVTF_fix_1(Imm<4> immh, Imm<3> immb, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FCVTZS_fix_1(Imm<4> immh, Imm<3> immb, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool USHR_1(Imm<4> immh, Imm<3> immb, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool USRA_1(Imm<4> immh, Imm<3> immb, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool URSHR_1(Imm<4> immh, Imm<3> immb, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool URSRA_1(Imm<4> immh, Imm<3> immb, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool SRI_1(Imm<4> immh, Imm<3> immb, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool SLI_1(Imm<4> immh, Imm<3> immb, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool SQSHLU_1(Imm<4> immh, Imm<3> immb, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool UQSHL_imm_1(Imm<4> immh, Imm<3> immb, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool SQSHRUN_1(Imm<4> immh, Imm<3> immb, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool SQRSHRUN_1(Imm<4> immh, Imm<3> immb, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool UQSHRN_1(Imm<4> immh, Imm<3> immb, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool UQRSHRN_1(Imm<4> immh, Imm<3> immb, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool UCVTF_fix_1(Imm<4> immh, Imm<3> immb, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FCVTZU_fix_1(Imm<4> immh, Imm<3> immb, Vec Vn, Vec Vd) {
        return false;
    }

    // Data Processing - FP and SIMD - SIMD Scalar x indexed element
    virtual bool SQDMLAL_elt_1(Imm<2> size, Imm<1> L, Imm<1> M, Imm<4> Vmlo, Imm<1> H, Vec Vn,
                               Vec Vd) {
        return false;
    }
    virtual bool SQDMLSL_elt_1(Imm<2> size, Imm<1> L, Imm<1> M, Imm<4> Vmlo, Imm<1> H, Vec Vn,
                               Vec Vd) {
        return false;
    }
    virtual bool SQDMULL_elt_1(Imm<2> size, Imm<1> L, Imm<1> M, Imm<4> Vmlo, Imm<1> H, Vec Vn,
                               Vec Vd) {
        return false;
    }
    virtual bool SQDMULH_elt_1(Imm<2> size, Imm<1> L, Imm<1> M, Imm<4> Vmlo, Imm<1> H, Vec Vn,
                               Vec Vd) {
        return false;
    }
    virtual bool SQRDMULH_elt_1(Imm<2> size, Imm<1> L, Imm<1> M, Imm<4> Vmlo, Imm<1> H, Vec Vn,
                                Vec Vd) {
        return false;
    }
    virtual bool FMLA_elt_1(Imm<1> L, Imm<1> M, Imm<4> Vmlo, Imm<1> H, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FMLA_elt_2(bool sz, Imm<1> L, Imm<1> M, Imm<4> Vmlo, Imm<1> H, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FMLS_elt_1(Imm<1> L, Imm<1> M, Imm<4> Vmlo, Imm<1> H, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FMLS_elt_2(bool sz, Imm<1> L, Imm<1> M, Imm<4> Vmlo, Imm<1> H, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FMUL_elt_1(Imm<1> L, Imm<1> M, Imm<4> Vmlo, Imm<1> H, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FMUL_elt_2(bool sz, Imm<1> L, Imm<1> M, Imm<4> Vmlo, Imm<1> H, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool SQRDMLAH_elt_1(Imm<2> size, Imm<1> L, Imm<1> M, Imm<4> Vmlo, Imm<1> H, Vec Vn,
                                Vec Vd) {
        return false;
    }
    virtual bool SQRDMLSH_elt_1(Imm<2> size, Imm<1> L, Imm<1> M, Imm<4> Vmlo, Imm<1> H, Vec Vn,
                                Vec Vd) {
        return false;
    }
    virtual bool FMULX_elt_1(Imm<1> L, Imm<1> M, Imm<4> Vmlo, Imm<1> H, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FMULX_elt_2(bool sz, Imm<1> L, Imm<1> M, Imm<4> Vmlo, Imm<1> H, Vec Vn, Vec Vd) {
        return false;
    }

    // Data Processing - FP and SIMD - SIMD Table Lookup
    virtual bool TBL(bool Q, Vec Vm, Imm<2> len, size_t Vn, Vec Vd) {
        return false;
    }
    virtual bool TBX(bool Q, Vec Vm, Imm<2> len, size_t Vn, Vec Vd) {
        return false;
    }

    // Data Processing - FP and SIMD - SIMD Permute
    virtual bool UZP1(bool Q, Imm<2> size, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool TRN1(bool Q, Imm<2> size, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool ZIP1(bool Q, Imm<2> size, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool UZP2(bool Q, Imm<2> size, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool TRN2(bool Q, Imm<2> size, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool ZIP2(bool Q, Imm<2> size, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }

    // Data Processing - FP and SIMD - SIMD Extract
    virtual bool EXT(bool Q, Vec Vm, Imm<4> imm4, Vec Vn, Vec Vd) {
        return false;
    }

    // Data Processing - FP and SIMD - SIMD Copy
    virtual bool DUP_elt_2(bool Q, Imm<5> imm5, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool DUP_gen(bool Q, Imm<5> imm5, Reg Rn, Vec Vd) {
        return false;
    }
    virtual bool SMOV(bool Q, Imm<5> imm5, Vec Vn, Reg Rd) {
        return false;
    }
    virtual bool UMOV(bool Q, Imm<5> imm5, Vec Vn, Reg Rd) {
        return false;
    }
    virtual bool INS_gen(Imm<5> imm5, Reg Rn, Vec Vd) {
        return false;
    }
    virtual bool INS_elt(Imm<5> imm5, Imm<4> imm4, Vec Vn, Vec Vd) {
        return false;
    }

    // Data Processing - FP and SIMD - SIMD Three same
    virtual bool FMULX_vec_3(bool Q, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FCMEQ_reg_3(bool Q, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FRECPS_3(bool Q, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FRSQRTS_3(bool Q, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FCMGE_reg_3(bool Q, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FACGE_3(bool Q, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FABD_3(bool Q, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FCMGT_reg_3(bool Q, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FACGT_3(bool Q, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FMAXNM_1(bool Q, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FMLA_vec_1(bool Q, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FADD_1(bool Q, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FMAX_1(bool Q, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FMINNM_1(bool Q, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FMLS_vec_1(bool Q, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FSUB_1(bool Q, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FMIN_1(bool Q, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FMAXNMP_vec_1(bool Q, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FADDP_vec_1(bool Q, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FMUL_vec_1(bool Q, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FMAXP_vec_1(bool Q, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FDIV_1(bool Q, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FMINNMP_vec_1(bool Q, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FMINP_vec_1(bool Q, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }

    // Data Processing - FP and SIMD - SIMD Three same extra
    virtual bool SDOT_vec(bool Q, Imm<2> size, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool UDOT_vec(bool Q, Imm<2> size, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FCMLA_vec(bool Q, Imm<2> size, Vec Vm, Imm<2> rot, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FCADD_vec(bool Q, Imm<2> size, Vec Vm, Imm<1> rot, Vec Vn, Vec Vd) {
        return false;
    }

    // Data Processing - FP and SIMD - SIMD Two register misc
    virtual bool REV64_asimd(bool Q, Imm<2> size, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool REV16_asimd(bool Q, Imm<2> size, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool SADDLP(bool Q, Imm<2> size, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool CLS_asimd(bool Q, Imm<2> size, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool CNT(bool Q, Imm<2> size, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool SADALP(bool Q, Imm<2> size, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool XTN(bool Q, Imm<2> size, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FCVTN(bool Q, bool sz, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FCVTL(bool Q, bool sz, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool URECPE(bool Q, bool sz, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool REV32_asimd(bool Q, Imm<2> size, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool UADDLP(bool Q, Imm<2> size, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool CLZ_asimd(bool Q, Imm<2> size, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool UADALP(bool Q, Imm<2> size, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool SHLL(bool Q, Imm<2> size, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool NOT(bool Q, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool RBIT_asimd(bool Q, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool URSQRTE(bool Q, bool sz, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool SUQADD_2(bool Q, Imm<2> size, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool SQABS_2(bool Q, Imm<2> size, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool CMGT_zero_2(bool Q, Imm<2> size, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool CMEQ_zero_2(bool Q, Imm<2> size, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool CMLT_2(bool Q, Imm<2> size, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool ABS_2(bool Q, Imm<2> size, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool SQXTN_2(bool Q, Imm<2> size, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool USQADD_2(bool Q, Imm<2> size, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool SQNEG_2(bool Q, Imm<2> size, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool CMGE_zero_2(bool Q, Imm<2> size, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool CMLE_2(bool Q, Imm<2> size, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool NEG_2(bool Q, Imm<2> size, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool SQXTUN_2(bool Q, Imm<2> size, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool UQXTN_2(bool Q, Imm<2> size, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FCVTXN_2(bool Q, bool sz, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FRINTN_1(bool Q, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FRINTN_2(bool Q, bool sz, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FRINTM_1(bool Q, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FRINTM_2(bool Q, bool sz, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FABS_1(bool Q, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FABS_2(bool Q, bool sz, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FRINTP_1(bool Q, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FRINTP_2(bool Q, bool sz, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FRINTZ_1(bool Q, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FRINTZ_2(bool Q, bool sz, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FRINTA_1(bool Q, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FRINTA_2(bool Q, bool sz, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FRINTX_1(bool Q, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FRINTX_2(bool Q, bool sz, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FNEG_1(bool Q, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FNEG_2(bool Q, bool sz, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FRINTI_1(bool Q, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FRINTI_2(bool Q, bool sz, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FSQRT_1(bool Q, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FSQRT_2(bool Q, bool sz, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FRINT32X_1(bool Q, bool sz, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FRINT64X_1(bool Q, bool sz, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FRINT32Z_1(bool Q, bool sz, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FRINT64Z_1(bool Q, bool sz, Vec Vn, Vec Vd) {
        return false;
    }

    // Data Processing - FP and SIMD - SIMD across lanes
    virtual bool SADDLV(bool Q, Imm<2> size, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool SMAXV(bool Q, Imm<2> size, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool SMINV(bool Q, Imm<2> size, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool ADDV(bool Q, Imm<2> size, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FMAXNMV_1(bool Q, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FMAXNMV_2(bool Q, bool sz, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FMAXV_1(bool Q, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FMAXV_2(bool Q, bool sz, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FMINNMV_1(bool Q, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FMINNMV_2(bool Q, bool sz, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FMINV_1(bool Q, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FMINV_2(bool Q, bool sz, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool UADDLV(bool Q, Imm<2> size, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool UMAXV(bool Q, Imm<2> size, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool UMINV(bool Q, Imm<2> size, Vec Vn, Vec Vd) {
        return false;
    }

    // Data Processing - FP and SIMD - SIMD three different
    virtual bool SADDL(bool Q, Imm<2> size, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool SADDW(bool Q, Imm<2> size, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool SSUBL(bool Q, Imm<2> size, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool SSUBW(bool Q, Imm<2> size, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool ADDHN(bool Q, Imm<2> size, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool SABAL(bool Q, Imm<2> size, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool SUBHN(bool Q, Imm<2> size, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool SABDL(bool Q, Imm<2> size, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool SMLAL_vec(bool Q, Imm<2> size, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool SMLSL_vec(bool Q, Imm<2> size, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool SMULL_vec(bool Q, Imm<2> size, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool PMULL(bool Q, Imm<2> size, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool UADDL(bool Q, Imm<2> size, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool UADDW(bool Q, Imm<2> size, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool USUBL(bool Q, Imm<2> size, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool USUBW(bool Q, Imm<2> size, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool RADDHN(bool Q, Imm<2> size, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool UABAL(bool Q, Imm<2> size, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool RSUBHN(bool Q, Imm<2> size, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool UABDL(bool Q, Imm<2> size, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool UMLAL_vec(bool Q, Imm<2> size, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool UMLSL_vec(bool Q, Imm<2> size, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool UMULL_vec(bool Q, Imm<2> size, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool SQDMLAL_vec_2(bool Q, Imm<2> size, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool SQDMLSL_vec_2(bool Q, Imm<2> size, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool SQDMULL_vec_2(bool Q, Imm<2> size, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }

    // Data Processing - FP and SIMD - SIMD three same
    virtual bool SHADD(bool Q, Imm<2> size, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool SRHADD(bool Q, Imm<2> size, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool SHSUB(bool Q, Imm<2> size, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool SMAX(bool Q, Imm<2> size, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool SMIN(bool Q, Imm<2> size, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool SABD(bool Q, Imm<2> size, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool SABA(bool Q, Imm<2> size, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool MLA_vec(bool Q, Imm<2> size, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool MUL_vec(bool Q, Imm<2> size, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool SMAXP(bool Q, Imm<2> size, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool SMINP(bool Q, Imm<2> size, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool ADDP_vec(bool Q, Imm<2> size, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FMLAL_vec_1(bool Q, bool sz, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FMLAL_vec_2(bool Q, bool sz, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool AND_asimd(bool Q, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool BIC_asimd_reg(bool Q, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FMLSL_vec_1(bool Q, bool sz, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FMLSL_vec_2(bool Q, bool sz, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool ORR_asimd_reg(bool Q, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool ORN_asimd(bool Q, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool UHADD(bool Q, Imm<2> size, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool URHADD(bool Q, Imm<2> size, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool UHSUB(bool Q, Imm<2> size, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool UMAX(bool Q, Imm<2> size, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool UMIN(bool Q, Imm<2> size, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool UABD(bool Q, Imm<2> size, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool UABA(bool Q, Imm<2> size, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool MLS_vec(bool Q, Imm<2> size, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool PMUL(bool Q, Imm<2> size, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool UMAXP(bool Q, Imm<2> size, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool UMINP(bool Q, Imm<2> size, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool EOR_asimd(bool Q, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool BSL(bool Q, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool BIT(bool Q, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool BIF(bool Q, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FMAXNM_2(bool Q, bool sz, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FMLA_vec_2(bool Q, bool sz, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FADD_2(bool Q, bool sz, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FMAX_2(bool Q, bool sz, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FMINNM_2(bool Q, bool sz, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FMLS_vec_2(bool Q, bool sz, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FSUB_2(bool Q, bool sz, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FMIN_2(bool Q, bool sz, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FMAXNMP_vec_2(bool Q, bool sz, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FADDP_vec_2(bool Q, bool sz, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FMUL_vec_2(bool Q, bool sz, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FMAXP_vec_2(bool Q, bool sz, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FDIV_2(bool Q, bool sz, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FMINNMP_vec_2(bool Q, bool sz, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FMINP_vec_2(bool Q, bool sz, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FMULX_vec_4(bool Q, bool sz, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FCMEQ_reg_4(bool Q, bool sz, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FRECPS_4(bool Q, bool sz, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FRSQRTS_4(bool Q, bool sz, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FCMGE_reg_4(bool Q, bool sz, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FACGE_4(bool Q, bool sz, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FABD_4(bool Q, bool sz, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FCMGT_reg_4(bool Q, bool sz, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FACGT_4(bool Q, bool sz, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool SQADD_2(bool Q, Imm<2> size, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool SQSUB_2(bool Q, Imm<2> size, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool CMGT_reg_2(bool Q, Imm<2> size, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool CMGE_reg_2(bool Q, Imm<2> size, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool SSHL_2(bool Q, Imm<2> size, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool SQSHL_reg_2(bool Q, Imm<2> size, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool SRSHL_2(bool Q, Imm<2> size, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool SQRSHL_2(bool Q, Imm<2> size, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool ADD_vector(bool Q, Imm<2> size, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool CMTST_2(bool Q, Imm<2> size, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool SQDMULH_vec_2(bool Q, Imm<2> size, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool UQADD_2(bool Q, Imm<2> size, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool UQSUB_2(bool Q, Imm<2> size, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool CMHI_2(bool Q, Imm<2> size, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool CMHS_2(bool Q, Imm<2> size, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool USHL_2(bool Q, Imm<2> size, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool UQSHL_reg_2(bool Q, Imm<2> size, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool URSHL_2(bool Q, Imm<2> size, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool UQRSHL_2(bool Q, Imm<2> size, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool SUB_2(bool Q, Imm<2> size, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool CMEQ_reg_2(bool Q, Imm<2> size, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool SQRDMULH_vec_2(bool Q, Imm<2> size, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }

    // Data Processing - FP and SIMD - SIMD modified immediate
    virtual bool MOVI(bool Q, bool op, Imm<1> a, Imm<1> b, Imm<1> c, Imm<4> cmode, Imm<1> d,
                      Imm<1> e, Imm<1> f, Imm<1> g, Imm<1> h, Vec Vd) {
        return false;
    }
    virtual bool FMOV_2(bool Q, bool op, Imm<1> a, Imm<1> b, Imm<1> c, Imm<1> d, Imm<1> e, Imm<1> f,
                        Imm<1> g, Imm<1> h, Vec Vd) {
        return false;
    }
    virtual bool FMOV_3(bool Q, Imm<1> a, Imm<1> b, Imm<1> c, Imm<1> d, Imm<1> e, Imm<1> f,
                        Imm<1> g, Imm<1> h, Vec Vd) {
        return false;
    }

    // Data Processing - FP and SIMD - SIMD Shift by immediate
    virtual bool SSHR_2(bool Q, Imm<4> immh, Imm<3> immb, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool SSRA_2(bool Q, Imm<4> immh, Imm<3> immb, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool SRSHR_2(bool Q, Imm<4> immh, Imm<3> immb, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool SRSRA_2(bool Q, Imm<4> immh, Imm<3> immb, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool SHL_2(bool Q, Imm<4> immh, Imm<3> immb, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool SQSHL_imm_2(bool Q, Imm<4> immh, Imm<3> immb, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool SHRN(bool Q, Imm<4> immh, Imm<3> immb, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool RSHRN(bool Q, Imm<4> immh, Imm<3> immb, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool SQSHRN_2(bool Q, Imm<4> immh, Imm<3> immb, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool SQRSHRN_2(bool Q, Imm<4> immh, Imm<3> immb, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool SSHLL(bool Q, Imm<4> immh, Imm<3> immb, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool SCVTF_fix_2(bool Q, Imm<4> immh, Imm<3> immb, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FCVTZS_fix_2(bool Q, Imm<4> immh, Imm<3> immb, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool USHR_2(bool Q, Imm<4> immh, Imm<3> immb, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool USRA_2(bool Q, Imm<4> immh, Imm<3> immb, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool URSHR_2(bool Q, Imm<4> immh, Imm<3> immb, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool URSRA_2(bool Q, Imm<4> immh, Imm<3> immb, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool SRI_2(bool Q, Imm<4> immh, Imm<3> immb, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool SLI_2(bool Q, Imm<4> immh, Imm<3> immb, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool SQSHLU_2(bool Q, Imm<4> immh, Imm<3> immb, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool UQSHL_imm_2(bool Q, Imm<4> immh, Imm<3> immb, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool SQSHRUN_2(bool Q, Imm<4> immh, Imm<3> immb, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool SQRSHRUN_2(bool Q, Imm<4> immh, Imm<3> immb, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool UQSHRN_2(bool Q, Imm<4> immh, Imm<3> immb, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool UQRSHRN_2(bool Q, Imm<4> immh, Imm<3> immb, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool USHLL(bool Q, Imm<4> immh, Imm<3> immb, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool UCVTF_fix_2(bool Q, Imm<4> immh, Imm<3> immb, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FCVTZU_fix_2(bool Q, Imm<4> immh, Imm<3> immb, Vec Vn, Vec Vd) {
        return false;
    }

    // Data Processing - FP and SIMD - SIMD vector x indexed element
    virtual bool SMLAL_elt(bool Q, Imm<2> size, Imm<1> L, Imm<1> M, Imm<4> Vmlo, Imm<1> H, Vec Vn,
                           Vec Vd) {
        return false;
    }
    virtual bool SQDMLAL_elt_2(bool Q, Imm<2> size, Imm<1> L, Imm<1> M, Imm<4> Vmlo, Imm<1> H,
                               Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool SMLSL_elt(bool Q, Imm<2> size, Imm<1> L, Imm<1> M, Imm<4> Vmlo, Imm<1> H, Vec Vn,
                           Vec Vd) {
        return false;
    }
    virtual bool SQDMLSL_elt_2(bool Q, Imm<2> size, Imm<1> L, Imm<1> M, Imm<4> Vmlo, Imm<1> H,
                               Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool MUL_elt(bool Q, Imm<2> size, Imm<1> L, Imm<1> M, Imm<4> Vmlo, Imm<1> H, Vec Vn,
                         Vec Vd) {
        return false;
    }
    virtual bool SMULL_elt(bool Q, Imm<2> size, Imm<1> L, Imm<1> M, Imm<4> Vm, Imm<1> H, Vec Vn,
                           Vec Vd) {
        return false;
    }
    virtual bool SQDMULL_elt_2(bool Q, Imm<2> size, Imm<1> L, Imm<1> M, Imm<4> Vmlo, Imm<1> H,
                               Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool SQDMULH_elt_2(bool Q, Imm<2> size, Imm<1> L, Imm<1> M, Imm<4> Vmlo, Imm<1> H,
                               Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool SQRDMULH_elt_2(bool Q, Imm<2> size, Imm<1> L, Imm<1> M, Imm<4> Vmlo, Imm<1> H,
                                Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool SDOT_elt(bool Q, Imm<2> size, Imm<1> L, Imm<1> M, Imm<4> Vmlo, Imm<1> H, Vec Vn,
                          Vec Vd) {
        return false;
    }
    virtual bool FMLA_elt_3(bool Q, Imm<1> L, Imm<1> M, Imm<4> Vmlo, Imm<1> H, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FMLA_elt_4(bool Q, bool sz, Imm<1> L, Imm<1> M, Imm<4> Vmlo, Imm<1> H, Vec Vn,
                            Vec Vd) {
        return false;
    }
    virtual bool FMLS_elt_3(bool Q, Imm<1> L, Imm<1> M, Imm<4> Vmlo, Imm<1> H, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FMLS_elt_4(bool Q, bool sz, Imm<1> L, Imm<1> M, Imm<4> Vmlo, Imm<1> H, Vec Vn,
                            Vec Vd) {
        return false;
    }
    virtual bool FMUL_elt_3(bool Q, Imm<1> L, Imm<1> M, Imm<4> Vmlo, Imm<1> H, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FMUL_elt_4(bool Q, bool sz, Imm<1> L, Imm<1> M, Imm<4> Vmlo, Imm<1> H, Vec Vn,
                            Vec Vd) {
        return false;
    }
    virtual bool FMLAL_elt_1(bool Q, bool sz, Imm<1> L, Imm<1> M, Imm<4> Vmlo, Imm<1> H, Vec Vn,
                             Vec Vd) {
        return false;
    }
    virtual bool FMLAL_elt_2(bool Q, bool sz, Imm<1> L, Imm<1> M, Imm<4> Vmlo, Imm<1> H, Vec Vn,
                             Vec Vd) {
        return false;
    }
    virtual bool FMLSL_elt_1(bool Q, bool sz, Imm<1> L, Imm<1> M, Imm<4> Vmlo, Imm<1> H, Vec Vn,
                             Vec Vd) {
        return false;
    }
    virtual bool FMLSL_elt_2(bool Q, bool sz, Imm<1> L, Imm<1> M, Imm<4> Vmlo, Imm<1> H, Vec Vn,
                             Vec Vd) {
        return false;
    }
    virtual bool MLA_elt(bool Q, Imm<2> size, Imm<1> L, Imm<1> M, Imm<4> Vmlo, Imm<1> H, Vec Vn,
                         Vec Vd) {
        return false;
    }
    virtual bool UMLAL_elt(bool Q, Imm<2> size, Imm<1> L, Imm<1> M, Imm<4> Vmlo, Imm<1> H, Vec Vn,
                           Vec Vd) {
        return false;
    }
    virtual bool MLS_elt(bool Q, Imm<2> size, Imm<1> L, Imm<1> M, Imm<4> Vmlo, Imm<1> H, Vec Vn,
                         Vec Vd) {
        return false;
    }
    virtual bool UMLSL_elt(bool Q, Imm<2> size, Imm<1> L, Imm<1> M, Imm<4> Vmlo, Imm<1> H, Vec Vn,
                           Vec Vd) {
        return false;
    }
    virtual bool UMULL_elt(bool Q, Imm<2> size, Imm<1> L, Imm<1> M, Imm<4> Vmlo, Imm<1> H, Vec Vn,
                           Vec Vd) {
        return false;
    }
    virtual bool SQRDMLAH_elt_2(bool Q, Imm<2> size, Imm<1> L, Imm<1> M, Imm<4> Vmlo, Imm<1> H,
                                Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool UDOT_elt(bool Q, Imm<2> size, Imm<1> L, Imm<1> M, Imm<4> Vmlo, Imm<1> H, Vec Vn,
                          Vec Vd) {
        return false;
    }
    virtual bool SQRDMLSH_elt_2(bool Q, Imm<2> size, Imm<1> L, Imm<1> M, Imm<4> Vmlo, Imm<1> H,
                                Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FMULX_elt_3(bool Q, Imm<1> L, Imm<1> M, Imm<4> Vmlo, Imm<1> H, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FMULX_elt_4(bool Q, bool sz, Imm<1> L, Imm<1> M, Imm<4> Vmlo, Imm<1> H, Vec Vn,
                             Vec Vd) {
        return false;
    }
    virtual bool FCMLA_elt(bool Q, Imm<2> size, Imm<1> L, Imm<1> M, Imm<4> Vmlo, Imm<2> rot,
                           Imm<1> H, Vec Vn, Vec Vd) {
        return false;
    }

    // Data Processing - FP and SIMD - Cryptographic three register
    virtual bool SM3TT1A(Vec Vm, Imm<2> imm2, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool SM3TT1B(Vec Vm, Imm<2> imm2, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool SM3TT2A(Vec Vm, Imm<2> imm2, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool SM3TT2B(Vec Vm, Imm<2> imm2, Vec Vn, Vec Vd) {
        return false;
    }

    // Data Processing - FP and SIMD - SHA512 three register
    virtual bool SHA512H(Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool SHA512H2(Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool SHA512SU1(Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool RAX1(Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool XAR(Vec Vm, Imm<6> imm6, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool SM3PARTW1(Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool SM3PARTW2(Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool SM4EKEY(Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }

    // Data Processing - FP and SIMD - Cryptographic four register
    virtual bool EOR3(Vec Vm, Vec Va, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool BCAX(Vec Vm, Vec Va, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool SM3SS1(Vec Vm, Vec Va, Vec Vn, Vec Vd) {
        return false;
    }

    // Data Processing - FP and SIMD - SHA512 two register
    virtual bool SHA512SU0(Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool SM4E(Vec Vn, Vec Vd) {
        return false;
    }

    // Data Processing - FP and SIMD - Conversion between floating point and fixed point
    virtual bool SCVTF_float_fix(bool sf, Imm<2> type, Imm<6> scale, Reg Rn, Vec Vd) {
        return false;
    }
    virtual bool UCVTF_float_fix(bool sf, Imm<2> type, Imm<6> scale, Reg Rn, Vec Vd) {
        return false;
    }
    virtual bool FCVTZS_float_fix(bool sf, Imm<2> type, Imm<6> scale, Vec Vn, Reg Rd) {
        return false;
    }
    virtual bool FCVTZU_float_fix(bool sf, Imm<2> type, Imm<6> scale, Vec Vn, Reg Rd) {
        return false;
    }

    // Data Processing - FP and SIMD - Conversion between floating point and integer
    virtual bool FCVTNS_float(bool sf, Imm<2> type, Vec Vn, Reg Rd) {
        return false;
    }
    virtual bool FCVTNU_float(bool sf, Imm<2> type, Vec Vn, Reg Rd) {
        return false;
    }
    virtual bool SCVTF_float_int(bool sf, Imm<2> type, Reg Rn, Vec Vd) {
        return false;
    }
    virtual bool UCVTF_float_int(bool sf, Imm<2> type, Reg Rn, Vec Vd) {
        return false;
    }
    virtual bool FCVTAS_float(bool sf, Imm<2> type, Vec Vn, Reg Rd) {
        return false;
    }
    virtual bool FCVTAU_float(bool sf, Imm<2> type, Vec Vn, Reg Rd) {
        return false;
    }
    virtual bool FMOV_float_gen(bool sf, Imm<2> type, Imm<1> rmode_0, Imm<1> opc_0, size_t n,
                                size_t d) {
        return false;
    }
    virtual bool FCVTPS_float(bool sf, Imm<2> type, Vec Vn, Reg Rd) {
        return false;
    }
    virtual bool FCVTPU_float(bool sf, Imm<2> type, Vec Vn, Reg Rd) {
        return false;
    }
    virtual bool FCVTMS_float(bool sf, Imm<2> type, Vec Vn, Reg Rd) {
        return false;
    }
    virtual bool FCVTMU_float(bool sf, Imm<2> type, Vec Vn, Reg Rd) {
        return false;
    }
    virtual bool FCVTZS_float_int(bool sf, Imm<2> type, Vec Vn, Reg Rd) {
        return false;
    }
    virtual bool FCVTZU_float_int(bool sf, Imm<2> type, Vec Vn, Reg Rd) {
        return false;
    }
    virtual bool FJCVTZS(Vec Vn, Reg Rd) {
        return false;
    }

    // Data Processing - FP and SIMD - Floating point data processing
    virtual bool FMOV_float(Imm<2> type, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FABS_float(Imm<2> type, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FNEG_float(Imm<2> type, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FSQRT_float(Imm<2> type, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FCVT_float(Imm<2> type, Imm<2> opc, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FRINTN_float(Imm<2> type, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FRINTP_float(Imm<2> type, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FRINTM_float(Imm<2> type, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FRINTZ_float(Imm<2> type, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FRINTA_float(Imm<2> type, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FRINTX_float(Imm<2> type, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FRINTI_float(Imm<2> type, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FRINT32X_float(Imm<2> type, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FRINT64X_float(Imm<2> type, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FRINT32Z_float(Imm<2> type, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FRINT64Z_float(Imm<2> type, Vec Vn, Vec Vd) {
        return false;
    }

    // Data Processing - FP and SIMD - Floating point compare
    virtual bool FCMP_float(Imm<2> type, Vec Vm, Vec Vn, bool cmp_with_zero) {
        return false;
    }
    virtual bool FCMPE_float(Imm<2> type, Vec Vm, Vec Vn, bool cmp_with_zero) {
        return false;
    }

    // Data Processing - FP and SIMD - Floating point immediate
    virtual bool FMOV_float_imm(Imm<2> type, Imm<8> imm8, Vec Vd) {
        return false;
    }

    // Data Processing - FP and SIMD - Floating point conditional compare
    virtual bool FCCMP_float(Imm<2> type, Vec Vm, Cond cond, Vec Vn, Imm<4> nzcv) {
        return false;
    }
    virtual bool FCCMPE_float(Imm<2> type, Vec Vm, Cond cond, Vec Vn, Imm<4> nzcv) {
        return false;
    }

    // Data Processing - FP and SIMD - Floating point data processing two register
    virtual bool FMUL_float(Imm<2> type, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FDIV_float(Imm<2> type, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FADD_float(Imm<2> type, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FSUB_float(Imm<2> type, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FMAX_float(Imm<2> type, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FMIN_float(Imm<2> type, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FMAXNM_float(Imm<2> type, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FMINNM_float(Imm<2> type, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FNMUL_float(Imm<2> type, Vec Vm, Vec Vn, Vec Vd) {
        return false;
    }

    // Data Processing - FP and SIMD - Floating point conditional select
    virtual bool FCSEL_float(Imm<2> type, Vec Vm, Cond cond, Vec Vn, Vec Vd) {
        return false;
    }

    // Data Processing - FP and SIMD - Floating point data processing three register
    virtual bool FMADD_float(Imm<2> type, Vec Vm, Vec Va, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FMSUB_float(Imm<2> type, Vec Vm, Vec Va, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FNMADD_float(Imm<2> type, Vec Vm, Vec Va, Vec Vn, Vec Vd) {
        return false;
    }
    virtual bool FNMSUB_float(Imm<2> type, Vec Vm, Vec Va, Vec Vn, Vec Vd) {
        return false;
    }
};

} // namespace Core
