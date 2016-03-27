// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/math_util.h"

#include "core/arm/jit_x64/jit_x64.h"

namespace JitX64 {

void JitX64::thumb_B(Cond cond, ArmImm8 imm8) {
    cond_manager.CompileCond((ConditionCode)cond);

    const u32 new_pc = GetReg15Value() + MathUtil::SignExtend<9, s32>(imm8 << 1);

    reg_alloc.FlushEverything();
    current.arm_pc += GetInstSize();
    CompileUpdateCycles(false);
    CompileJumpToBB(new_pc);

    if (cond == ConditionCode::AL) {
        stop_compilation = true;
    }
}

void JitX64::thumb_B(ArmImm11 imm11) {
    cond_manager.Always();

    const u32 new_pc = GetReg15Value() + MathUtil::SignExtend<12, s32>(imm11 << 1);

    reg_alloc.FlushEverything();
    current.arm_pc += GetInstSize();
    CompileUpdateCycles(false);
    CompileJumpToBB(new_pc);

    stop_compilation = true;
}

void JitX64::thumb_BLX_prefix(ArmImm11 imm11) { CompileInterpretInstruction(); }
void JitX64::thumb_BLX_suffix(bool L, ArmImm11 imm11) { CompileInterpretInstruction(); }

}
