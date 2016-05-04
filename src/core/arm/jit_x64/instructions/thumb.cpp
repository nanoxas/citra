// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/assert.h"
#include "common/bit_util.h"

#include "core/arm/jit_x64/jit_x64.h"

namespace JitX64 {

using namespace Gen;

void JitX64::thumb_B(Cond cond, ArmImm8 imm8) {
    cond_manager.CompileCond(cond);

    ASSERT_MSG(current.TFlag, "thumb_B may only be called in thumb mode");

    const u32 new_pc = PC() + BitUtil::SignExtend<9>(imm8 << 1);

    reg_alloc.FlushEverything();
    current.arm_pc += GetInstSize();
    CompileUpdateCycles(false);
    CompileJumpToBB(new_pc);

    if (cond == Cond::AL) {
        stop_compilation = true;
    }
}

void JitX64::thumb_B(ArmImm11 imm11) {
    cond_manager.Always();

    ASSERT_MSG(current.TFlag, "thumb_B may only be called in thumb mode");

    const u32 new_pc = PC() + BitUtil::SignExtend<12>(imm11 << 1);

    reg_alloc.FlushEverything();
    current.arm_pc += GetInstSize();
    CompileUpdateCycles(false);
    CompileJumpToBB(new_pc);

    stop_compilation = true;
}

void JitX64::thumb_BLX_prefix(ArmImm11 imm11) {
    cond_manager.Always();

    ASSERT_MSG(!thumb_BLX_prefix_executed, "two thumb BLX prefixes cannot occur in a row, pc = %u", current.arm_pc);
    ASSERT_MSG(!thumb_BLX_suffix_executed, "thumb_BLX_prefix invalid state, pc = %u", current.arm_pc);
    ASSERT_MSG(current.TFlag, "thumb_BLX_prefix should only be called in thumb mode, pc = %u", current.arm_pc);

    thumb_BLX_prefix_imm11 = imm11;
    thumb_BLX_prefix_executed = true;

    current.arm_pc += GetInstSize();

    // Compile the suffix, and make sure that it's compiled.
    instructions_compiled++; // Has to be done to pass unit tests (same method of counting as interpreter).
    CompileSingleThumbInstruction();
    ASSERT_MSG(thumb_BLX_suffix_executed, "thumb BLX suffix did not come after thumb BLX prefix, pc = %u", current.arm_pc);

    thumb_BLX_prefix_executed = false;
    thumb_BLX_suffix_executed = false;
}

void JitX64::thumb_BLX_suffix(bool X, ArmImm11 imm11) {
    cond_manager.Always();

    // Must only be ever called right after the prefix.
    ASSERT_MSG(thumb_BLX_prefix_executed, "thumb BLX suffix may only come after a thumb BLX prefix, pc = %u", current.arm_pc);
    ASSERT_MSG(!thumb_BLX_suffix_executed, "thumb_BLX_suffix invalid state, pc = %u", current.arm_pc);
    ASSERT_MSG(current.TFlag, "thumb_BLX_suffix should only be called in thumb mode, pc = %u", current.arm_pc);

    u32 new_pc = (current.arm_pc + 2) +
        BitUtil::SignExtend<23>(thumb_BLX_prefix_imm11 << 12) +
        (imm11 << 1);
    u32 new_lr = (current.arm_pc + 2) | 1;

    OpArg LR = reg_alloc.LockArmForWrite(ArmReg::LR);
    code->MOV(32, LR, Imm32(new_lr));
    reg_alloc.UnlockArm(ArmReg::LR);

    if (X) {
        current.TFlag = false;
        code->MOV(32, MJitStateTFlag(), Imm32(0));
        new_pc &= 0xFFFFFFFC;
    }

    reg_alloc.FlushEverything();
    current.arm_pc += GetInstSize();
    CompileUpdateCycles();
    CompileJumpToBB(new_pc);

    stop_compilation = true;

    thumb_BLX_suffix_executed = true;
}

} // namespace JitX64
