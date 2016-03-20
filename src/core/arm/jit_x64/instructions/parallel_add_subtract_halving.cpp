// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/arm/jit_x64/jit_x64.h"

namespace JitX64 {

void JitX64::SHADD8() { CompileInterpretInstruction(); }
void JitX64::SHADD16() { CompileInterpretInstruction(); }
void JitX64::SHASX() { CompileInterpretInstruction(); }
void JitX64::SHSAX() { CompileInterpretInstruction(); }
void JitX64::SHSUB8() { CompileInterpretInstruction(); }
void JitX64::SHSUB16() { CompileInterpretInstruction(); }
void JitX64::UHADD8() { CompileInterpretInstruction(); }
void JitX64::UHADD16() { CompileInterpretInstruction(); }
void JitX64::UHASX() { CompileInterpretInstruction(); }
void JitX64::UHSAX() { CompileInterpretInstruction(); }
void JitX64::UHSUB8() { CompileInterpretInstruction(); }
void JitX64::UHSUB16() { CompileInterpretInstruction(); }

}
