// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "shader_recompiler/frontend/ir/basic_block.h"
#include "shader_recompiler/frontend/ir/ir_emitter.h"
#include "shader_recompiler/frontend/ir/value.h"
#include "shader_recompiler/ir_opt/passes.h"

namespace Shader::Optimization {

namespace {
void AddingByteSwapsWorkaround(IR::Block& block, IR::Inst& inst) {
    /*
     * Workaround for an NVIDIA bug seen in Super Mario RPG
     *
     * We are looking for this pattern:
     *   %lhs_bfe = BitFieldUExtract %factor_a, #0, #16
     *   %lhs_mul = IMul32 %lhs_bfe, %factor_b           // potentially optional?
     *   %lhs_shl = ShiftLeftLogical32 %lhs_mul, #16
     *   %rhs_bfe = BitFieldUExtract %factor_a, #16, #16
     *   %result  = IAdd32 %lhs_shl, %rhs_bfe
     *
     * And replacing the IAdd32 with a BitwiseOr32
     *   %result  = BitwiseOr32 %lhs_shl, %rhs_bfe
     *
     */
    IR::Inst* const lhs_shl{inst.Arg(0).TryInstRecursive()};
    IR::Inst* const rhs_bfe{inst.Arg(1).TryInstRecursive()};
    if (!lhs_shl || !rhs_bfe) {
        return;
    }
    if (lhs_shl->GetOpcode() != IR::Opcode::ShiftLeftLogical32 ||
        lhs_shl->Arg(1) != IR::Value{16U}) {
        return;
    }
    if (rhs_bfe->GetOpcode() != IR::Opcode::BitFieldUExtract || rhs_bfe->Arg(1) != IR::Value{16U} ||
        rhs_bfe->Arg(2) != IR::Value{16U}) {
        return;
    }
    IR::Inst* const lhs_mul{lhs_shl->Arg(0).TryInstRecursive()};
    if (!lhs_mul) {
        return;
    }
    const bool lhs_mul_optional{lhs_mul->GetOpcode() == IR::Opcode::BitFieldUExtract};
    if (lhs_mul->GetOpcode() != IR::Opcode::IMul32 &&
        lhs_mul->GetOpcode() != IR::Opcode::BitFieldUExtract) {
        return;
    }
    IR::Inst* const lhs_bfe{lhs_mul_optional ? lhs_mul : lhs_mul->Arg(0).TryInstRecursive()};
    if (!lhs_bfe) {
        return;
    }
    if (lhs_bfe->GetOpcode() != IR::Opcode::BitFieldUExtract) {
        return;
    }
    if (lhs_bfe->Arg(1) != IR::Value{0U} || lhs_bfe->Arg(2) != IR::Value{16U}) {
        return;
    }
    IR::IREmitter ir{block, IR::Block::InstructionList::s_iterator_to(inst)};
    inst.ReplaceUsesWith(ir.BitwiseOr(IR::U32{inst.Arg(0)}, IR::U32{inst.Arg(1)}));
}

} // Anonymous namespace

void VendorWorkaroundPass(IR::Program& program) {
    for (IR::Block* const block : program.post_order_blocks) {
        for (IR::Inst& inst : block->Instructions()) {
            switch (inst.GetOpcode()) {
            case IR::Opcode::IAdd32:
                AddingByteSwapsWorkaround(*block, inst);
                break;
            default:
                break;
            }
        }
    }
}

} // namespace Shader::Optimization
