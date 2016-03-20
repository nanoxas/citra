// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/arm/jit_x64/jit_x64.h"

namespace JitX64 {

void JitX64::CLREX() { CompileInterpretInstruction(); }
void JitX64::LDREX() { CompileInterpretInstruction(); }
void JitX64::LDREXB() { CompileInterpretInstruction(); }
void JitX64::LDREXD() { CompileInterpretInstruction(); }
void JitX64::LDREXH() { CompileInterpretInstruction(); }
void JitX64::STREX() { CompileInterpretInstruction(); }
void JitX64::STREXB() { CompileInterpretInstruction(); }
void JitX64::STREXD() { CompileInterpretInstruction(); }
void JitX64::STREXH() { CompileInterpretInstruction(); }
void JitX64::SWP() { CompileInterpretInstruction(); }

}