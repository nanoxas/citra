// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/arm/jit_x64/jit_x64.h"

namespace JitX64 {

void JitX64::SSAT(Cond cond, ArmImm5 sat_imm, ArmReg Rd, ArmImm5 imm5, bool sh, ArmReg Rn) { CompileInterpretInstruction(); }
void JitX64::SSAT16(Cond cond, ArmImm4 sat_imm, ArmReg Rd, ArmReg Rn) { CompileInterpretInstruction(); }
void JitX64::USAT(Cond cond, ArmImm5 sat_imm, ArmReg Rd, ArmImm5 imm5, bool sh, ArmReg Rn) { CompileInterpretInstruction(); }
void JitX64::USAT16(Cond cond, ArmImm4 sat_imm, ArmReg Rd, ArmReg Rn) { CompileInterpretInstruction(); }

} // namespace JitX64
