// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/arm/jit_x64/jit_x64.h"
#include "core/memory.h"

namespace JitX64 {

using namespace Gen;

JitX64::JitX64(XEmitter* code_) : code(code_) {}

CodePtr JitX64::GetBB(u32 pc, bool TFlag, bool EFlag) {
    const LocationDescriptor desc = { pc, TFlag, EFlag };

    if (basic_blocks.find(desc) == basic_blocks.end()) {
        return Compile(pc, TFlag, EFlag);
    }

    return basic_blocks[desc];
}

CodePtr JitX64::Compile(u32 pc, bool TFlag, bool EFlag) {
    const CodePtr bb = code->GetWritableCodePtr();
    const LocationDescriptor desc = { pc, TFlag, EFlag };
    ASSERT(basic_blocks.find(desc) == basic_blocks.end());
    basic_blocks[desc] = bb;
    Patch(desc, bb);

    reg_alloc.Init(code);
    cond_manager.Init(this);
    current = desc;
    instructions_compiled = 0;
    stop_compilation = false;

    do {
        instructions_compiled++;

        if (current.TFlag) {
            CompileSingleThumbInstruction();
        } else {
            CompileSingleArmInstruction();
        }
    } while (!stop_compilation && ((current.arm_pc & 0xFFF) != 0));

    if (!stop_compilation) {
        // We're stopping compilation because we've reached a page boundary.
        cond_manager.Always();
        CompileUpdateCycles();
        CompileJumpToBB(current.arm_pc);
    }

    // Insert easily searchable byte sequence for ease of lookup in memory dumps.
    code->NOP();
    code->INT3();
    code->NOP();

    return bb;
}

void JitX64::CompileUpdateCycles() {
    // We're just taking one instruction == one cycle.
    if (instructions_compiled) {
        code->SUB(32, MJitStateCycleCount(), Imm32(instructions_compiled));
    }
    instructions_compiled = 0;
}

void JitX64::CompileJumpToBB(u32 new_pc) {
    ASSERT(instructions_compiled == 0);

    reg_alloc.FlushEverything();
    code->CMP(32, MJitStateCycleCount(), Imm8(0));

    const LocationDescriptor new_desc = { new_pc, current.TFlag, current.EFlag };
    patch_jmp_locations[new_desc].emplace_back(code->GetWritableCodePtr());
    if (basic_blocks.find(new_desc) == basic_blocks.end()) {
        code->NOP(6); // Leave enough space for a jg instruction.
    } else {
        code->J_CC(CC_G, basic_blocks[new_desc], true);
    }

    code->MOV(32, MJitStateArmPC(), Imm32(new_pc));
    code->JMPptr(MJitStateHostReturnRIP());
}

void JitX64::Patch(LocationDescriptor desc, CodePtr bb) {
    const CodePtr save_code_ptr = code->GetWritableCodePtr();

    for (CodePtr location : patch_jmp_locations[desc]) {
        code->SetCodePtr(location);
        code->J_CC(CC_G, bb, true);
        ASSERT(code->GetCodePtr() - location == 6);
    }

    code->SetCodePtr(save_code_ptr);
}

void JitX64::CompileSingleArmInstruction() {
    u32 inst = Memory::Read32(current.arm_pc & 0xFFFFFFFC);

    ArmDecoder::DecodeArm(inst).Visit(this, inst);
}

void JitX64::CompileSingleThumbInstruction() {
    u32 inst_u32 = Memory::Read32(current.arm_pc & 0xFFFFFFFC);
    if ((current.arm_pc & 0x3) != 0) {
        inst_u32 >>= 16;
    }
    inst_u32 &= 0xFFFFF;
    u16 inst = inst_u32;

    ArmDecoder::DecodeThumb(inst).Visit(this, inst);
}

// Convenience functions:
// We static_assert types because anything that calls these functions makes those assumptions.
// If the types of the variables are changed please update all code that calls these functions.

Gen::OpArg JitX64::MJitStateCycleCount() {
    static_assert(std::is_same<decltype(JitState::cycles_remaining), s32>::value, "JitState::cycles_remaining must be s32");

    return Gen::MDisp(reg_alloc.JitStateReg(), offsetof(JitState, cycles_remaining));
}

Gen::OpArg JitX64::MJitStateArmPC() {
    static_assert(std::is_same<decltype(JitState::cpu_state), ARMul_State>::value, "JitState::cpu_state must be ARMul_State");
    static_assert(std::is_same<decltype(ARMul_State::Reg), std::array<u32, 16>>::value, "ARMul_State::Reg must be std::array<u32, 16>");

    return Gen::MDisp(reg_alloc.JitStateReg(), offsetof(JitState, cpu_state) + offsetof(ARMul_State, Reg) + 15 * sizeof(u32));
}

Gen::OpArg JitX64::MJitStateHostReturnRIP() {
    static_assert(std::is_same<decltype(JitState::return_RIP), u64>::value, "JitState::return_RIP must be u64");

    return Gen::MDisp(reg_alloc.JitStateReg(), offsetof(JitState, return_RIP));
}

Gen::OpArg JitX64::MJitStateHostReturnRSP() {
    static_assert(std::is_same<decltype(JitState::save_host_RSP), u64>::value, "JitState::save_host_RSP must be u64");

    return Gen::MDisp(reg_alloc.JitStateReg(), offsetof(JitState, save_host_RSP));
}

Gen::OpArg JitX64::MJitStateZFlag() {
    static_assert(std::is_same<decltype(JitState::cpu_state), ARMul_State>::value, "JitState::cpu_state must be ARMul_State");
    static_assert(std::is_same<decltype(ARMul_State::ZFlag), u32>::value, "ZFlag must be u32");

    return Gen::MDisp(reg_alloc.JitStateReg(), offsetof(JitState, cpu_state) + offsetof(ARMul_State, ZFlag));
}

Gen::OpArg JitX64::MJitStateCFlag() {
    static_assert(std::is_same<decltype(JitState::cpu_state), ARMul_State>::value, "JitState::cpu_state must be ARMul_State");
    static_assert(std::is_same<decltype(ARMul_State::CFlag), u32>::value, "CFlag must be u32");

    return Gen::MDisp(reg_alloc.JitStateReg(), offsetof(JitState, cpu_state) + offsetof(ARMul_State, CFlag));
}

Gen::OpArg JitX64::MJitStateNFlag() {
    static_assert(std::is_same<decltype(JitState::cpu_state), ARMul_State>::value, "JitState::cpu_state must be ARMul_State");
    static_assert(std::is_same<decltype(ARMul_State::NFlag), u32>::value, "NFlag must be u32");

    return Gen::MDisp(reg_alloc.JitStateReg(), offsetof(JitState, cpu_state) + offsetof(ARMul_State, NFlag));
}

Gen::OpArg JitX64::MJitStateVFlag() {
    static_assert(std::is_same<decltype(JitState::cpu_state), ARMul_State>::value, "JitState::cpu_state must be ARMul_State");
    static_assert(std::is_same<decltype(ARMul_State::VFlag), u32>::value, "VFlag must be u32");

    return Gen::MDisp(reg_alloc.JitStateReg(), offsetof(JitState, cpu_state) + offsetof(ARMul_State, VFlag));
}

Gen::OpArg JitX64::MJitStateExclusiveTag() {
    static_assert(std::is_same<decltype(JitState::cpu_state), ARMul_State>::value, "JitState::cpu_state must be ARMul_State");
    static_assert(std::is_same<decltype(ARMul_State::exclusive_tag), u32>::value, "exclusive_tag must be u32");

    return Gen::MDisp(reg_alloc.JitStateReg(), offsetof(JitState, cpu_state) + offsetof(ARMul_State, exclusive_tag));
}

Gen::OpArg JitX64::MJitStateExclusiveState() {
    static_assert(std::is_same<decltype(JitState::cpu_state), ARMul_State>::value, "JitState::cpu_state must be ARMul_State");
    static_assert(std::is_same<decltype(ARMul_State::exclusive_state), bool>::value, "exclusive_state must be bool");

    return Gen::MDisp(reg_alloc.JitStateReg(), offsetof(JitState, cpu_state) + offsetof(ARMul_State, exclusive_state));
}

}
