// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/common_funcs.h"

#include "core/arm/jit_x64/jit_x64.h"

namespace JitX64 {

using namespace Gen;

void JitX64::CompileDataProcessingHelper(ArmReg Rn_index, ArmReg Rd_index, std::function<void(X64Reg)> body) {
    // The major consideration is if Rn and/or Rd == R15.

    if (Rd_index == 15) {
        X64Reg Rd = reg_alloc.AllocAndLockTemp();
        reg_alloc.LockArm(Rn_index);

        if (Rn_index == 15) {
            // TODO: In this case we can actually calculat the final result.
            //       We can also use CompileJumpToBB instead of having to use CompileReturnToDispatch.
            code->MOV(32, R(Rd), Imm32(GetReg15Value()));
        } else {
            code->MOV(32, R(Rd), reg_alloc.ArmR(Rn_index));
        }

        body(Rd);

        reg_alloc.UnlockArm(Rn_index);

        code->MOV(32, MJitStateArmPC(), R(Rd));
        reg_alloc.UnlockTemp(Rd);
    } else if (Rn_index == 15) {
        X64Reg Rd = reg_alloc.BindNoLoadAndLockArm(Rd_index);
        reg_alloc.MarkDirty(Rd_index);

        code->MOV(32, R(Rd), Imm32(GetReg15Value()));

        body(Rd);

        reg_alloc.UnlockArm(Rd_index);
    } else if (Rn_index == Rd_index) {
        X64Reg Rd = reg_alloc.BindAndLockArm(Rd_index);
        reg_alloc.MarkDirty(Rd_index);

        body(Rd);

        reg_alloc.UnlockArm(Rd_index);
    } else {
        X64Reg Rd = reg_alloc.BindNoLoadAndLockArm(Rd_index);
        reg_alloc.MarkDirty(Rd_index);
        reg_alloc.LockArm(Rn_index);

        code->MOV(32, R(Rd), reg_alloc.ArmR(Rn_index));

        body(Rd);

        reg_alloc.UnlockArm(Rd_index);
        reg_alloc.UnlockArm(Rn_index);
    }
}

void JitX64::CompileDataProcessingHelper_Reverse(ArmReg Rn_index, ArmReg Rd_index, std::function<void(X64Reg)> body) {
    // The major consideration is if Rn and/or Rd == R15.

    if (Rd_index != Rn_index) {

        X64Reg Rd = INVALID_REG;

        if (Rd_index == 15) {
            Rd = reg_alloc.AllocAndLockTemp();
        } else {
            Rd = reg_alloc.BindNoLoadAndLockArm(Rd_index);
            reg_alloc.MarkDirty(Rd_index);
        }

        body(Rd);

        if (Rd_index == 15) {
            code->MOV(32, MJitStateArmPC(), R(Rd));
            reg_alloc.UnlockTemp(Rd);
        } else {
            reg_alloc.UnlockArm(Rd_index);
        }

    } else {

        X64Reg tmp = reg_alloc.AllocAndLockTemp();

        body(tmp);

        // TODO: Efficiency: Could implement this as a register rebind instead of needing to MOV.
        reg_alloc.LockArm(Rd_index);
        code->MOV(32, reg_alloc.ArmR(Rd_index), R(tmp));
        reg_alloc.UnlockArm(Rd_index);
        reg_alloc.UnlockTemp(tmp);

    }
}

void JitX64::ADC_imm(Cond cond, bool S, ArmReg Rn_index, ArmReg Rd_index, int rotate, ArmImm8 imm8) {
    cond_manager.CompileCond((ConditionCode)cond);

    u32 immediate = rotr(imm8, rotate * 2);

    CompileDataProcessingHelper(Rn_index, Rd_index, [&](X64Reg Rd) {
        code->BT(32, MJitStateCFlag(), Imm8(0));
        code->ADC(32, R(Rd), Imm32(immediate));
    });

    if (S) {
        cond_manager.FlagsDirty();
        UpdateFlagsZVCN();
    }

    reg_alloc.AssertNoLocked();

    current.arm_pc += GetInstSize();
    if (Rd_index == 15) {
        CompileReturnToDispatch();
    }
}

void JitX64::ADD_imm(Cond cond, bool S, ArmReg Rn_index, ArmReg Rd_index, int rotate, ArmImm8 imm8) {
    cond_manager.CompileCond((ConditionCode)cond);

    u32 immediate = rotr(imm8, rotate * 2);

    CompileDataProcessingHelper(Rn_index, Rd_index, [&](X64Reg Rd) {
        code->ADD(32, R(Rd), Imm32(immediate));
    });

    if (S) {
        cond_manager.FlagsDirty();
        UpdateFlagsZVCN();
    }

    reg_alloc.AssertNoLocked();

    current.arm_pc += GetInstSize();
    if (Rd_index == 15) {
        CompileReturnToDispatch();
    }
}

void JitX64::AND_imm(Cond cond, bool S, ArmReg Rn_index, ArmReg Rd_index, int rotate, ArmImm8 imm8) {
    cond_manager.CompileCond((ConditionCode)cond);

    u32 immediate = rotr(imm8, rotate * 2);

    CompileDataProcessingHelper(Rn_index, Rd_index, [&](X64Reg Rd) {
        code->AND(32, R(Rd), Imm32(immediate));
    });

    if (S) {
        cond_manager.FlagsDirty();
        UpdateFlagsZN();
        if (rotate != 0) {
            code->MOV(32, MJitStateCFlag(), Imm32(immediate & 0x80000000 ? 1 : 0));
        }
    }

    reg_alloc.AssertNoLocked();

    current.arm_pc += GetInstSize();
    if (Rd_index == 15) {
        CompileReturnToDispatch();
    }
}

void JitX64::BIC_imm(Cond cond, bool S, ArmReg Rn_index, ArmReg Rd_index, int rotate, ArmImm8 imm8) {
    cond_manager.CompileCond((ConditionCode)cond);

    u32 immediate = rotr(imm8, rotate * 2);

    CompileDataProcessingHelper(Rn_index, Rd_index, [&](X64Reg Rd) {
        code->AND(32, R(Rd), Imm32(~immediate));
    });

    if (S) {
        cond_manager.FlagsDirty();
        UpdateFlagsZN();
        if (rotate != 0) {
            code->MOV(32, MJitStateCFlag(), Imm32(immediate & 0x80000000 ? 1 : 0));
        }
    }

    reg_alloc.AssertNoLocked();

    current.arm_pc += GetInstSize();
    if (Rd_index == 15) {
        CompileReturnToDispatch();
    }
}

void JitX64::CMN_imm(Cond cond, ArmReg Rn_index, int rotate, ArmImm8 imm8) { CompileInterpretInstruction(); }
void JitX64::CMP_imm(Cond cond, ArmReg Rn_index, int rotate, ArmImm8 imm8) { CompileInterpretInstruction(); }

void JitX64::EOR_imm(Cond cond, bool S, ArmReg Rn_index, ArmReg Rd_index, int rotate, ArmImm8 imm8) {
    cond_manager.CompileCond((ConditionCode)cond);

    u32 immediate = rotr(imm8, rotate * 2);

    CompileDataProcessingHelper(Rn_index, Rd_index, [&](X64Reg Rd) {
        code->XOR(32, R(Rd), Imm32(immediate));
    });

    if (S) {
        cond_manager.FlagsDirty();
        UpdateFlagsZN();
        if (rotate != 0) {
            code->MOV(32, MJitStateCFlag(), Imm32(immediate & 0x80000000 ? 1 : 0));
        }
    }

    reg_alloc.AssertNoLocked();

    current.arm_pc += GetInstSize();
    if (Rd_index == 15) {
        CompileReturnToDispatch();
    }
}

void JitX64::MOV_imm(Cond cond, bool S, ArmReg Rd_index, int rotate, ArmImm8 imm8) {
    cond_manager.CompileCond((ConditionCode)cond);

    u32 immediate = rotr(imm8, rotate * 2);

    if (Rd_index != 15) {
        reg_alloc.LockArm(Rd_index);
        code->MOV(32, reg_alloc.ArmR(Rd_index), Imm32(immediate));
        reg_alloc.UnlockArm(Rd_index);
    } else {
        code->MOV(32, MJitStateArmPC(), Imm32(immediate));
    }

    if (S) {
        cond_manager.FlagsDirty();
        reg_alloc.LockArm(Rd_index);
        code->CMP(32, reg_alloc.ArmR(Rd_index), Imm32(0));
        reg_alloc.UnlockArm(Rd_index);
        UpdateFlagsZN();
        if (rotate != 0) {
            code->MOV(32, MJitStateCFlag(), Imm32(immediate & 0x80000000 ? 1 : 0));
        }
    }

    reg_alloc.AssertNoLocked();

    current.arm_pc += GetInstSize();
    if (Rd_index == 15) {
        CompileReturnToDispatch();
    }
}

void JitX64::MVN_imm(Cond cond, bool S, ArmReg Rd_index, int rotate, ArmImm8 imm8) {
    cond_manager.CompileCond((ConditionCode)cond);

    u32 immediate = rotr(imm8, rotate * 2);

    if (Rd_index != 15) {
        reg_alloc.LockArm(Rd_index);
        code->MOV(32, reg_alloc.ArmR(Rd_index), Imm32(~immediate));
        reg_alloc.UnlockArm(Rd_index);
    } else {
        code->MOV(32, MJitStateArmPC(), Imm32(~immediate));
    }

    if (S) {
        cond_manager.FlagsDirty();
        reg_alloc.LockArm(Rd_index);
        code->CMP(32, reg_alloc.ArmR(Rd_index), Imm32(0));
        reg_alloc.UnlockArm(Rd_index);
        UpdateFlagsZN();
        if (rotate != 0) {
            code->MOV(32, MJitStateCFlag(), Imm32(immediate & 0x80000000 ? 1 : 0));
        }
    }

    reg_alloc.AssertNoLocked();

    current.arm_pc += GetInstSize();
    if (Rd_index == 15) {
        CompileReturnToDispatch();
    }
}

void JitX64::ORR_imm(Cond cond, bool S, ArmReg Rn_index, ArmReg Rd_index, int rotate, ArmImm8 imm8) {
    cond_manager.CompileCond((ConditionCode)cond);

    u32 immediate = rotr(imm8, rotate * 2);

    CompileDataProcessingHelper(Rn_index, Rd_index, [&](X64Reg Rd) {
        code->OR(32, R(Rd), Imm32(immediate));
    });

    if (S) {
        cond_manager.FlagsDirty();
        UpdateFlagsZN();
        if (rotate != 0) {
            code->MOV(32, MJitStateCFlag(), Imm32(immediate & 0x80000000 ? 1 : 0));
        }
    }

    reg_alloc.AssertNoLocked();

    current.arm_pc += GetInstSize();
    if (Rd_index == 15) {
        CompileReturnToDispatch();
    }
}

void JitX64::RSB_imm(Cond cond, bool S, ArmReg Rn_index, ArmReg Rd_index, int rotate, ArmImm8 imm8) {
    cond_manager.CompileCond((ConditionCode)cond);

    u32 immediate = rotr(imm8, rotate * 2);

    CompileDataProcessingHelper_Reverse(Rn_index, Rd_index, [&](X64Reg Rd) {
        code->MOV(32, R(Rd), Imm32(immediate));

        if (Rn_index == 15) {
            code->SUB(32, R(Rd), Imm32(GetReg15Value()));
        } else {
            reg_alloc.LockArm(Rn_index);
            code->SUB(32, R(Rd), reg_alloc.ArmR(Rn_index));
            reg_alloc.UnlockArm(Rn_index);
        }
    });

    if (S) {
        cond_manager.FlagsDirty();
        UpdateFlagsZVN();
        UpdateFlagsC_complement();
    }

    reg_alloc.AssertNoLocked();

    current.arm_pc += GetInstSize();
    if (Rd_index == 15) {
        CompileReturnToDispatch();
    }
}

void JitX64::RSC_imm(Cond cond, bool S, ArmReg Rn_index, ArmReg Rd_index, int rotate, ArmImm8 imm8) {
    cond_manager.CompileCond((ConditionCode)cond);

    u32 immediate = rotr(imm8, rotate * 2);

    CompileDataProcessingHelper_Reverse(Rn_index, Rd_index, [&](X64Reg Rd) {
        code->MOV(32, R(Rd), Imm32(immediate));

        code->BT(32, MJitStateCFlag(), Imm8(0));
        code->CMC();

        if (Rn_index == 15) {
            code->SBB(32, R(Rd), Imm32(GetReg15Value()));
        } else {
            reg_alloc.LockArm(Rn_index);
            code->SBB(32, R(Rd), reg_alloc.ArmR(Rn_index));
            reg_alloc.UnlockArm(Rn_index);
        }
    });

    if (S) {
        cond_manager.FlagsDirty();
        UpdateFlagsZVN();
        UpdateFlagsC_complement();
    }

    reg_alloc.AssertNoLocked();

    current.arm_pc += GetInstSize();
    if (Rd_index == 15) {
        CompileReturnToDispatch();
    }
}

void JitX64::SBC_imm(Cond cond, bool S, ArmReg Rn_index, ArmReg Rd_index, int rotate, ArmImm8 imm8) {
    cond_manager.CompileCond((ConditionCode)cond);

    u32 immediate = rotr(imm8, rotate * 2);

    CompileDataProcessingHelper(Rn_index, Rd_index, [&](X64Reg Rd) {
        code->BT(32, MJitStateCFlag(), Imm8(0));
        code->CMC();
        code->SBB(32, R(Rd), Imm32(immediate));
    });

    if (S) {
        cond_manager.FlagsDirty();
        UpdateFlagsZVN();
        UpdateFlagsC_complement();
    }

    reg_alloc.AssertNoLocked();

    current.arm_pc += GetInstSize();
    if (Rd_index == 15) {
        CompileReturnToDispatch();
    }
}

void JitX64::SUB_imm(Cond cond, bool S, ArmReg Rn_index, ArmReg Rd_index, int rotate, ArmImm8 imm8) {
    cond_manager.CompileCond((ConditionCode)cond);

    u32 immediate = rotr(imm8, rotate * 2);

    CompileDataProcessingHelper(Rn_index, Rd_index, [&](X64Reg Rd) {
        code->SUB(32, R(Rd), Imm32(immediate));
    });

    if (S) {
        cond_manager.FlagsDirty();
        UpdateFlagsZVN();
        UpdateFlagsC_complement();
    }

    reg_alloc.AssertNoLocked();

    current.arm_pc += GetInstSize();
    if (Rd_index == 15) {
        CompileReturnToDispatch();
    }
}

void JitX64::TEQ_imm(Cond cond, ArmReg Rn_index, int rotate, ArmImm8 imm8) {
    cond_manager.CompileCond((ConditionCode)cond);

    u32 immediate = rotr(imm8, rotate * 2);

    X64Reg Rn = reg_alloc.AllocAndLockTemp();

    if (Rn_index == 15) {
        code->MOV(32, R(Rn), Imm32(GetReg15Value()));
    } else {
        reg_alloc.LockArm(Rn_index);
        code->MOV(32, R(Rn), reg_alloc.ArmR(Rn_index));
        reg_alloc.UnlockArm(Rn_index);
    }

    code->XOR(32, R(Rn), Imm32(immediate));

    reg_alloc.UnlockTemp(Rn);

    cond_manager.FlagsDirty();
    UpdateFlagsZN();
    if (rotate != 0) {
        code->MOV(32, MJitStateCFlag(), Imm32(immediate & 0x80000000 ? 1 : 0));
    }

    reg_alloc.AssertNoLocked();

    current.arm_pc += GetInstSize();
}

void JitX64::TST_imm(Cond cond, ArmReg Rn_index, int rotate, ArmImm8 imm8) {
    cond_manager.CompileCond((ConditionCode)cond);

    u32 immediate = rotr(imm8, rotate * 2);

    X64Reg Rn;

    if (Rn_index == 15) {
        Rn = reg_alloc.AllocAndLockTemp();
        code->MOV(32, R(Rn), Imm32(GetReg15Value()));
    } else {
        Rn = reg_alloc.BindAndLockArm(Rn_index);
    }

    code->TEST(32, R(Rn), Imm32(immediate));

    if (Rn_index == 15) {
        reg_alloc.UnlockTemp(Rn);
    } else {
        reg_alloc.UnlockArm(Rn_index);
    }

    cond_manager.FlagsDirty();
    UpdateFlagsZN();
    if (rotate != 0) {
        code->MOV(32, MJitStateCFlag(), Imm32(immediate & 0x80000000 ? 1 : 0));
    }

    reg_alloc.AssertNoLocked();

    current.arm_pc += GetInstSize();
}

void JitX64::ADC_reg(Cond cond, bool S, ArmReg Rn_index, ArmReg Rd_index, ArmImm5 imm5, ShiftType shift, ArmReg Rm) { CompileInterpretInstruction(); }
void JitX64::ADC_rsr(Cond cond, bool S, ArmReg Rn_index, ArmReg Rd_index, ArmReg Rs, ShiftType shift, ArmReg Rm) { CompileInterpretInstruction(); }
void JitX64::ADD_reg(Cond cond, bool S, ArmReg Rn_index, ArmReg Rd_index, ArmImm5 imm5, ShiftType shift, ArmReg Rm) { CompileInterpretInstruction(); }
void JitX64::ADD_rsr(Cond cond, bool S, ArmReg Rn_index, ArmReg Rd_index, ArmReg Rs, ShiftType shift, ArmReg Rm) { CompileInterpretInstruction(); }
void JitX64::AND_reg(Cond cond, bool S, ArmReg Rn_index, ArmReg Rd_index, ArmImm5 imm5, ShiftType shift, ArmReg Rm) { CompileInterpretInstruction(); }
void JitX64::AND_rsr(Cond cond, bool S, ArmReg Rn_index, ArmReg Rd_index, ArmReg Rs, ShiftType shift, ArmReg Rm) { CompileInterpretInstruction(); }
void JitX64::BIC_reg(Cond cond, bool S, ArmReg Rn_index, ArmReg Rd_index, ArmImm5 imm5, ShiftType shift, ArmReg Rm) { CompileInterpretInstruction(); }
void JitX64::BIC_rsr(Cond cond, bool S, ArmReg Rn_index, ArmReg Rd_index, ArmReg Rs, ShiftType shift, ArmReg Rm) { CompileInterpretInstruction(); }
void JitX64::CMN_reg(Cond cond, ArmReg Rn_index, ArmImm5 imm5, ShiftType shift, ArmReg Rm) { CompileInterpretInstruction(); }
void JitX64::CMN_rsr(Cond cond, ArmReg Rn_index, ArmReg Rs, ShiftType shift, ArmReg Rm) { CompileInterpretInstruction(); }
void JitX64::CMP_reg(Cond cond, ArmReg Rn_index, ArmImm5 imm5, ShiftType shift, ArmReg Rm) { CompileInterpretInstruction(); }
void JitX64::CMP_rsr(Cond cond, ArmReg Rn_index, ArmReg Rs, ShiftType shift, ArmReg Rm) { CompileInterpretInstruction(); }
void JitX64::EOR_reg(Cond cond, bool S, ArmReg Rn_index, ArmReg Rd_index, ArmImm5 imm5, ShiftType shift, ArmReg Rm) { CompileInterpretInstruction(); }
void JitX64::EOR_rsr(Cond cond, bool S, ArmReg Rn_index, ArmReg Rd_index, ArmReg Rs, ShiftType shift, ArmReg Rm) { CompileInterpretInstruction(); }
void JitX64::MOV_reg(Cond cond, bool S, ArmReg Rd_index, ArmImm5 imm5, ShiftType shift, ArmReg Rm) { CompileInterpretInstruction(); }
void JitX64::MOV_rsr(Cond cond, bool S, ArmReg Rd_index, ArmReg Rs, ShiftType shift, ArmReg Rm) { CompileInterpretInstruction(); }
void JitX64::MVN_reg(Cond cond, bool S, ArmReg Rd_index, ArmImm5 imm5, ShiftType shift, ArmReg Rm) { CompileInterpretInstruction(); }
void JitX64::MVN_rsr(Cond cond, bool S, ArmReg Rd_index, ArmReg Rs, ShiftType shift, ArmReg Rm) { CompileInterpretInstruction(); }
void JitX64::ORR_reg(Cond cond, bool S, ArmReg Rn_index, ArmReg Rd_index, ArmImm5 imm5, ShiftType shift, ArmReg Rm) { CompileInterpretInstruction(); }
void JitX64::ORR_rsr(Cond cond, bool S, ArmReg Rn_index, ArmReg Rd_index, ArmReg Rs, ShiftType shift, ArmReg Rm) { CompileInterpretInstruction(); }
void JitX64::RSB_reg(Cond cond, bool S, ArmReg Rn_index, ArmReg Rd_index, ArmImm5 imm5, ShiftType shift, ArmReg Rm) { CompileInterpretInstruction(); }
void JitX64::RSB_rsr(Cond cond, bool S, ArmReg Rn_index, ArmReg Rd_index, ArmReg Rs, ShiftType shift, ArmReg Rm) { CompileInterpretInstruction(); }
void JitX64::RSC_reg(Cond cond, bool S, ArmReg Rn_index, ArmReg Rd_index, ArmImm5 imm5, ShiftType shift, ArmReg Rm) { CompileInterpretInstruction(); }
void JitX64::RSC_rsr(Cond cond, bool S, ArmReg Rn_index, ArmReg Rd_index, ArmReg Rs, ShiftType shift, ArmReg Rm) { CompileInterpretInstruction(); }
void JitX64::SBC_reg(Cond cond, bool S, ArmReg Rn_index, ArmReg Rd_index, ArmImm5 imm5, ShiftType shift, ArmReg Rm) { CompileInterpretInstruction(); }
void JitX64::SBC_rsr(Cond cond, bool S, ArmReg Rn_index, ArmReg Rd_index, ArmReg Rs, ShiftType shift, ArmReg Rm) { CompileInterpretInstruction(); }
void JitX64::SUB_reg(Cond cond, bool S, ArmReg Rn_index, ArmReg Rd_index, ArmImm5 imm5, ShiftType shift, ArmReg Rm) { CompileInterpretInstruction(); }
void JitX64::SUB_rsr(Cond cond, bool S, ArmReg Rn_index, ArmReg Rd_index, ArmReg Rs, ShiftType shift, ArmReg Rm) { CompileInterpretInstruction(); }
void JitX64::TEQ_reg(Cond cond, ArmReg Rn_index, ArmImm5 imm5, ShiftType shift, ArmReg Rm) { CompileInterpretInstruction(); }
void JitX64::TEQ_rsr(Cond cond, ArmReg Rn_index, ArmReg Rs, ShiftType shift, ArmReg Rm) { CompileInterpretInstruction(); }
void JitX64::TST_reg(Cond cond, ArmReg Rn_index, ArmImm5 imm5, ShiftType shift, ArmReg Rm) { CompileInterpretInstruction(); }
void JitX64::TST_rsr(Cond cond, ArmReg Rn_index, ArmReg Rs, ShiftType shift, ArmReg Rm) { CompileInterpretInstruction(); }

}
