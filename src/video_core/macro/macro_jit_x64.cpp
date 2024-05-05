// SPDX-FileCopyrightText: Copyright 2020 sudachi Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <array>
#include <bitset>
#include <optional>

#include <xbyak/xbyak.h>

#include "common/assert.h"
#include "common/bit_field.h"
#include "common/logging/log.h"
#include "common/profiling.h"
#include "common/x64/xbyak_abi.h"
#include "common/x64/xbyak_util.h"
#include "video_core/engines/maxwell_3d.h"
#include "video_core/macro/macro_interpreter.h"
#include "video_core/macro/macro_jit_x64.h"

namespace Tegra {
namespace {
constexpr Xbyak::Reg64 STATE = Xbyak::util::rbx;
constexpr Xbyak::Reg32 RESULT = Xbyak::util::r10d;
constexpr Xbyak::Reg64 MAX_PARAMETER = Xbyak::util::r11;
constexpr Xbyak::Reg64 PARAMETERS = Xbyak::util::r12;
constexpr Xbyak::Reg32 METHOD_ADDRESS = Xbyak::util::r14d;
constexpr Xbyak::Reg64 BRANCH_HOLDER = Xbyak::util::r15;

constexpr std::bitset<32> PERSISTENT_REGISTERS = Common::X64::BuildRegSet({
    STATE,
    RESULT,
    MAX_PARAMETER,
    PARAMETERS,
    METHOD_ADDRESS,
    BRANCH_HOLDER,
});

// Arbitrarily chosen based on current booting games.
constexpr size_t MAX_CODE_SIZE = 0x10000;

std::bitset<32> PersistentCallerSavedRegs() {
    return PERSISTENT_REGISTERS & Common::X64::ABI_ALL_CALLER_SAVED;
}

class MacroJITx64Impl final : public Xbyak::CodeGenerator, public CachedMacro {
public:
    explicit MacroJITx64Impl(Engines::Maxwell3D& maxwell3d_, const std::vector<u32>& code_)
        : CodeGenerator{MAX_CODE_SIZE}, code{code_}, maxwell3d{maxwell3d_} {
        Compile();
    }

    void Execute(const std::vector<u32>& parameters, u32 method) override;

    void Compile_ALU(Macro::Opcode opcode);
    void Compile_AddImmediate(Macro::Opcode opcode);
    void Compile_ExtractInsert(Macro::Opcode opcode);
    void Compile_ExtractShiftLeftImmediate(Macro::Opcode opcode);
    void Compile_ExtractShiftLeftRegister(Macro::Opcode opcode);
    void Compile_Read(Macro::Opcode opcode);
    void Compile_Branch(Macro::Opcode opcode);

private:
    void Optimizer_ScanFlags();

    void Compile();
    bool Compile_NextInstruction();

    Xbyak::Reg32 Compile_FetchParameter();
    Xbyak::Reg32 Compile_GetRegister(u32 index, Xbyak::Reg32 dst);

    void Compile_ProcessResult(Macro::ResultOperation operation, u32 reg);
    void Compile_Send(Xbyak::Reg32 value);

    Macro::Opcode GetOpCode() const;

    struct JITState {
        Engines::Maxwell3D* maxwell3d{};
        std::array<u32, Macro::NUM_MACRO_REGISTERS> registers{};
        u32 carry_flag{};
    };
    static_assert(offsetof(JITState, maxwell3d) == 0, "Maxwell3D is not at 0x0");
    using ProgramType = void (*)(JITState*, const u32*, const u32*);

    struct OptimizerState {
        bool can_skip_carry{};
        bool has_delayed_pc{};
        bool zero_reg_skip{};
        bool skip_dummy_addimmediate{};
        bool optimize_for_method_move{};
        bool enable_asserts{};
    };
    OptimizerState optimizer{};

    std::optional<Macro::Opcode> next_opcode{};
    ProgramType program{nullptr};

    std::array<Xbyak::Label, MAX_CODE_SIZE> labels;
    std::array<Xbyak::Label, MAX_CODE_SIZE> delay_skip;
    Xbyak::Label end_of_code{};

    bool is_delay_slot{};
    u32 pc{};

