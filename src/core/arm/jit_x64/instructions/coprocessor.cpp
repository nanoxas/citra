// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/arm/jit_x64/jit_x64.h"

namespace JitX64 {

void JitX64::CDP() { CompileInterpretInstruction(); }
void JitX64::LDC() { CompileInterpretInstruction(); }
void JitX64::MCR() { CompileInterpretInstruction(); }
void JitX64::MCRR() { CompileInterpretInstruction(); }
void JitX64::MRC() { CompileInterpretInstruction(); }
void JitX64::MRRC() { CompileInterpretInstruction(); }
void JitX64::STC() { CompileInterpretInstruction(); }

} // namespace JitX64
