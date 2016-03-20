// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/arm/jit_x64/jit_x64.h"

namespace JitX64 {

void JitX64::QADD8() { CompileInterpretInstruction(); }
void JitX64::QADD16() { CompileInterpretInstruction(); }
void JitX64::QASX() { CompileInterpretInstruction(); }
void JitX64::QSAX() { CompileInterpretInstruction(); }
void JitX64::QSUB8() { CompileInterpretInstruction(); }
void JitX64::QSUB16() { CompileInterpretInstruction(); }
void JitX64::UQADD8() { CompileInterpretInstruction(); }
void JitX64::UQADD16() { CompileInterpretInstruction(); }
void JitX64::UQASX() { CompileInterpretInstruction(); }
void JitX64::UQSAX() { CompileInterpretInstruction(); }
void JitX64::UQSUB8() { CompileInterpretInstruction(); }
void JitX64::UQSUB16() { CompileInterpretInstruction(); }


}
