// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/arm/jit_x64/jit_x64.h"

namespace JitX64 {

void JitX64::B(Cond cond, ArmImm24 imm24) { CompileInterpretInstruction(); }
void JitX64::BL(Cond cond, ArmImm24 imm24) { CompileInterpretInstruction(); }
void JitX64::BLX_imm(bool H, ArmImm24 imm24) { CompileInterpretInstruction(); }
void JitX64::BLX_reg(Cond cond, ArmReg Rm) { CompileInterpretInstruction(); }
void JitX64::BX(Cond cond, ArmReg Rm) { CompileInterpretInstruction(); }
void JitX64::BXJ(Cond cond, ArmReg Rm) { CompileInterpretInstruction(); }

}
