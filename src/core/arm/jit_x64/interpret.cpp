// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/x64/abi.h"

#include "core/arm/jit_x64/jit_x64.h"

extern unsigned InterpreterMainLoop(ARMul_State* cpu);

namespace JitX64 {

using namespace Gen;

static JitState* CallInterpreter(JitState* jit_state, u64 pc, u64 TFlag, u64 EFlag) {
    ARMul_State* cpu = &jit_state->cpu_state;

    cpu->Reg[15] = pc;

    cpu->Cpsr = (cpu->Cpsr & 0x0fffffdf) |
                (cpu->NFlag << 31) |
                (cpu->ZFlag << 30) |
                (cpu->CFlag << 29) |
                (cpu->VFlag << 28) |
                (cpu->TFlag << 5);

    if (jit_state->cycles_remaining >= 0) {
#if 0
        cpu->NumInstrsToExecute = jit_state->cycles_remaining + 1;
        if (cpu->NumInstrsToExecute > 100) cpu->NumInstrsToExecute = 100;
        jit_state->cycles_remaining -= InterpreterMainLoop(cpu) - 1;
#else
        cpu->NumInstrsToExecute = 1;
        jit_state->cycles_remaining -= InterpreterMainLoop(cpu) - 1;
#endif
    }

    return jit_state;
}

void JitX64::CompileInterpretInstruction() {
    cond_manager.Always();
    reg_alloc.FlushEverything();

    CompileUpdateCycles();

    ASSERT(reg_alloc.JitStateReg() != RSP);
    code->MOV(64, R(RSP), MJitStateHostReturnRSP());
    code->MOV(64, R(Gen::ABI_PARAM1), R(reg_alloc.JitStateReg()));
    code->MOV(64, R(Gen::ABI_PARAM2), Imm64(current.arm_pc));
    code->MOV(64, R(Gen::ABI_PARAM3), Imm64(current.TFlag));
    code->MOV(64, R(Gen::ABI_PARAM4), Imm64(current.EFlag));

    const void *const fn = reinterpret_cast<const void* const>(&CallInterpreter);

    const u64 distance = reinterpret_cast<u64>(fn) - (reinterpret_cast<u64>(code->GetCodePtr()) + 5);
    if (distance >= 0x0000000080000000ULL && distance < 0xFFFFFFFF80000000ULL) {
        // Far call
        code->MOV(64, R(RAX), ImmPtr(fn));
        code->CALLptr(R(RAX));
    } else {
        code->CALL(fn);
    }

    code->MOV(64, R(reg_alloc.JitStateReg()), R(Gen::ABI_RETURN));

    // Return to dispatch
    code->JMPptr(MJitStateHostReturnRIP());

    current.arm_pc += GetInstSize();
    stop_compilation = true;
}

}
