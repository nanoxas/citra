// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/arm/jit_x64/jit_x64.h"

namespace JitX64 {

void JitX64::QADD8(Cond cond, ArmReg Rn, ArmReg Rd, ArmReg Rm) { CompileInterpretInstruction(); }
void JitX64::QADD16(Cond cond, ArmReg Rn, ArmReg Rd, ArmReg Rm) { CompileInterpretInstruction(); }
void JitX64::QASX(Cond cond, ArmReg Rn, ArmReg Rd, ArmReg Rm) { CompileInterpretInstruction(); }
void JitX64::QSAX(Cond cond, ArmReg Rn, ArmReg Rd, ArmReg Rm) { CompileInterpretInstruction(); }
void JitX64::QSUB8(Cond cond, ArmReg Rn, ArmReg Rd, ArmReg Rm) { CompileInterpretInstruction(); }
void JitX64::QSUB16(Cond cond, ArmReg Rn, ArmReg Rd, ArmReg Rm) { CompileInterpretInstruction(); }
void JitX64::UQADD8(Cond cond, ArmReg Rn, ArmReg Rd, ArmReg Rm) { CompileInterpretInstruction(); }
void JitX64::UQADD16(Cond cond, ArmReg Rn, ArmReg Rd, ArmReg Rm) { CompileInterpretInstruction(); }
void JitX64::UQASX(Cond cond, ArmReg Rn, ArmReg Rd, ArmReg Rm) { CompileInterpretInstruction(); }
void JitX64::UQSAX(Cond cond, ArmReg Rn, ArmReg Rd, ArmReg Rm) { CompileInterpretInstruction(); }
void JitX64::UQSUB8(Cond cond, ArmReg Rn, ArmReg Rd, ArmReg Rm) { CompileInterpretInstruction(); }
void JitX64::UQSUB16(Cond cond, ArmReg Rn, ArmReg Rd, ArmReg Rm) { CompileInterpretInstruction(); }

} // namespace JitX64
