// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/arm/jit_x64/jit_x64.h"

namespace JitX64 {

// Multiply (Normal) instructions
void JitX64::MLA(Cond cond, bool S, ArmReg Rd, ArmReg Ra, ArmReg Rm, ArmReg Rn) { CompileInterpretInstruction(); }
void JitX64::MUL(Cond cond, bool S, ArmReg Rd, ArmReg Rm, ArmReg Rn) { CompileInterpretInstruction(); }

// Multiply (Long) instructions
void JitX64::SMLAL(Cond cond, bool S, ArmReg RdHi, ArmReg RdLo, ArmReg Rm, ArmReg Rn) { CompileInterpretInstruction(); }
void JitX64::SMULL(Cond cond, bool S, ArmReg RdHi, ArmReg RdLo, ArmReg Rm, ArmReg Rn) { CompileInterpretInstruction(); }
void JitX64::UMAAL(Cond cond, ArmReg RdHi, ArmReg RdLo, ArmReg Rm, ArmReg Rn) { CompileInterpretInstruction(); }
void JitX64::UMLAL(Cond cond, bool S, ArmReg RdHi, ArmReg RdLo, ArmReg Rm, ArmReg Rn) { CompileInterpretInstruction(); }
void JitX64::UMULL(Cond cond, bool S, ArmReg RdHi, ArmReg RdLo, ArmReg Rm, ArmReg Rn) { CompileInterpretInstruction(); }

// Multiply (Halfword) instructions
void JitX64::SMLALxy(Cond cond, ArmReg RdHi, ArmReg RdLo, ArmReg Rm, bool M, bool N, ArmReg Rn) { CompileInterpretInstruction(); }
void JitX64::SMLAxy(Cond cond, ArmReg Rd, ArmReg Ra, ArmReg Rm, bool M, bool N, ArmReg Rn) { CompileInterpretInstruction(); }
void JitX64::SMULxy(Cond cond, ArmReg Rd, ArmReg Rm, bool M, bool N, ArmReg Rn) { CompileInterpretInstruction(); }

// Multiply (word by halfword) instructions
void JitX64::SMLAWy(Cond cond, ArmReg Rd, ArmReg Ra, ArmReg Rm, bool M, ArmReg Rn) { CompileInterpretInstruction(); }
void JitX64::SMULWy(Cond cond, ArmReg Rd, ArmReg Rm, bool M, ArmReg Rn) { CompileInterpretInstruction(); }

// Multiply (Most significant word) instructions
void JitX64::SMMLA(Cond cond, ArmReg Rd, ArmReg Ra, ArmReg Rm, bool R, ArmReg Rn) { CompileInterpretInstruction(); }
void JitX64::SMMLS(Cond cond, ArmReg Rd, ArmReg Ra, ArmReg Rm, bool R, ArmReg Rn) { CompileInterpretInstruction(); }
void JitX64::SMMUL(Cond cond, ArmReg Rd, ArmReg Rm, bool R, ArmReg Rn) { CompileInterpretInstruction(); }

// Multiply (Dual) instructions
void JitX64::SMLAD(Cond cond, ArmReg Rd, ArmReg Ra, ArmReg Rm, bool M, ArmReg Rn) { CompileInterpretInstruction(); }
void JitX64::SMLALD(Cond cond, ArmReg RdHi, ArmReg RdLo, ArmReg Rm, bool M, ArmReg Rn) { CompileInterpretInstruction(); }
void JitX64::SMLSD(Cond cond, ArmReg Rd, ArmReg Ra, ArmReg Rm, bool M, ArmReg Rn) { CompileInterpretInstruction(); }
void JitX64::SMLSLD(Cond cond, ArmReg RdHi, ArmReg RdLo, ArmReg Rm, bool M, ArmReg Rn) { CompileInterpretInstruction(); }
void JitX64::SMUAD(Cond cond, ArmReg Rd, ArmReg Rm, bool M, ArmReg Rn) { CompileInterpretInstruction(); }
void JitX64::SMUSD(Cond cond, ArmReg Rd, ArmReg Rm, bool M, ArmReg Rn) { CompileInterpretInstruction(); }

} // namespace JitX64
