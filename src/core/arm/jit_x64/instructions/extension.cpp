// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/arm/jit_x64/jit_x64.h"

namespace JitX64 {

void JitX64::SXTAB(Cond cond, ArmReg Rn, ArmReg Rd, SignExtendRotation rotate, ArmReg Rm) { CompileInterpretInstruction(); }
void JitX64::SXTAB16(Cond cond, ArmReg Rn, ArmReg Rd, SignExtendRotation rotate, ArmReg Rm) { CompileInterpretInstruction(); }
void JitX64::SXTAH(Cond cond, ArmReg Rn, ArmReg Rd, SignExtendRotation rotate, ArmReg Rm) { CompileInterpretInstruction(); }
void JitX64::SXTB(Cond cond, ArmReg Rd, SignExtendRotation rotate, ArmReg Rm) { CompileInterpretInstruction(); }
void JitX64::SXTB16(Cond cond, ArmReg Rd, SignExtendRotation rotate, ArmReg Rm) { CompileInterpretInstruction(); }
void JitX64::SXTH(Cond cond, ArmReg Rd, SignExtendRotation rotate, ArmReg Rm) { CompileInterpretInstruction(); }
void JitX64::UXTAB(Cond cond, ArmReg Rn, ArmReg Rd, SignExtendRotation rotate, ArmReg Rm) { CompileInterpretInstruction(); }
void JitX64::UXTAB16(Cond cond, ArmReg Rn, ArmReg Rd, SignExtendRotation rotate, ArmReg Rm) { CompileInterpretInstruction(); }
void JitX64::UXTAH(Cond cond, ArmReg Rn, ArmReg Rd, SignExtendRotation rotate, ArmReg Rm) { CompileInterpretInstruction(); }
void JitX64::UXTB(Cond cond, ArmReg Rd, SignExtendRotation rotate, ArmReg Rm) { CompileInterpretInstruction(); }
void JitX64::UXTB16(Cond cond, ArmReg Rd, SignExtendRotation rotate, ArmReg Rm) { CompileInterpretInstruction(); }
void JitX64::UXTH(Cond cond, ArmReg Rd, SignExtendRotation rotate, ArmReg Rm) { CompileInterpretInstruction(); }

} // namespace JitX64
