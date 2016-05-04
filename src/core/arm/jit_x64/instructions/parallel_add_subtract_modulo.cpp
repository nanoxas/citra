// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/arm/jit_x64/jit_x64.h"

namespace JitX64 {

void JitX64::SADD8(Cond cond, ArmReg Rn, ArmReg Rd, ArmReg Rm) { CompileInterpretInstruction(); }
void JitX64::SADD16(Cond cond, ArmReg Rn, ArmReg Rd, ArmReg Rm) { CompileInterpretInstruction(); }
void JitX64::SASX(Cond cond, ArmReg Rn, ArmReg Rd, ArmReg Rm) { CompileInterpretInstruction(); }
void JitX64::SSAX(Cond cond, ArmReg Rn, ArmReg Rd, ArmReg Rm) { CompileInterpretInstruction(); }
void JitX64::SSUB8(Cond cond, ArmReg Rn, ArmReg Rd, ArmReg Rm) { CompileInterpretInstruction(); }
void JitX64::SSUB16(Cond cond, ArmReg Rn, ArmReg Rd, ArmReg Rm) { CompileInterpretInstruction(); }
void JitX64::UADD8(Cond cond, ArmReg Rn, ArmReg Rd, ArmReg Rm) { CompileInterpretInstruction(); }
void JitX64::UADD16(Cond cond, ArmReg Rn, ArmReg Rd, ArmReg Rm) { CompileInterpretInstruction(); }
void JitX64::UASX(Cond cond, ArmReg Rn, ArmReg Rd, ArmReg Rm) { CompileInterpretInstruction(); }
void JitX64::USAX(Cond cond, ArmReg Rn, ArmReg Rd, ArmReg Rm) { CompileInterpretInstruction(); }
void JitX64::USUB8(Cond cond, ArmReg Rn, ArmReg Rd, ArmReg Rm) { CompileInterpretInstruction(); }
void JitX64::USUB16(Cond cond, ArmReg Rn, ArmReg Rd, ArmReg Rm) { CompileInterpretInstruction(); }

} // namespace JitX64
