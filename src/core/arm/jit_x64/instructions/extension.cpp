// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/arm/jit_x64/jit_x64.h"

namespace JitX64 {

void JitX64::SXTAB() { CompileInterpretInstruction(); }
void JitX64::SXTAB16() { CompileInterpretInstruction(); }
void JitX64::SXTAH() { CompileInterpretInstruction(); }
void JitX64::SXTB() { CompileInterpretInstruction(); }
void JitX64::SXTB16() { CompileInterpretInstruction(); }
void JitX64::SXTH() { CompileInterpretInstruction(); }
void JitX64::UXTAB() { CompileInterpretInstruction(); }
void JitX64::UXTAB16() { CompileInterpretInstruction(); }
void JitX64::UXTAH() { CompileInterpretInstruction(); }
void JitX64::UXTB() { CompileInterpretInstruction(); }
void JitX64::UXTB16() { CompileInterpretInstruction(); }
void JitX64::UXTH() { CompileInterpretInstruction(); }

}
