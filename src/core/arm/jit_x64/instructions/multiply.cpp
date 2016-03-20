// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/arm/jit_x64/jit_x64.h"

namespace JitX64 {

// Multiply (Normal) instructions
void JitX64::MLA() { CompileInterpretInstruction(); }
void JitX64::MLS() { CompileInterpretInstruction(); }
void JitX64::MUL() { CompileInterpretInstruction(); }

// Multiply (Long) instructions
void JitX64::SMLAL() { CompileInterpretInstruction(); }
void JitX64::SMULL() { CompileInterpretInstruction(); }
void JitX64::UMAAL() { CompileInterpretInstruction(); }
void JitX64::UMLAL() { CompileInterpretInstruction(); }
void JitX64::UMULL() { CompileInterpretInstruction(); }

// Multiply (Halfword) instructions
void JitX64::SMLALxy() { CompileInterpretInstruction(); }
void JitX64::SMLAxy() { CompileInterpretInstruction(); }
void JitX64::SMULxy() { CompileInterpretInstruction(); }

// Multiply (word by halfword) instructions
void JitX64::SMLAWy() { CompileInterpretInstruction(); }
void JitX64::SMULWy() { CompileInterpretInstruction(); }

// Multiply (Most significant word) instructions
void JitX64::SMMLA() { CompileInterpretInstruction(); }
void JitX64::SMMLS() { CompileInterpretInstruction(); }
void JitX64::SMMUL() { CompileInterpretInstruction(); }

// Multiply (Dual) instructions
void JitX64::SMLAD() { CompileInterpretInstruction(); }
void JitX64::SMLALD() { CompileInterpretInstruction(); }
void JitX64::SMLSD() { CompileInterpretInstruction(); }
void JitX64::SMLSLD() { CompileInterpretInstruction(); }
void JitX64::SMUAD() { CompileInterpretInstruction(); }
void JitX64::SMUSD() { CompileInterpretInstruction(); }

}
