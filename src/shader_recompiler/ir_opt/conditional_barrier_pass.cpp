// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "shader_recompiler/frontend/ir/program.h"
#include "shader_recompiler/ir_opt/passes.h"

namespace Shader::Optimization {

void ConditionalBarrierPass(IR::Program& program) {
    s32 conditional_control_flow_count{0};
    s32 conditional_return_count{0};
    for (IR::AbstractSyntaxNode& node : program.syntax_list) {
        switch (node.type) {
        case IR::AbstractSyntaxNode::Type::If:
        case IR::AbstractSyntaxNode::Type::Loop:
            conditional_control_flow_count++;
            break;
        case IR::AbstractSyntaxNode::Type::EndIf:
        case IR::AbstractSyntaxNode::Type::Repeat:
            conditional_control_flow_count--;
            break;
        case IR::AbstractSyntaxNode::Type::Unreachable:
        case IR::AbstractSyntaxNode::Type::Return:
            if (conditional_control_flow_count > 0) {
                conditional_return_count++;
            }
            break;
        case IR::AbstractSyntaxNode::Type::Block:
            for (IR::Inst& inst : node.data.block->Instructions()) {
                if ((conditional_control_flow_count > 0 || conditional_return_count > 0) &&
                    inst.GetOpcode() == IR::Opcode::Barrier) {
                    LOG_WARNING(Shader, "Barrier within conditional control flow");
                    inst.ReplaceOpcode(IR::Opcode::Identity);
                }
            }
            break;
        default:
            break;
        }
    }
    ASSERT(conditional_control_flow_count == 0);
}

} // namespace Shader::Optimization
