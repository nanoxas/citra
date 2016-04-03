// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/arm/jit_x64/jit_x64.h"

namespace JitX64 {

void JitX64::SHADD8(Cond cond, ArmReg Rn, ArmReg Rd, ArmReg Rm) { CompileInterpretInstruction(); }
void JitX64::SHADD16(Cond cond, ArmReg Rn, ArmReg Rd, ArmReg Rm) { CompileInterpretInstruction(); }
void JitX64::SHASX(Cond cond, ArmReg Rn, ArmReg Rd, ArmReg Rm) { CompileInterpretInstruction(); }
void JitX64::SHSAX(Cond cond, ArmReg Rn, ArmReg Rd, ArmReg Rm) { CompileInterpretInstruction(); }
void JitX64::SHSUB8(Cond cond, ArmReg Rn, ArmReg Rd, ArmReg Rm) { CompileInterpretInstruction(); }
void JitX64::SHSUB16(Cond cond, ArmReg Rn, ArmReg Rd, ArmReg Rm) { CompileInterpretInstruction(); }
void JitX64::UHADD8(Cond cond, ArmReg Rn, ArmReg Rd, ArmReg Rm) { CompileInterpretInstruction(); }
void JitX64::UHADD16(Cond cond, ArmReg Rn, ArmReg Rd, ArmReg Rm) { CompileInterpretInstruction(); }
void JitX64::UHASX(Cond cond, ArmReg Rn, ArmReg Rd, ArmReg Rm) { CompileInterpretInstruction(); }
void JitX64::UHSAX(Cond cond, ArmReg Rn, ArmReg Rd, ArmReg Rm) { CompileInterpretInstruction(); }
void JitX64::UHSUB8(Cond cond, ArmReg Rn, ArmReg Rd, ArmReg Rm) { CompileInterpretInstruction(); }
void JitX64::UHSUB16(Cond cond, ArmReg Rn, ArmReg Rd, ArmReg Rm) { CompileInterpretInstruction(); }

} // namespace JitX64
