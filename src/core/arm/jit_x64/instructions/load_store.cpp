// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/arm/jit_x64/jit_x64.h"

namespace JitX64 {

// Load/Store instructions
void JitX64::LDR_imm() { CompileInterpretInstruction(); }
void JitX64::LDR_reg() { CompileInterpretInstruction(); }
void JitX64::LDRB_imm() { CompileInterpretInstruction(); }
void JitX64::LDRB_reg() { CompileInterpretInstruction(); }
void JitX64::LDRBT() { CompileInterpretInstruction(); }
void JitX64::LDRD_imm() { CompileInterpretInstruction(); }
void JitX64::LDRD_reg() { CompileInterpretInstruction(); }
void JitX64::LDRH_imm() { CompileInterpretInstruction(); }
void JitX64::LDRH_reg() { CompileInterpretInstruction(); }
void JitX64::LDRHT() { CompileInterpretInstruction(); }
void JitX64::LDRSB_imm() { CompileInterpretInstruction(); }
void JitX64::LDRSB_reg() { CompileInterpretInstruction(); }
void JitX64::LDRSBT() { CompileInterpretInstruction(); }
void JitX64::LDRSH_imm() { CompileInterpretInstruction(); }
void JitX64::LDRSH_reg() { CompileInterpretInstruction(); }
void JitX64::LDRSHT() { CompileInterpretInstruction(); }
void JitX64::LDRT() { CompileInterpretInstruction(); }
void JitX64::STR_imm() { CompileInterpretInstruction(); }
void JitX64::STR_reg() { CompileInterpretInstruction(); }
void JitX64::STRB_imm() { CompileInterpretInstruction(); }
void JitX64::STRB_reg() { CompileInterpretInstruction(); }
void JitX64::STRBT() { CompileInterpretInstruction(); }
void JitX64::STRD_imm() { CompileInterpretInstruction(); }
void JitX64::STRD_reg() { CompileInterpretInstruction(); }
void JitX64::STRH_imm() { CompileInterpretInstruction(); }
void JitX64::STRH_reg() { CompileInterpretInstruction(); }
void JitX64::STRHT() { CompileInterpretInstruction(); }
void JitX64::STRT() { CompileInterpretInstruction(); }

// Load/Store multiple instructions
void JitX64::LDM() { CompileInterpretInstruction(); }
void JitX64::STM() { CompileInterpretInstruction(); }

}