    const std::vector<u32>& code;
    Engines::Maxwell3D& maxwell3d;
};

void MacroJITx64Impl::Execute(const std::vector<u32>& parameters, u32 method) {
    SUDACHI_PROFILE("MacroJIT", "Execute");
    ASSERT_OR_EXECUTE(program != nullptr, { return; });
    JITState state{};
    state.maxwell3d = &maxwell3d;
    state.registers = {};
    program(&state, parameters.data(), parameters.data() + parameters.size());
}

void MacroJITx64Impl::Compile_ALU(Macro::Opcode opcode) {
    const bool is_a_zero = opcode.src_a == 0;
    const bool is_b_zero = opcode.src_b == 0;
    const bool valid_operation = !is_a_zero && !is_b_zero;
    [[maybe_unused]] const bool is_move_operation = !is_a_zero && is_b_zero;
    const bool has_zero_register = is_a_zero || is_b_zero;
    const bool no_zero_reg_skip = opcode.alu_operation == Macro::ALUOperation::AddWithCarry ||
                                  opcode.alu_operation == Macro::ALUOperation::SubtractWithBorrow;

    Xbyak::Reg32 src_a;
    Xbyak::Reg32 src_b;

    if (!optimizer.zero_reg_skip || no_zero_reg_skip) {
        src_a = Compile_GetRegister(opcode.src_a, RESULT);
        src_b = Compile_GetRegister(opcode.src_b, eax);
    } else {
        if (!is_a_zero) {
            src_a = Compile_GetRegister(opcode.src_a, RESULT);
        }
        if (!is_b_zero) {
            src_b = Compile_GetRegister(opcode.src_b, eax);
        }
    }

    bool has_emitted = false;

    switch (opcode.alu_operation) {
    case Macro::ALUOperation::Add:
        if (optimizer.zero_reg_skip) {
            if (valid_operation) {
                add(src_a, src_b);
            }
        } else {
            add(src_a, src_b);
        }

        if (!optimizer.can_skip_carry) {
            setc(byte[STATE + offsetof(JITState, carry_flag)]);
        }
        break;
    case Macro::ALUOperation::AddWithCarry:
        bt(dword[STATE + offsetof(JITState, carry_flag)], 0);
        adc(src_a, src_b);
        setc(byte[STATE + offsetof(JITState, carry_flag)]);
        break;
    case Macro::ALUOperation::Subtract:
        if (optimizer.zero_reg_skip) {
            if (valid_operation) {
                sub(src_a, src_b);
                has_emitted = true;
            }
        } else {
            sub(src_a, src_b);
            has_emitted = true;
        }
        if (!optimizer.can_skip_carry && has_emitted) {
            setc(byte[STATE + offsetof(JITState, carry_flag)]);
        }
        break;
    case Macro::ALUOperation::SubtractWithBorrow:
        bt(dword[STATE + offsetof(JITState, carry_flag)], 0);
        sbb(src_a, src_b);
        setc(byte[STATE + offsetof(JITState, carry_flag)]);
        break;
    case Macro::ALUOperation::Xor:
        if (optimizer.zero_reg_skip) {
            if (valid_operation) {
                xor_(src_a, src_b);
            }
        } else {
            xor_(src_a, src_b);
        }
        break;
    case Macro::ALUOperation::Or:
        if (optimizer.zero_reg_skip) {
            if (valid_operation) {
                or_(src_a, src_b);
            }
        } else {
            or_(src_a, src_b);
        }
        break;
    case Macro::ALUOperation::And:
        if (optimizer.zero_reg_skip) {
            if (!has_zero_register) {
                and_(src_a, src_b);
            }
        } else {
            and_(src_a, src_b);
        }
        break;
    case Macro::ALUOperation::AndNot:
        if (optimizer.zero_reg_skip) {
            if (!is_a_zero) {
                not_(src_b);
                and_(src_a, src_b);
            }
        } else {
            not_(src_b);
            and_(src_a, src_b);
        }
        break;
    case Macro::ALUOperation::Nand:
        if (optimizer.zero_reg_skip) {
            if (!is_a_zero) {
                and_(src_a, src_b);
                not_(src_a);
            }
        } else {
            and_(src_a, src_b);
            not_(src_a);
        }
        break;
    default:
        UNIMPLEMENTED_MSG("Unimplemented ALU operation {}", opcode.alu_operation.Value());
        break;
    }
    Compile_ProcessResult(opcode.result_operation, opcode.dst);
}

void MacroJITx64Impl::Compile_AddImmediate(Macro::Opcode opcode) {
    if (optimizer.skip_dummy_addimmediate) {
        // Games tend to use this as an exit instruction placeholder. It's to encode an instruction
        // without doing anything. In our case we can just not emit anything.
        if (opcode.result_operation == Macro::ResultOperation::Move && opcode.dst == 0) {
            return;
        }
    }
    // Check for redundant moves
    if (optimizer.optimize_for_method_move &&
        opcode.result_operation == Macro::ResultOperation::MoveAndSetMethod) {
        if (next_opcode.has_value()) {
            const auto next = *next_opcode;
            if (next.result_operation == Macro::ResultOperation::MoveAndSetMethod &&
                opcode.dst == next.dst) {
                return;
            }
        }
    }
    if (optimizer.zero_reg_skip && opcode.src_a == 0) {
        if (opcode.immediate == 0) {
            xor_(RESULT, RESULT);
        } else {
            mov(RESULT, opcode.immediate);
        }
    } else {
        auto result = Compile_GetRegister(opcode.src_a, RESULT);
        if (opcode.immediate > 2) {
            add(result, opcode.immediate);
        } else if (opcode.immediate == 1) {
            inc(result);
        } else if (opcode.immediate < 0) {
            sub(result, opcode.immediate * -1);
        }
    }
    Compile_ProcessResult(opcode.result_operation, opcode.dst);
}

void MacroJITx64Impl::Compile_ExtractInsert(Macro::Opcode opcode) {
    auto dst = Compile_GetRegister(opcode.src_a, RESULT);
    auto src = Compile_GetRegister(opcode.src_b, eax);

    const u32 mask = ~(opcode.GetBitfieldMask() << opcode.bf_dst_bit);
    and_(dst, mask);
    shr(src, opcode.bf_src_bit);
    and_(src, opcode.GetBitfieldMask());
    shl(src, opcode.bf_dst_bit);
    or_(dst, src);

    Compile_ProcessResult(opcode.result_operation, opcode.dst);
}

void MacroJITx64Impl::Compile_ExtractShiftLeftImmediate(Macro::Opcode opcode) {
    const auto dst = Compile_GetRegister(opcode.src_a, ecx);
    const auto src = Compile_GetRegister(opcode.src_b, RESULT);

    shr(src, dst.cvt8());
    and_(src, opcode.GetBitfieldMask());
    shl(src, opcode.bf_dst_bit);

    Compile_ProcessResult(opcode.result_operation, opcode.dst);
}

void MacroJITx64Impl::Compile_ExtractShiftLeftRegister(Macro::Opcode opcode) {
    const auto dst = Compile_GetRegister(opcode.src_a, ecx);
    const auto src = Compile_GetRegister(opcode.src_b, RESULT);

    shr(src, opcode.bf_src_bit);
    and_(src, opcode.GetBitfieldMask());
    shl(src, dst.cvt8());

    Compile_ProcessResult(opcode.result_operation, opcode.dst);
}

void MacroJITx64Impl::Compile_Read(Macro::Opcode opcode) {
    if (optimizer.zero_reg_skip && opcode.src_a == 0) {
        if (opcode.immediate == 0) {
            xor_(RESULT, RESULT);
        } else {
            mov(RESULT, opcode.immediate);
        }
    } else {
        auto result = Compile_GetRegister(opcode.src_a, RESULT);
        if (opcode.immediate > 2) {
            add(result, opcode.immediate);
        } else if (opcode.immediate == 1) {
            inc(result);
        } else if (opcode.immediate < 0) {
            sub(result, opcode.immediate * -1);
        }
    }

    // Equivalent to Engines::Maxwell3D::GetRegisterValue:
    if (optimizer.enable_asserts) {
        Xbyak::Label pass_range_check;
        cmp(RESULT, static_cast<u32>(Engines::Maxwell3D::Regs::NUM_REGS));
        jb(pass_range_check);
        int3();
        L(pass_range_check);
    }
    mov(rax, qword[STATE]);
    mov(RESULT,
        dword[rax + offsetof(Engines::Maxwell3D, regs) +
              offsetof(Engines::Maxwell3D::Regs, reg_array) + RESULT.cvt64() * sizeof(u32)]);

    Compile_ProcessResult(opcode.result_operation, opcode.dst);
}

void Send(Engines::Maxwell3D* maxwell3d, Macro::MethodAddress method_address, u32 value) {
    maxwell3d->CallMethod(method_address.address, value, true);
}

void MacroJITx64Impl::Compile_Send(Xbyak::Reg32 value) {
    Common::X64::ABI_PushRegistersAndAdjustStack(*this, PersistentCallerSavedRegs(), 0);
    mov(Common::X64::ABI_PARAM1, qword[STATE]);
    mov(Common::X64::ABI_PARAM2, METHOD_ADDRESS);
    mov(Common::X64::ABI_PARAM3, value);
    Common::X64::CallFarFunction(*this, &Send);
    Common::X64::ABI_PopRegistersAndAdjustStack(*this, PersistentCallerSavedRegs(), 0);

    Xbyak::Label dont_process{};
    // Get increment
    test(METHOD_ADDRESS, 0x3f000);
    // If zero, method address doesn't update
    je(dont_process);

    mov(ecx, METHOD_ADDRESS);
    and_(METHOD_ADDRESS, 0xfff);
    shr(ecx, 12);
    and_(ecx, 0x3f);
    lea(eax, ptr[rcx + METHOD_ADDRESS.cvt64()]);
    sal(ecx, 12);
    or_(eax, ecx);

    mov(METHOD_ADDRESS, eax);

    L(dont_process);
}

void MacroJITx64Impl::Compile_Branch(Macro::Opcode opcode) {
    ASSERT_MSG(!is_delay_slot, "Executing a branch in a delay slot is not valid");
    const s32 jump_address =
        static_cast<s32>(pc) + static_cast<s32>(opcode.GetBranchTarget() / sizeof(s32));

    Xbyak::Label end;
    auto value = Compile_GetRegister(opcode.src_a, eax);
    cmp(value, 0); // test(value, value);
    if (optimizer.has_delayed_pc) {
        switch (opcode.branch_condition) {
        case Macro::BranchCondition::Zero:
            jne(end, T_NEAR);
            break;
        case Macro::BranchCondition::NotZero:
            je(end, T_NEAR);
            break;
        }

        if (opcode.branch_annul) {
            xor_(BRANCH_HOLDER, BRANCH_HOLDER);
            jmp(labels[jump_address], T_NEAR);
        } else {
            Xbyak::Label handle_post_exit{};
            Xbyak::Label skip{};
            jmp(skip, T_NEAR);

            L(handle_post_exit);
            xor_(BRANCH_HOLDER, BRANCH_HOLDER);
            jmp(labels[jump_address], T_NEAR);

            L(skip);
            mov(BRANCH_HOLDER, handle_post_exit);
            jmp(delay_skip[pc], T_NEAR);
        }
    } else {
        switch (opcode.branch_condition) {
        case Macro::BranchCondition::Zero:
            je(labels[jump_address], T_NEAR);
            break;
        case Macro::BranchCondition::NotZero:
            jne(labels[jump_address], T_NEAR);
            break;
        }
    }

    L(end);
}

void MacroJITx64Impl::Optimizer_ScanFlags() {
    optimizer.can_skip_carry = true;
    optimizer.has_delayed_pc = false;
    for (auto raw_op : code) {
        Macro::Opcode op{};
        op.raw = raw_op;

        if (op.operation == Macro::Operation::ALU) {
            // Scan for any ALU operations which actually use the carry flag, if they don't exist in
            // our current code we can skip emitting the carry flag handling operations
            if (op.alu_operation == Macro::ALUOperation::AddWithCarry ||
                op.alu_operation == Macro::ALUOperation::SubtractWithBorrow) {
                optimizer.can_skip_carry = false;
            }
        }

        if (op.operation == Macro::Operation::Branch) {
            if (!op.branch_annul) {
                optimizer.has_delayed_pc = true;
            }
        }
    }
}

void MacroJITx64Impl::Compile() {
    SUDACHI_PROFILE("MacroJIT", "Compile");
    labels.fill(Xbyak::Label());

    Common::X64::ABI_PushRegistersAndAdjustStack(*this, Common::X64::ABI_ALL_CALLEE_SAVED, 8);
    // JIT state
    mov(STATE, Common::X64::ABI_PARAM1);
    mov(PARAMETERS, Common::X64::ABI_PARAM2);
    mov(MAX_PARAMETER, Common::X64::ABI_PARAM3);
    xor_(RESULT, RESULT);
    xor_(METHOD_ADDRESS, METHOD_ADDRESS);
    xor_(BRANCH_HOLDER, BRANCH_HOLDER);

    mov(dword[STATE + offsetof(JITState, registers) + 4], Compile_FetchParameter());

    // Track get register for zero registers and mark it as no-op
    optimizer.zero_reg_skip = true;

    // AddImmediate tends to be used as a NOP instruction, if we detect this we can
    // completely skip the entire code path and no emit anything
    optimizer.skip_dummy_addimmediate = true;

    // SMO tends to emit a lot of unnecessary method moves, we can mitigate this by only emitting
    // one if our register isn't "dirty"
    optimizer.optimize_for_method_move = true;

    // Enable run-time assertions in JITted code
    optimizer.enable_asserts = false;

    // Check to see if we can skip emitting certain instructions
    Optimizer_ScanFlags();

    const u32 op_count = static_cast<u32>(code.size());
    for (u32 i = 0; i < op_count; i++) {
        if (i < op_count - 1) {
            pc = i + 1;
            next_opcode = GetOpCode();
        } else {
            next_opcode = {};
        }
        pc = i;
        Compile_NextInstruction();
    }

    L(end_of_code);

    Common::X64::ABI_PopRegistersAndAdjustStack(*this, Common::X64::ABI_ALL_CALLEE_SAVED, 8);
    ret();
    ready();
    program = getCode<ProgramType>();
}

bool MacroJITx64Impl::Compile_NextInstruction() {
    const auto opcode = GetOpCode();
    if (labels[pc].getAddress()) {
        return false;
    }

    L(labels[pc]);

    switch (opcode.operation) {
    case Macro::Operation::ALU:
        Compile_ALU(opcode);
        break;
    case Macro::Operation::AddImmediate:
        Compile_AddImmediate(opcode);
        break;
    case Macro::Operation::ExtractInsert:
        Compile_ExtractInsert(opcode);
        break;
    case Macro::Operation::ExtractShiftLeftImmediate:
        Compile_ExtractShiftLeftImmediate(opcode);
        break;
    case Macro::Operation::ExtractShiftLeftRegister:
        Compile_ExtractShiftLeftRegister(opcode);
        break;
    case Macro::Operation::Read:
        Compile_Read(opcode);
        break;
    case Macro::Operation::Branch:
        Compile_Branch(opcode);
        break;
    default:
        UNIMPLEMENTED_MSG("Unimplemented opcode {}", opcode.operation.Value());
        break;
    }

    if (optimizer.has_delayed_pc) {
        if (opcode.is_exit) {
            mov(rax, end_of_code);
            test(BRANCH_HOLDER, BRANCH_HOLDER);
            cmove(BRANCH_HOLDER, rax);
            // Jump to next instruction to skip delay slot check
            je(labels[pc + 1], T_NEAR);
        } else {
            // TODO(ogniK): Optimize delay slot branching
            Xbyak::Label no_delay_slot{};
            test(BRANCH_HOLDER, BRANCH_HOLDER);
            je(no_delay_slot, T_NEAR);
            mov(rax, BRANCH_HOLDER);
            xor_(BRANCH_HOLDER, BRANCH_HOLDER);
            jmp(rax);
            L(no_delay_slot);
        }
        L(delay_skip[pc]);
        if (opcode.is_exit) {
            return false;
        }
    } else {
        test(BRANCH_HOLDER, BRANCH_HOLDER);
        jne(end_of_code, T_NEAR);
        if (opcode.is_exit) {
            inc(BRANCH_HOLDER);
            return false;
        }
    }
    return true;
}

static void WarnInvalidParameter(uintptr_t parameter, uintptr_t max_parameter) {
    LOG_CRITICAL(HW_GPU,
                 "Macro JIT: invalid parameter access 0x{:x} (0x{:x} is the last parameter)",
                 parameter, max_parameter - sizeof(u32));
}

Xbyak::Reg32 MacroJITx64Impl::Compile_FetchParameter() {
    Xbyak::Label parameter_ok{};
    cmp(PARAMETERS, MAX_PARAMETER);
    jb(parameter_ok, T_NEAR);
    Common::X64::ABI_PushRegistersAndAdjustStack(*this, PersistentCallerSavedRegs(), 0);
    mov(Common::X64::ABI_PARAM1, PARAMETERS);
    mov(Common::X64::ABI_PARAM2, MAX_PARAMETER);
    Common::X64::CallFarFunction(*this, &WarnInvalidParameter);
    Common::X64::ABI_PopRegistersAndAdjustStack(*this, PersistentCallerSavedRegs(), 0);
    L(parameter_ok);
    mov(eax, dword[PARAMETERS]);
    add(PARAMETERS, sizeof(u32));
    return eax;
}

Xbyak::Reg32 MacroJITx64Impl::Compile_GetRegister(u32 index, Xbyak::Reg32 dst) {
    if (index == 0) {
        // Register 0 is always zero
        xor_(dst, dst);
    } else {
        mov(dst, dword[STATE + offsetof(JITState, registers) + index * sizeof(u32)]);
    }

    return dst;
}

void MacroJITx64Impl::Compile_ProcessResult(Macro::ResultOperation operation, u32 reg) {
    const auto SetRegister = [this](u32 reg_index, const Xbyak::Reg32& result) {
        // Register 0 is supposed to always return 0. NOP is implemented as a store to the zero
        // register.
        if (reg_index == 0) {
            return;
        }
        mov(dword[STATE + offsetof(JITState, registers) + reg_index * sizeof(u32)], result);
    };
    const auto SetMethodAddress = [this](const Xbyak::Reg32& reg32) { mov(METHOD_ADDRESS, reg32); };

    switch (operation) {
    case Macro::ResultOperation::IgnoreAndFetch:
        SetRegister(reg, Compile_FetchParameter());
        break;
    case Macro::ResultOperation::Move:
        SetRegister(reg, RESULT);
        break;
    case Macro::ResultOperation::MoveAndSetMethod:
        SetRegister(reg, RESULT);
        SetMethodAddress(RESULT);
        break;
    case Macro::ResultOperation::FetchAndSend:
        // Fetch parameter and send result.
        SetRegister(reg, Compile_FetchParameter());
        Compile_Send(RESULT);
        break;
    case Macro::ResultOperation::MoveAndSend:
        // Move and send result.
        SetRegister(reg, RESULT);
        Compile_Send(RESULT);
        break;
    case Macro::ResultOperation::FetchAndSetMethod:
        // Fetch parameter and use result as Method Address.
        SetRegister(reg, Compile_FetchParameter());
        SetMethodAddress(RESULT);
        break;
    case Macro::ResultOperation::MoveAndSetMethodFetchAndSend:
        // Move result and use as Method Address, then fetch and send parameter.
        SetRegister(reg, RESULT);
        SetMethodAddress(RESULT);
        Compile_Send(Compile_FetchParameter());
        break;
    case Macro::ResultOperation::MoveAndSetMethodSend:
        // Move result and use as Method Address, then send bits 12:17 of result.
        SetRegister(reg, RESULT);
        SetMethodAddress(RESULT);
        shr(RESULT, 12);
        and_(RESULT, 0b111111);
        Compile_Send(RESULT);
        break;
    default:
        UNIMPLEMENTED_MSG("Unimplemented macro operation {}", operation);
        break;
    }
}

Macro::Opcode MacroJITx64Impl::GetOpCode() const {
    ASSERT(pc < code.size());
    return {code[pc]};
}
} // Anonymous namespace

MacroJITx64::MacroJITx64(Engines::Maxwell3D& maxwell3d_)
    : MacroEngine{maxwell3d_}, maxwell3d{maxwell3d_} {}

std::unique_ptr<CachedMacro> MacroJITx64::Compile(const std::vector<u32>& code) {
    return std::make_unique<MacroJITx64Impl>(maxwell3d, code);
}
} // namespace Tegra
