// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-FileCopyrightText: Copyright 2023 merryhime <https://mary.rs>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/bit_cast.h"
#include "core/arm/nce/interpreter_visitor.h"

namespace Core {

template <u32 BitSize>
u64 SignExtendToLong(u64 value) {
    u64 mask = 1ULL << (BitSize - 1);
    value &= (1ULL << BitSize) - 1;
    return (value ^ mask) - mask;
}

static u64 SignExtendToLong(u64 value, u64 bitsize) {
    switch (bitsize) {
    case 8:
        return SignExtendToLong<8>(value);
    case 16:
        return SignExtendToLong<16>(value);
    case 32:
        return SignExtendToLong<32>(value);
    default:
        return value;
    }
}

template <u64 BitSize>
u32 SignExtendToWord(u32 value) {
    u32 mask = 1ULL << (BitSize - 1);
    value &= (1ULL << BitSize) - 1;
    return (value ^ mask) - mask;
}

static u32 SignExtendToWord(u32 value, u64 bitsize) {
    switch (bitsize) {
    case 8:
        return SignExtendToWord<8>(value);
    case 16:
        return SignExtendToWord<16>(value);
    default:
        return value;
    }
}

static u64 SignExtend(u64 value, u64 bitsize, u64 regsize) {
    if (regsize == 64) {
        return SignExtendToLong(value, bitsize);
    } else {
        return SignExtendToWord(static_cast<u32>(value), bitsize);
    }
}

static u128 VectorGetElement(u128 value, u64 bitsize) {
    switch (bitsize) {
    case 8:
        return {value[0] & ((1ULL << 8) - 1), 0};
    case 16:
        return {value[0] & ((1ULL << 16) - 1), 0};
    case 32:
        return {value[0] & ((1ULL << 32) - 1), 0};
    case 64:
        return {value[0], 0};
    default:
        return value;
    }
}

u64 InterpreterVisitor::ExtendReg(size_t bitsize, Reg reg, Imm<3> option, u8 shift) {
    ASSERT(shift <= 4);
    ASSERT(bitsize == 32 || bitsize == 64);
    u64 val = this->GetReg(reg);
    size_t len;
    u64 extended;
    bool signed_extend;

    switch (option.ZeroExtend()) {
    case 0b000: { // UXTB
        val &= ((1ULL << 8) - 1);
        len = 8;
        signed_extend = false;
        break;
    }
    case 0b001: { // UXTH
        val &= ((1ULL << 16) - 1);
        len = 16;
        signed_extend = false;
        break;
    }
    case 0b010: { // UXTW
        val &= ((1ULL << 32) - 1);
        len = 32;
        signed_extend = false;
        break;
    }
    case 0b011: { // UXTX
        len = 64;
        signed_extend = false;
        break;
    }
    case 0b100: { // SXTB
        val &= ((1ULL << 8) - 1);
        len = 8;
        signed_extend = true;
        break;
    }
    case 0b101: { // SXTH
        val &= ((1ULL << 16) - 1);
        len = 16;
        signed_extend = true;
        break;
    }
    case 0b110: { // SXTW
        val &= ((1ULL << 32) - 1);
        len = 32;
        signed_extend = true;
        break;
    }
    case 0b111: { // SXTX
        len = 64;
        signed_extend = true;
        break;
    }
    default:
        UNREACHABLE();
    }

    if (len < bitsize && signed_extend) {
        extended = SignExtend(val, len, bitsize);
    } else {
        extended = val;
    }

    return extended << shift;
}

u128 InterpreterVisitor::GetVec(Vec v) {
    return m_fpsimd_regs[static_cast<u32>(v)];
}

u64 InterpreterVisitor::GetReg(Reg r) {
    return m_regs[static_cast<u32>(r)];
}

u64 InterpreterVisitor::GetSp() {
    return m_sp;
}

u64 InterpreterVisitor::GetPc() {
    return m_pc;
}

void InterpreterVisitor::SetVec(Vec v, u128 value) {
    m_fpsimd_regs[static_cast<u32>(v)] = value;
}

void InterpreterVisitor::SetReg(Reg r, u64 value) {
    m_regs[static_cast<u32>(r)] = value;
}

void InterpreterVisitor::SetSp(u64 value) {
    m_sp = value;
}

bool InterpreterVisitor::Ordered(size_t size, bool L, bool o0, Reg Rn, Reg Rt) {
    const auto memop = L ? MemOp::Load : MemOp::Store;
    const size_t elsize = 8 << size;
    const size_t datasize = elsize;

    // Operation
    const size_t dbytes = datasize / 8;

    u64 address;
    if (Rn == Reg::SP) {
        address = this->GetSp();
    } else {
        address = this->GetReg(Rn);
    }

    switch (memop) {
    case MemOp::Store: {
        std::atomic_thread_fence(std::memory_order_seq_cst);
        u64 value = this->GetReg(Rt);
        m_memory.WriteBlock(address, &value, dbytes);
        std::atomic_thread_fence(std::memory_order_seq_cst);
        break;
    }
    case MemOp::Load: {
        u64 value = 0;
        m_memory.ReadBlock(address, &value, dbytes);
        this->SetReg(Rt, value);
        std::atomic_thread_fence(std::memory_order_seq_cst);
        break;
    }
    default:
        UNREACHABLE();
    }

    return true;
}

bool InterpreterVisitor::STLLR(Imm<2> sz, Reg Rn, Reg Rt) {
    const size_t size = sz.ZeroExtend<size_t>();
    const bool L = 0;
    const bool o0 = 0;
    return this->Ordered(size, L, o0, Rn, Rt);
}

bool InterpreterVisitor::STLR(Imm<2> sz, Reg Rn, Reg Rt) {
    const size_t size = sz.ZeroExtend<size_t>();
    const bool L = 0;
    const bool o0 = 1;
    return this->Ordered(size, L, o0, Rn, Rt);
}

bool InterpreterVisitor::LDLAR(Imm<2> sz, Reg Rn, Reg Rt) {
    const size_t size = sz.ZeroExtend<size_t>();
    const bool L = 1;
    const bool o0 = 0;
    return this->Ordered(size, L, o0, Rn, Rt);
}

bool InterpreterVisitor::LDAR(Imm<2> sz, Reg Rn, Reg Rt) {
    const size_t size = sz.ZeroExtend<size_t>();
    const bool L = 1;
    const bool o0 = 1;
    return this->Ordered(size, L, o0, Rn, Rt);
}

bool InterpreterVisitor::LDR_lit_gen(bool opc_0, Imm<19> imm19, Reg Rt) {
    const size_t size = opc_0 == 0 ? 4 : 8;
    const s64 offset = Dynarmic::concatenate(imm19, Imm<2>{0}).SignExtend<s64>();
    const u64 address = this->GetPc() + offset;

    u64 data = 0;
    m_memory.ReadBlock(address, &data, size);

    this->SetReg(Rt, data);
    return true;
}

bool InterpreterVisitor::LDR_lit_fpsimd(Imm<2> opc, Imm<19> imm19, Vec Vt) {
    if (opc == 0b11) {
        // Unallocated encoding
        return false;
    }

    // Size in bytes
    const u64 size = 4 << opc.ZeroExtend();
    const u64 offset = imm19.SignExtend<u64>() << 2;
    const u64 address = this->GetPc() + offset;

    u128 data{};
    m_memory.ReadBlock(address, &data, size);
    this->SetVec(Vt, data);
    return true;
}

bool InterpreterVisitor::STP_LDP_gen(Imm<2> opc, bool not_postindex, bool wback, Imm<1> L,
                                     Imm<7> imm7, Reg Rt2, Reg Rn, Reg Rt) {
    if ((L == 0 && opc.Bit<0>() == 1) || opc == 0b11) {
        // Unallocated encoding
        return false;
    }

    const auto memop = L == 1 ? MemOp::Load : MemOp::Store;
    if (memop == MemOp::Load && wback && (Rt == Rn || Rt2 == Rn) && Rn != Reg::R31) {
        // Unpredictable instruction
        return false;
    }
    if (memop == MemOp::Store && wback && (Rt == Rn || Rt2 == Rn) && Rn != Reg::R31) {
        // Unpredictable instruction
        return false;
    }
    if (memop == MemOp::Load && Rt == Rt2) {
        // Unpredictable instruction
        return false;
    }

    u64 address;
    if (Rn == Reg::SP) {
        address = this->GetSp();
    } else {
        address = this->GetReg(Rn);
    }

    const bool postindex = !not_postindex;
    const bool signed_ = opc.Bit<0>() != 0;
    const size_t scale = 2 + opc.Bit<1>();
    const size_t datasize = 8 << scale;
    const u64 offset = imm7.SignExtend<u64>() << scale;

    if (!postindex) {
        address += offset;
    }

    const size_t dbytes = datasize / 8;
    switch (memop) {
    case MemOp::Store: {
        u64 data1 = this->GetReg(Rt);
        u64 data2 = this->GetReg(Rt2);
        m_memory.WriteBlock(address, &data1, dbytes);
        m_memory.WriteBlock(address + dbytes, &data2, dbytes);
        break;
    }
    case MemOp::Load: {
        u64 data1 = 0, data2 = 0;
        m_memory.ReadBlock(address, &data1, dbytes);
        m_memory.ReadBlock(address + dbytes, &data2, dbytes);
        if (signed_) {
            this->SetReg(Rt, SignExtend(data1, datasize, 64));
            this->SetReg(Rt2, SignExtend(data2, datasize, 64));
        } else {
            this->SetReg(Rt, data1);
            this->SetReg(Rt2, data2);
        }
        break;
    }
    default:
        UNREACHABLE();
    }

    if (wback) {
        if (postindex) {
            address += offset;
        }

        if (Rn == Reg::SP) {
            this->SetSp(address);
        } else {
            this->SetReg(Rn, address);
        }
    }

    return true;
}

bool InterpreterVisitor::STP_LDP_fpsimd(Imm<2> opc, bool not_postindex, bool wback, Imm<1> L,
                                        Imm<7> imm7, Vec Vt2, Reg Rn, Vec Vt) {
    if (opc == 0b11) {
        // Unallocated encoding
        return false;
    }

    const auto memop = L == 1 ? MemOp::Load : MemOp::Store;
    if (memop == MemOp::Load && Vt == Vt2) {
        // Unpredictable instruction
        return false;
    }

    u64 address;
    if (Rn == Reg::SP) {
        address = this->GetSp();
    } else {
        address = this->GetReg(Rn);
    }

    const bool postindex = !not_postindex;
    const size_t scale = 2 + opc.ZeroExtend<size_t>();
    const size_t datasize = 8 << scale;
    const u64 offset = imm7.SignExtend<u64>() << scale;
    const size_t dbytes = datasize / 8;

    if (!postindex) {
        address += offset;
    }

    switch (memop) {
    case MemOp::Store: {
        u128 data1 = VectorGetElement(this->GetVec(Vt), datasize);
        u128 data2 = VectorGetElement(this->GetVec(Vt2), datasize);
        m_memory.WriteBlock(address, &data1, dbytes);
        m_memory.WriteBlock(address + dbytes, &data2, dbytes);
        break;
    }
    case MemOp::Load: {
        u128 data1{}, data2{};
        m_memory.ReadBlock(address, &data1, dbytes);
        m_memory.ReadBlock(address + dbytes, &data2, dbytes);
        this->SetVec(Vt, data1);
        this->SetVec(Vt2, data2);
        break;
    }
    default:
        UNREACHABLE();
    }

    if (wback) {
        if (postindex) {
            address += offset;
        }

        if (Rn == Reg::SP) {
            this->SetSp(address);
        } else {
            this->SetReg(Rn, address);
        }
    }

    return true;
}

bool InterpreterVisitor::RegisterImmediate(bool wback, bool postindex, size_t scale, u64 offset,
                                           Imm<2> size, Imm<2> opc, Reg Rn, Reg Rt) {
    MemOp memop;
    bool signed_ = false;
    size_t regsize = 0;

    if (opc.Bit<1>() == 0) {
        memop = opc.Bit<0>() ? MemOp::Load : MemOp::Store;
        regsize = size == 0b11 ? 64 : 32;
        signed_ = false;
    } else if (size == 0b11) {
        memop = MemOp::Prefetch;
        ASSERT(!opc.Bit<0>());
    } else {
        memop = MemOp::Load;
        ASSERT(!(size == 0b10 && opc.Bit<0>() == 1));
        regsize = opc.Bit<0>() ? 32 : 64;
        signed_ = true;
    }

    if (memop == MemOp::Load && wback && Rn == Rt && Rn != Reg::R31) {
        // Unpredictable instruction
        return false;
    }
    if (memop == MemOp::Store && wback && Rn == Rt && Rn != Reg::R31) {
        // Unpredictable instruction
        return false;
    }

    u64 address;
    if (Rn == Reg::SP) {
        address = this->GetSp();
    } else {
        address = this->GetReg(Rn);
    }
    if (!postindex) {
        address += offset;
    }

    const size_t datasize = 8 << scale;
    switch (memop) {
    case MemOp::Store: {
        u64 data = this->GetReg(Rt);
        m_memory.WriteBlock(address, &data, datasize / 8);
        break;
    }
    case MemOp::Load: {
        u64 data = 0;
        m_memory.ReadBlock(address, &data, datasize / 8);
        if (signed_) {
            this->SetReg(Rt, SignExtend(data, datasize, regsize));
        } else {
            this->SetReg(Rt, data);
        }
        break;
    }
    case MemOp::Prefetch:
        // this->Prefetch(address, Rt)
        break;
    }

    if (wback) {
        if (postindex) {
            address += offset;
        }

        if (Rn == Reg::SP) {
            this->SetSp(address);
        } else {
            this->SetReg(Rn, address);
        }
    }

    return true;
}

bool InterpreterVisitor::STRx_LDRx_imm_1(Imm<2> size, Imm<2> opc, Imm<9> imm9, bool not_postindex,
                                         Reg Rn, Reg Rt) {
    const bool wback = true;
    const bool postindex = !not_postindex;
    const size_t scale = size.ZeroExtend<size_t>();
    const u64 offset = imm9.SignExtend<u64>();

    return this->RegisterImmediate(wback, postindex, scale, offset, size, opc, Rn, Rt);
}

bool InterpreterVisitor::STRx_LDRx_imm_2(Imm<2> size, Imm<2> opc, Imm<12> imm12, Reg Rn, Reg Rt) {
    const bool wback = false;
    const bool postindex = false;
    const size_t scale = size.ZeroExtend<size_t>();
    const u64 offset = imm12.ZeroExtend<u64>() << scale;

    return this->RegisterImmediate(wback, postindex, scale, offset, size, opc, Rn, Rt);
}

bool InterpreterVisitor::STURx_LDURx(Imm<2> size, Imm<2> opc, Imm<9> imm9, Reg Rn, Reg Rt) {
    const bool wback = false;
    const bool postindex = false;
    const size_t scale = size.ZeroExtend<size_t>();
    const u64 offset = imm9.SignExtend<u64>();

    return this->RegisterImmediate(wback, postindex, scale, offset, size, opc, Rn, Rt);
}

bool InterpreterVisitor::SIMDImmediate(bool wback, bool postindex, size_t scale, u64 offset,
                                       MemOp memop, Reg Rn, Vec Vt) {
    const size_t datasize = 8 << scale;

    u64 address;
    if (Rn == Reg::SP) {
        address = this->GetSp();
    } else {
        address = this->GetReg(Rn);
    }

    if (!postindex) {
        address += offset;
    }

    switch (memop) {
    case MemOp::Store: {
        u128 data = VectorGetElement(this->GetVec(Vt), datasize);
        m_memory.WriteBlock(address, &data, datasize / 8);
        break;
    }
    case MemOp::Load: {
        u128 data{};
        m_memory.ReadBlock(address, &data, datasize / 8);
        this->SetVec(Vt, data);
        break;
    }
    default:
        UNREACHABLE();
    }

    if (wback) {
        if (postindex) {
            address += offset;
        }

        if (Rn == Reg::SP) {
            this->SetSp(address);
        } else {
            this->SetReg(Rn, address);
        }
    }

    return true;
}

bool InterpreterVisitor::STR_imm_fpsimd_1(Imm<2> size, Imm<1> opc_1, Imm<9> imm9,
                                          bool not_postindex, Reg Rn, Vec Vt) {
    const size_t scale = Dynarmic::concatenate(opc_1, size).ZeroExtend<size_t>();
    if (scale > 4) {
        // Unallocated encoding
        return false;
    }

    const bool wback = true;
    const bool postindex = !not_postindex;
    const u64 offset = imm9.SignExtend<u64>();

    return this->SIMDImmediate(wback, postindex, scale, offset, MemOp::Store, Rn, Vt);
}

bool InterpreterVisitor::STR_imm_fpsimd_2(Imm<2> size, Imm<1> opc_1, Imm<12> imm12, Reg Rn,
                                          Vec Vt) {
    const size_t scale = Dynarmic::concatenate(opc_1, size).ZeroExtend<size_t>();
    if (scale > 4) {
        // Unallocated encoding
        return false;
    }

    const bool wback = false;
    const bool postindex = false;
    const u64 offset = imm12.ZeroExtend<u64>() << scale;

    return this->SIMDImmediate(wback, postindex, scale, offset, MemOp::Store, Rn, Vt);
}

bool InterpreterVisitor::LDR_imm_fpsimd_1(Imm<2> size, Imm<1> opc_1, Imm<9> imm9,
                                          bool not_postindex, Reg Rn, Vec Vt) {
    const size_t scale = Dynarmic::concatenate(opc_1, size).ZeroExtend<size_t>();
    if (scale > 4) {
        // Unallocated encoding
        return false;
    }

    const bool wback = true;
    const bool postindex = !not_postindex;
    const u64 offset = imm9.SignExtend<u64>();

    return this->SIMDImmediate(wback, postindex, scale, offset, MemOp::Load, Rn, Vt);
}

bool InterpreterVisitor::LDR_imm_fpsimd_2(Imm<2> size, Imm<1> opc_1, Imm<12> imm12, Reg Rn,
                                          Vec Vt) {
    const size_t scale = Dynarmic::concatenate(opc_1, size).ZeroExtend<size_t>();
    if (scale > 4) {
        // Unallocated encoding
        return false;
    }

    const bool wback = false;
    const bool postindex = false;
    const u64 offset = imm12.ZeroExtend<u64>() << scale;

    return this->SIMDImmediate(wback, postindex, scale, offset, MemOp::Load, Rn, Vt);
}

bool InterpreterVisitor::STUR_fpsimd(Imm<2> size, Imm<1> opc_1, Imm<9> imm9, Reg Rn, Vec Vt) {
    const size_t scale = Dynarmic::concatenate(opc_1, size).ZeroExtend<size_t>();
    if (scale > 4) {
        // Unallocated encoding
        return false;
    }

    const bool wback = false;
    const bool postindex = false;
    const u64 offset = imm9.SignExtend<u64>();

    return this->SIMDImmediate(wback, postindex, scale, offset, MemOp::Store, Rn, Vt);
}

bool InterpreterVisitor::LDUR_fpsimd(Imm<2> size, Imm<1> opc_1, Imm<9> imm9, Reg Rn, Vec Vt) {
    const size_t scale = Dynarmic::concatenate(opc_1, size).ZeroExtend<size_t>();
    if (scale > 4) {
        // Unallocated encoding
        return false;
    }

    const bool wback = false;
    const bool postindex = false;
    const u64 offset = imm9.SignExtend<u64>();

    return this->SIMDImmediate(wback, postindex, scale, offset, MemOp::Load, Rn, Vt);
}

bool InterpreterVisitor::RegisterOffset(size_t scale, u8 shift, Imm<2> size, Imm<1> opc_1,
                                        Imm<1> opc_0, Reg Rm, Imm<3> option, Reg Rn, Reg Rt) {
    MemOp memop;
    size_t regsize = 64;
    bool signed_ = false;

    if (opc_1 == 0) {
        memop = opc_0 == 1 ? MemOp::Load : MemOp::Store;
        regsize = size == 0b11 ? 64 : 32;
        signed_ = false;
    } else if (size == 0b11) {
        memop = MemOp::Prefetch;
        if (opc_0 == 1) {
            // Unallocated encoding
            return false;
        }
    } else {
        memop = MemOp::Load;
        if (size == 0b10 && opc_0 == 1) {
            // Unallocated encoding
            return false;
        }
        regsize = opc_0 == 1 ? 32 : 64;
        signed_ = true;
    }

    const size_t datasize = 8 << scale;

    // Operation
    const u64 offset = this->ExtendReg(64, Rm, option, shift);

    u64 address;
    if (Rn == Reg::SP) {
        address = this->GetSp();
    } else {
        address = this->GetReg(Rn);
    }
    address += offset;

    switch (memop) {
    case MemOp::Store: {
        u64 data = this->GetReg(Rt);
        m_memory.WriteBlock(address, &data, datasize / 8);
        break;
    }
    case MemOp::Load: {
        u64 data = 0;
        m_memory.ReadBlock(address, &data, datasize / 8);
        if (signed_) {
            this->SetReg(Rt, SignExtend(data, datasize, regsize));
        } else {
            this->SetReg(Rt, data);
        }
        break;
    }
    case MemOp::Prefetch:
        break;
    }

    return true;
}

bool InterpreterVisitor::STRx_reg(Imm<2> size, Imm<1> opc_1, Reg Rm, Imm<3> option, bool S, Reg Rn,
                                  Reg Rt) {
    const Imm<1> opc_0{0};
    const size_t scale = size.ZeroExtend<size_t>();
    const u8 shift = S ? static_cast<u8>(scale) : 0;
    if (!option.Bit<1>()) {
        // Unallocated encoding
        return false;
    }
    return this->RegisterOffset(scale, shift, size, opc_1, opc_0, Rm, option, Rn, Rt);
}

bool InterpreterVisitor::LDRx_reg(Imm<2> size, Imm<1> opc_1, Reg Rm, Imm<3> option, bool S, Reg Rn,
                                  Reg Rt) {
    const Imm<1> opc_0{1};
    const size_t scale = size.ZeroExtend<size_t>();
    const u8 shift = S ? static_cast<u8>(scale) : 0;
    if (!option.Bit<1>()) {
        // Unallocated encoding
        return false;
    }
    return this->RegisterOffset(scale, shift, size, opc_1, opc_0, Rm, option, Rn, Rt);
}

bool InterpreterVisitor::SIMDOffset(size_t scale, u8 shift, Imm<1> opc_0, Reg Rm, Imm<3> option,
                                    Reg Rn, Vec Vt) {
    const auto memop = opc_0 == 1 ? MemOp::Load : MemOp::Store;
    const size_t datasize = 8 << scale;

    // Operation
    const u64 offset = this->ExtendReg(64, Rm, option, shift);

    u64 address;
    if (Rn == Reg::SP) {
        address = this->GetSp();
    } else {
        address = this->GetReg(Rn);
    }
    address += offset;

    switch (memop) {
    case MemOp::Store: {
        u128 data = VectorGetElement(this->GetVec(Vt), datasize);
        m_memory.WriteBlock(address, &data, datasize / 8);
        break;
    }
    case MemOp::Load: {
        u128 data{};
        m_memory.ReadBlock(address, &data, datasize / 8);
        this->SetVec(Vt, data);
        break;
    }
    default:
        UNREACHABLE();
    }

    return true;
}

bool InterpreterVisitor::STR_reg_fpsimd(Imm<2> size, Imm<1> opc_1, Reg Rm, Imm<3> option, bool S,
                                        Reg Rn, Vec Vt) {
    const Imm<1> opc_0{0};
    const size_t scale = Dynarmic::concatenate(opc_1, size).ZeroExtend<size_t>();
    if (scale > 4) {
        // Unallocated encoding
        return false;
    }
    const u8 shift = S ? static_cast<u8>(scale) : 0;
    if (!option.Bit<1>()) {
        // Unallocated encoding
        return false;
    }
    return this->SIMDOffset(scale, shift, opc_0, Rm, option, Rn, Vt);
}

bool InterpreterVisitor::LDR_reg_fpsimd(Imm<2> size, Imm<1> opc_1, Reg Rm, Imm<3> option, bool S,
                                        Reg Rn, Vec Vt) {
    const Imm<1> opc_0{1};
    const size_t scale = Dynarmic::concatenate(opc_1, size).ZeroExtend<size_t>();
    if (scale > 4) {
        // Unallocated encoding
        return false;
    }
    const u8 shift = S ? static_cast<u8>(scale) : 0;
    if (!option.Bit<1>()) {
        // Unallocated encoding
        return false;
    }
    return this->SIMDOffset(scale, shift, opc_0, Rm, option, Rn, Vt);
}

std::optional<u64> MatchAndExecuteOneInstruction(Core::Memory::Memory& memory, mcontext_t* context,
                                                 fpsimd_context* fpsimd_context) {
    // Construct the interpreter.
    std::span<u64, 31> regs(reinterpret_cast<u64*>(context->regs), 31);
    std::span<u128, 32> vregs(reinterpret_cast<u128*>(fpsimd_context->vregs), 32);
    u64& sp = *reinterpret_cast<u64*>(&context->sp);
    const u64& pc = *reinterpret_cast<u64*>(&context->pc);

    InterpreterVisitor visitor(memory, regs, vregs, sp, pc);

    // Read the instruction at the program counter.
    u32 instruction = memory.Read32(pc);
    bool was_executed = false;

    // Interpret the instruction.
    if (auto decoder = Dynarmic::A64::Decode<VisitorBase>(instruction)) {
        was_executed = decoder->get().call(visitor, instruction);
    } else {
        LOG_ERROR(Core_ARM, "Unallocated encoding: {:#x}", instruction);
    }

    if (was_executed) {
        return pc + 4;
    }

    return std::nullopt;
}

} // namespace Core
