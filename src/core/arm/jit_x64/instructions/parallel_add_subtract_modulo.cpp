// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/arm/jit_x64/jit_x64.h"

namespace JitX64 {

void JitX64::SADD8() { CompileInterpretInstruction(); }
void JitX64::SADD16() { CompileInterpretInstruction(); }
void JitX64::SASX() { CompileInterpretInstruction(); }
void JitX64::SSAX() { CompileInterpretInstruction(); }
void JitX64::SSUB8() { CompileInterpretInstruction(); }
void JitX64::SSUB16() { CompileInterpretInstruction(); }
void JitX64::UADD8() { CompileInterpretInstruction(); }
void JitX64::UADD16() { CompileInterpretInstruction(); }
void JitX64::UASX() { CompileInterpretInstruction(); }
void JitX64::USAX() { CompileInterpretInstruction(); }
void JitX64::USUB8() { CompileInterpretInstruction(); }
void JitX64::USUB16() { CompileInterpretInstruction(); }

}
