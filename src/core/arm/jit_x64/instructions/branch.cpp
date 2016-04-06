// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/bit_util.h"

#include "core/arm/jit_x64/jit_x64.h"

namespace JitX64 {

using namespace Gen;

void JitX64::B(Cond cond, ArmImm24 imm24) {
    cond_manager.CompileCond(cond);

    const u32 new_pc = GetReg15Value() + BitUtil::SignExtend<26>(imm24 << 2);

    reg_alloc.FlushEverything();
    current.arm_pc += GetInstSize();
    CompileUpdateCycles(false);
    CompileJumpToBB(new_pc);

    if (cond == Cond::AL) {
        stop_compilation = true;
    }
}

void JitX64::BL(Cond cond, ArmImm24 imm24) {
    cond_manager.CompileCond(cond);

    const u32 new_pc = GetReg15Value() + BitUtil::SignExtend<26>(imm24 << 2);

    ASSERT(!current.TFlag);
    const u32 link_pc = current.arm_pc + GetInstSize();
    Gen::OpArg LR = reg_alloc.LockArmForWrite(ArmReg::LR);
    code->MOV(32, LR, Imm32(link_pc));
    reg_alloc.UnlockArm(ArmReg::LR);

    reg_alloc.FlushEverything();
    current.arm_pc += GetInstSize();
    CompileUpdateCycles(false);
    CompileJumpToBB(new_pc);

    if (cond == Cond::AL) {
        stop_compilation = true;
    }
}

void JitX64::BLX_imm(bool H, ArmImm24 imm24) {
    cond_manager.Always();

    const u32 new_pc = GetReg15Value() + BitUtil::SignExtend<26>(imm24 << 2) + (H ? 2 : 0);

    ASSERT(!current.TFlag);
    const u32 link_pc = current.arm_pc + GetInstSize();
    Gen::OpArg LR = reg_alloc.LockArmForWrite(ArmReg::LR);
    code->MOV(32, LR, Imm32(link_pc));
    reg_alloc.UnlockArm(ArmReg::LR);

    current.TFlag = true;
    code->MOV(32, MJitStateTFlag(), Imm32(1));

    reg_alloc.FlushEverything();
    current.arm_pc += GetInstSize();
    CompileUpdateCycles(false);
    CompileJumpToBB(new_pc);

    stop_compilation = true;
}

void JitX64::BLX_reg(Cond cond, ArmReg Rm_index) {
    cond_manager.CompileCond(cond);

    ASSERT_MSG(Rm_index != ArmReg::PC, "UNPREDICTABLE");

    const u32 link_pc = current.arm_pc + GetInstSize() + (current.TFlag ? 1 : 0);
    Gen::OpArg LR = reg_alloc.LockArmForWrite(ArmReg::LR);
    code->MOV(32, LR, Imm32(link_pc));
    reg_alloc.UnlockArm(ArmReg::LR);

    Gen::X64Reg Rm = reg_alloc.BindArmForRead(Rm_index);
    code->MOV(32, MJitStateArmPC(), R(Rm));
    code->AND(32, MJitStateArmPC(), Imm32(0xFFFFFFFE));
    code->BT(32, R(Rm), Imm8(0));
    code->SETcc(CC_C, MJitStateTFlag()); // NOTE: current.TFlag is now inaccurate
    reg_alloc.UnlockArm(Rm_index);

    current.arm_pc += GetInstSize();
    CompileReturnToDispatch();

    stop_compilation = true;
}

void JitX64::BX(Cond cond, ArmReg Rm_index) {
    cond_manager.CompileCond(cond);

    if (Rm_index == ArmReg::PC) {
        code->MOV(32, MJitStateArmPC(), Imm32(GetReg15Value()));
        code->MOV(32, MJitStateTFlag(), Imm32(0));
    } else {
        Gen::X64Reg Rm = reg_alloc.BindArmForRead(Rm_index);
        code->MOV(32, MJitStateArmPC(), R(Rm));
        code->AND(32, MJitStateArmPC(), Imm32(0xFFFFFFFE));
        code->BT(32, R(Rm), Imm8(0));
        code->SETcc(CC_C, MJitStateTFlag()); // NOTE: current.TFlag is now inaccurate
        reg_alloc.UnlockArm(Rm_index);
    }

    current.arm_pc += GetInstSize();
    CompileReturnToDispatch();

    stop_compilation = true;
}

void JitX64::BXJ(Cond cond, ArmReg Rm) {
    // BXJ behaves exactly as BX since Jazelle is not supported
    BX(cond, Rm);
}

} // namespace JitX64
