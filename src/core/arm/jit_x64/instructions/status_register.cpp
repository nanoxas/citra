// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/arm/jit_x64/jit_x64.h"

namespace JitX64 {

using namespace Gen;

void JitX64::CPS() { CompileInterpretInstruction(); }
void JitX64::MRS() { CompileInterpretInstruction(); }
void JitX64::MSR() { CompileInterpretInstruction(); }
void JitX64::RFE() { CompileInterpretInstruction(); }

void JitX64::SETEND(bool E) {
    cond_manager.Always();

    if (E) {
        code->OR(32, MJitStateCpsr(), Imm32(1 << 9));
        current.EFlag = true;
    } else {
        code->AND(32, MJitStateCpsr(), Imm32(~(1 << 9)));
        current.EFlag = false;
    }

    current.arm_pc += GetInstSize();
}

void JitX64::SRS() { CompileInterpretInstruction(); }

} // namespace JitX64
