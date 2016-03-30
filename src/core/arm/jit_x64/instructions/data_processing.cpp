// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/common_funcs.h"

#include "core/arm/jit_x64/jit_x64.h"

namespace JitX64 {

using namespace Gen;

void JitX64::CompileDataProcessingHelper(ArmReg Rn_index, ArmReg Rd_index, std::function<void(X64Reg)> body) {
    if (Rn_index == 15) {
        X64Reg Rd = reg_alloc.BindArmForWrite(Rd_index);

        code->MOV(32, R(Rd), Imm32(GetReg15Value()));
        body(Rd);

        reg_alloc.UnlockArm(Rd_index);
    } else if (Rn_index == Rd_index) { // Note: Rd_index cannot possibly be 15 in this case.
        X64Reg Rd = reg_alloc.BindArmForReadWrite(Rd_index);

        body(Rd);

        reg_alloc.UnlockArm(Rd_index);
    } else {
        X64Reg Rd = reg_alloc.BindArmForWrite(Rd_index);
        OpArg Rn = reg_alloc.LockArmForRead(Rn_index);
        code->MOV(32, R(Rd), Rn);
        reg_alloc.UnlockArm(Rn_index);

        body(Rd);

        reg_alloc.UnlockArm(Rd_index);
    }
}

void JitX64::CompileDataProcessingHelper_Reverse(ArmReg Rn_index, ArmReg Rd_index, std::function<void(X64Reg)> body) {
    if (Rd_index != Rn_index) {
        X64Reg Rd = reg_alloc.BindArmForWrite(Rd_index);

        body(Rd);

        reg_alloc.UnlockArm(Rd_index);
    } else {
        X64Reg tmp = reg_alloc.AllocTemp();

        body(tmp);

        if (Rd_index != 15) {
            // TODO: Efficiency: Could implement this as a register rebind instead of needing to MOV.
            OpArg Rd = reg_alloc.LockArmForReadWrite(Rd_index);
            code->MOV(32, Rd, R(tmp));
            reg_alloc.UnlockArm(Rd_index);
        } else {
            code->MOV(32, MJitStateArmPC(), R(tmp));
        }

        reg_alloc.UnlockTemp(tmp);
    }
}

X64Reg JitX64::CompileDataProcessingHelper_reg(ArmImm5 imm5, ShiftType shift, ArmReg Rm_index, bool do_shifter_carry_out) {
    // We directly put shifter_carry_out into MJitStateCFlag()

    X64Reg tmp = reg_alloc.AllocTemp();

    if (Rm_index != 15) {
        OpArg Rm = reg_alloc.LockArmForRead(Rm_index);
        code->MOV(32, R(tmp), Rm);
        reg_alloc.UnlockArm(Rm_index);
    } else {
        code->MOV(32, R(tmp), Imm32(GetReg15Value()));
    }

    if (do_shifter_carry_out) {
        cond_manager.FlagsDirty();
    }

    switch (shift) {
    case 0b00: // Logical shift left by immediate
        if (imm5 != 0) {
            code->SHL(32, R(tmp), Imm8(imm5));
            if (do_shifter_carry_out) {
                code->SETcc(CC_C, MJitStateCFlag());
            }
        }
        return tmp;
    case 0b01: // Logical shift right by immediate
        if (imm5 == 0) {
            if (do_shifter_carry_out) {
                code->BT(32, R(tmp), Imm8(31));
                code->SETcc(CC_C, MJitStateCFlag());
            }
            code->MOV(64, R(tmp), Imm32(0));
        } else {
            code->SHR(32, R(tmp), Imm8(imm5));
            if (do_shifter_carry_out) {
                code->SETcc(CC_C, MJitStateCFlag());
            }
        }
        return tmp;
    case 0b10: // Arithmetic shift right by immediate
        if (imm5 == 0) {
            if (do_shifter_carry_out) {
                code->BT(32, R(tmp), Imm8(31));
                code->SETcc(CC_C, MJitStateCFlag());
            }
            code->SAR(32, R(tmp), Imm8(31));
        } else {
            code->SAR(32, R(tmp), Imm8(imm5));
            if (do_shifter_carry_out) {
                code->SETcc(CC_C, MJitStateCFlag());
            }
        }
        return tmp;
    case 0b11: // Rotate right by immediate
        if (imm5 == 0) { //RRX
            code->BT(8, MJitStateCFlag(), Imm8(0));
            code->RCR(32, R(tmp), Imm8(1));
            if (do_shifter_carry_out)
                code->SETcc(CC_C, MJitStateCFlag());
        } else {
            code->ROR(32, R(tmp), Imm8(imm5));
            if (do_shifter_carry_out)
                code->SETcc(CC_C, MJitStateCFlag());
        }
        return tmp;
    }

    UNREACHABLE();
}

void JitX64::ADC_imm(Cond cond, bool S, ArmReg Rn_index, ArmReg Rd_index, int rotate, ArmImm8 imm8) {
    cond_manager.CompileCond((ConditionCode)cond);

    u32 immediate = rotr(imm8, rotate * 2);

    CompileDataProcessingHelper(Rn_index, Rd_index, [&](X64Reg Rd) {
        code->BT(32, MJitStateCFlag(), Imm8(0));
        code->ADC(32, R(Rd), Imm32(immediate));
    });

    if (S) {
        UpdateFlagsZVCN();
    }

    current.arm_pc += GetInstSize();
    if (Rd_index == 15) {
        CompileReturnToDispatch();
    }
}

void JitX64::ADC_reg(Cond cond, bool S, ArmReg Rn_index, ArmReg Rd_index, ArmImm5 imm5, ShiftType shift, ArmReg Rm_index) {
    cond_manager.CompileCond((ConditionCode)cond);

    Gen::X64Reg tmp = CompileDataProcessingHelper_reg(imm5, shift, Rm_index, false);

    CompileDataProcessingHelper(Rn_index, Rd_index, [&](X64Reg Rd) {
        code->BT(32, MJitStateCFlag(), Imm8(0));
        code->ADC(32, R(Rd), R(tmp));
    });

    reg_alloc.UnlockTemp(tmp);

    if (S) {
        UpdateFlagsZVCN();
    }

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
        UpdateFlagsZVCN();
    }

    current.arm_pc += GetInstSize();
    if (Rd_index == 15) {
        CompileReturnToDispatch();
    }
}

void JitX64::ADD_reg(Cond cond, bool S, ArmReg Rn_index, ArmReg Rd_index, ArmImm5 imm5, ShiftType shift, ArmReg Rm_index) {
    cond_manager.CompileCond((ConditionCode)cond);

    Gen::X64Reg tmp = CompileDataProcessingHelper_reg(imm5, shift, Rm_index, false);

    CompileDataProcessingHelper(Rn_index, Rd_index, [&](X64Reg Rd) {
        code->ADD(32, R(Rd), R(tmp));
    });

    reg_alloc.UnlockTemp(tmp);

    if (S) {
        UpdateFlagsZVCN();
    }

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
        UpdateFlagsZN();
        if (rotate != 0) {
            code->MOV(32, MJitStateCFlag(), Imm32(immediate & 0x80000000 ? 1 : 0));
        }
    }

    current.arm_pc += GetInstSize();
    if (Rd_index == 15) {
        CompileReturnToDispatch();
    }
}

void JitX64::AND_reg(Cond cond, bool S, ArmReg Rn_index, ArmReg Rd_index, ArmImm5 imm5, ShiftType shift, ArmReg Rm_index) {
    cond_manager.CompileCond((ConditionCode)cond);

    Gen::X64Reg tmp = CompileDataProcessingHelper_reg(imm5, shift, Rm_index, S);

    CompileDataProcessingHelper(Rn_index, Rd_index, [&](X64Reg Rd) {
        code->AND(32, R(Rd), R(tmp));
    });

    reg_alloc.UnlockTemp(tmp);

    if (S) {
        UpdateFlagsZN();
    }

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
        UpdateFlagsZN();
        if (rotate != 0) {
            code->MOV(32, MJitStateCFlag(), Imm32(immediate & 0x80000000 ? 1 : 0));
        }
    }

    current.arm_pc += GetInstSize();
    if (Rd_index == 15) {
        CompileReturnToDispatch();
    }
}

void JitX64::BIC_reg(Cond cond, bool S, ArmReg Rn_index, ArmReg Rd_index, ArmImm5 imm5, ShiftType shift, ArmReg Rm_index) {
    cond_manager.CompileCond((ConditionCode)cond);

    Gen::X64Reg tmp = CompileDataProcessingHelper_reg(imm5, shift, Rm_index, S);

    CompileDataProcessingHelper(Rn_index, Rd_index, [&](X64Reg Rd) {
        // TODO: Use ANDN instead.
        code->NOT(32, R(tmp));
        code->AND(32, R(Rd), R(tmp));
    });

    reg_alloc.UnlockTemp(tmp);

    if (S) {
        UpdateFlagsZN();
    }

    current.arm_pc += GetInstSize();
    if (Rd_index == 15) {
        CompileReturnToDispatch();
    }
}

void JitX64::CMN_imm(Cond cond, ArmReg Rn_index, int rotate, ArmImm8 imm8) {
    cond_manager.CompileCond((ConditionCode)cond);

    u32 immediate = rotr(imm8, rotate * 2);

    X64Reg tmp = reg_alloc.AllocTemp();
    if (Rn_index != 15) {
        OpArg Rn = reg_alloc.LockArmForRead(Rn_index);
        code->MOV(32, R(tmp), Rn);
        reg_alloc.UnlockArm(Rn_index);
    } else {
        code->MOV(32, R(tmp), Imm32(GetReg15Value()));
    }

    code->ADD(32, R(tmp), Imm32(immediate));

    reg_alloc.UnlockTemp(tmp);

    UpdateFlagsZVCN();

    current.arm_pc += GetInstSize();
}

void JitX64::CMN_reg(Cond cond, ArmReg Rn_index, ArmImm5 imm5, ShiftType shift, ArmReg Rm_index) {
    cond_manager.CompileCond((ConditionCode)cond);

    Gen::X64Reg tmp = CompileDataProcessingHelper_reg(imm5, shift, Rm_index, false);

    if (Rn_index != 15) {
        OpArg Rn = reg_alloc.LockArmForRead(Rn_index);
        code->ADD(32, R(tmp), Rn);
        reg_alloc.UnlockArm(Rn_index);
    } else {
        code->ADD(32, R(tmp), Imm32(GetReg15Value()));
    }

    reg_alloc.UnlockTemp(tmp);

    UpdateFlagsZVCN();

    current.arm_pc += GetInstSize();
}

void JitX64::CMP_imm(Cond cond, ArmReg Rn_index, int rotate, ArmImm8 imm8) {
    cond_manager.CompileCond((ConditionCode)cond);

    u32 immediate = rotr(imm8, rotate * 2);

    if (Rn_index != 15) {
        OpArg Rn = reg_alloc.LockArmForRead(Rn_index);
        code->CMP(32, Rn, Imm32(immediate));
        reg_alloc.UnlockArm(Rn_index);
    } else {
        // TODO: Optimize this
        X64Reg tmp = reg_alloc.AllocTemp();
        code->MOV(32, R(tmp), Imm32(GetReg15Value()));
        code->CMP(32, R(tmp), Imm32(immediate));
        reg_alloc.UnlockTemp(tmp);
    }

    UpdateFlagsC_complement();
    UpdateFlagsZVN();

    current.arm_pc += GetInstSize();
}

void JitX64::CMP_reg(Cond cond, ArmReg Rn_index, ArmImm5 imm5, ShiftType shift, ArmReg Rm_index) {
    cond_manager.CompileCond((ConditionCode)cond);

    Gen::X64Reg tmp = CompileDataProcessingHelper_reg(imm5, shift, Rm_index, false);

    if (Rn_index != 15) {
        OpArg Rn = reg_alloc.LockArmForRead(Rn_index);
        code->CMP(32, Rn, R(tmp));
        reg_alloc.UnlockArm(Rn_index);
    } else {
        // TODO: Optimize this
        X64Reg tmp2 = reg_alloc.AllocTemp();
        code->MOV(32, R(tmp2), Imm32(GetReg15Value()));
        code->CMP(32, R(tmp2), R(tmp));
        reg_alloc.UnlockTemp(tmp2);
    }

    reg_alloc.UnlockTemp(tmp);

    UpdateFlagsC_complement();
    UpdateFlagsZVN();

    current.arm_pc += GetInstSize();
}

void JitX64::EOR_imm(Cond cond, bool S, ArmReg Rn_index, ArmReg Rd_index, int rotate, ArmImm8 imm8) {
    cond_manager.CompileCond((ConditionCode)cond);

    u32 immediate = rotr(imm8, rotate * 2);

    CompileDataProcessingHelper(Rn_index, Rd_index, [&](X64Reg Rd) {
        code->XOR(32, R(Rd), Imm32(immediate));
    });

    if (S) {
        UpdateFlagsZN();
        if (rotate != 0) {
            code->MOV(32, MJitStateCFlag(), Imm32(immediate & 0x80000000 ? 1 : 0));
        }
    }

    current.arm_pc += GetInstSize();
    if (Rd_index == 15) {
        CompileReturnToDispatch();
    }
}

void JitX64::EOR_reg(Cond cond, bool S, ArmReg Rn_index, ArmReg Rd_index, ArmImm5 imm5, ShiftType shift, ArmReg Rm_index) {
    cond_manager.CompileCond((ConditionCode)cond);

    Gen::X64Reg tmp = CompileDataProcessingHelper_reg(imm5, shift, Rm_index, S);

    CompileDataProcessingHelper(Rn_index, Rd_index, [&](X64Reg Rd) {
        code->XOR(32, R(Rd), R(tmp));
    });

    reg_alloc.UnlockTemp(tmp);

    if (S) {
        UpdateFlagsZN();
    }

    current.arm_pc += GetInstSize();
    if (Rd_index == 15) {
        CompileReturnToDispatch();
    }
}

void JitX64::MOV_imm(Cond cond, bool S, ArmReg Rd_index, int rotate, ArmImm8 imm8) {
    cond_manager.CompileCond((ConditionCode)cond);

    u32 immediate = rotr(imm8, rotate * 2);

    Gen::OpArg Rd = reg_alloc.LockArmForWrite(Rd_index);
    code->MOV(32, Rd, Imm32(immediate));
    reg_alloc.UnlockArm(Rd_index);

    if (S) {
        Gen::OpArg Rd = reg_alloc.LockArmForRead(Rd_index);
        code->CMP(32, Rd, Imm32(0));
        reg_alloc.UnlockArm(Rd_index);
        UpdateFlagsZN();
        if (rotate != 0) {
            code->MOV(32, MJitStateCFlag(), Imm32(immediate & 0x80000000 ? 1 : 0));
        }
    }

    current.arm_pc += GetInstSize();
    if (Rd_index == 15) {
        CompileReturnToDispatch();
    }
}

void JitX64::MOV_reg(Cond cond, bool S, ArmReg Rd_index, ArmImm5 imm5, ShiftType shift, ArmReg Rm_index) {
    cond_manager.CompileCond((ConditionCode)cond);

    Gen::X64Reg tmp = CompileDataProcessingHelper_reg(imm5, shift, Rm_index, S);

    Gen::OpArg Rd = reg_alloc.LockArmForWrite(Rd_index);
    code->MOV(32, Rd, R(tmp));
    reg_alloc.UnlockArm(Rd_index);

    if (S) {
        code->CMP(32, R(tmp), Imm32(0));
        UpdateFlagsZN();
    }

    reg_alloc.UnlockTemp(tmp);

    current.arm_pc += GetInstSize();
    if (Rd_index == 15) {
        CompileReturnToDispatch();
    }
}

void JitX64::MVN_imm(Cond cond, bool S, ArmReg Rd_index, int rotate, ArmImm8 imm8) {
    cond_manager.CompileCond((ConditionCode)cond);

    u32 immediate = rotr(imm8, rotate * 2);

    Gen::OpArg Rd = reg_alloc.LockArmForWrite(Rd_index);
    code->MOV(32, Rd, Imm32(~immediate));
    reg_alloc.UnlockArm(Rd_index);

    if (S) {
        Gen::OpArg Rd = reg_alloc.LockArmForRead(Rd_index);
        code->CMP(32, Rd, Imm32(0));
        reg_alloc.UnlockArm(Rd_index);
        UpdateFlagsZN();
        if (rotate != 0) {
            code->MOV(32, MJitStateCFlag(), Imm32(immediate & 0x80000000 ? 1 : 0));
        }
    }

    current.arm_pc += GetInstSize();
    if (Rd_index == 15) {
        CompileReturnToDispatch();
    }
}

void JitX64::MVN_reg(Cond cond, bool S, ArmReg Rd_index, ArmImm5 imm5, ShiftType shift, ArmReg Rm_index) {
    cond_manager.CompileCond((ConditionCode)cond);

    Gen::X64Reg tmp = CompileDataProcessingHelper_reg(imm5, shift, Rm_index, S);
    code->NOT(32, R(tmp));

    Gen::OpArg Rd = reg_alloc.LockArmForWrite(Rd_index);
    code->MOV(32, Rd, R(tmp));
    reg_alloc.UnlockArm(Rd_index);

    if (S) {
        code->CMP(32, R(tmp), Imm32(0));
        UpdateFlagsZN();
    }

    reg_alloc.UnlockTemp(tmp);

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
        UpdateFlagsZN();
        if (rotate != 0) {
            code->MOV(32, MJitStateCFlag(), Imm32(immediate & 0x80000000 ? 1 : 0));
        }
    }

    current.arm_pc += GetInstSize();
    if (Rd_index == 15) {
        CompileReturnToDispatch();
    }
}

void JitX64::ORR_reg(Cond cond, bool S, ArmReg Rn_index, ArmReg Rd_index, ArmImm5 imm5, ShiftType shift, ArmReg Rm_index) {
    cond_manager.CompileCond((ConditionCode)cond);

    Gen::X64Reg tmp = CompileDataProcessingHelper_reg(imm5, shift, Rm_index, S);

    CompileDataProcessingHelper(Rn_index, Rd_index, [&](X64Reg Rd) {
        code->OR(32, R(Rd), R(tmp));
    });

    reg_alloc.UnlockTemp(tmp);

    if (S) {
        UpdateFlagsZN();
    }

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
            Gen::OpArg Rn = reg_alloc.LockArmForRead(Rn_index);
            code->SUB(32, R(Rd), Rn);
            reg_alloc.UnlockArm(Rn_index);
        }
    });

    if (S) {
        UpdateFlagsZVN();
        UpdateFlagsC_complement();
    }

    current.arm_pc += GetInstSize();
    if (Rd_index == 15) {
        CompileReturnToDispatch();
    }
}

void JitX64::RSB_reg(Cond cond, bool S, ArmReg Rn_index, ArmReg Rd_index, ArmImm5 imm5, ShiftType shift, ArmReg Rm_index) {
    cond_manager.CompileCond((ConditionCode)cond);

    Gen::X64Reg tmp = CompileDataProcessingHelper_reg(imm5, shift, Rm_index, false);

    CompileDataProcessingHelper_Reverse(Rn_index, Rd_index, [&](X64Reg Rd) {
        code->MOV(32, R(Rd), R(tmp));

        if (Rn_index == 15) {
            code->SUB(32, R(Rd), Imm32(GetReg15Value()));
        } else {
            Gen::OpArg Rn = reg_alloc.LockArmForRead(Rn_index);
            code->SUB(32, R(Rd), Rn);
            reg_alloc.UnlockArm(Rn_index);
        }
    });

    reg_alloc.UnlockTemp(tmp);

    if (S) {
        UpdateFlagsZVN();
        UpdateFlagsC_complement();
    }

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
            Gen::OpArg Rn = reg_alloc.LockArmForRead(Rn_index);
            code->SBB(32, R(Rd), Rn);
            reg_alloc.UnlockArm(Rn_index);
        }
    });

    if (S) {
        UpdateFlagsZVN();
        UpdateFlagsC_complement();
    }

    current.arm_pc += GetInstSize();
    if (Rd_index == 15) {
        CompileReturnToDispatch();
    }
}

void JitX64::RSC_reg(Cond cond, bool S, ArmReg Rn_index, ArmReg Rd_index, ArmImm5 imm5, ShiftType shift, ArmReg Rm_index) {
    cond_manager.CompileCond((ConditionCode)cond);

    Gen::X64Reg tmp = CompileDataProcessingHelper_reg(imm5, shift, Rm_index, false);

    CompileDataProcessingHelper_Reverse(Rn_index, Rd_index, [&](X64Reg Rd) {
        code->MOV(32, R(Rd), R(tmp));

        code->BT(32, MJitStateCFlag(), Imm8(0));
        code->CMC();

        if (Rn_index == 15) {
            code->SBB(32, R(Rd), Imm32(GetReg15Value()));
        } else {
            Gen::OpArg Rn = reg_alloc.LockArmForRead(Rn_index);
            code->SBB(32, R(Rd), Rn);
            reg_alloc.UnlockArm(Rn_index);
        }
    });

    reg_alloc.UnlockTemp(tmp);

    if (S) {
        UpdateFlagsZVN();
        UpdateFlagsC_complement();
    }

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
        UpdateFlagsZVN();
        UpdateFlagsC_complement();
    }

    current.arm_pc += GetInstSize();
    if (Rd_index == 15) {
        CompileReturnToDispatch();
    }
}

void JitX64::SBC_reg(Cond cond, bool S, ArmReg Rn_index, ArmReg Rd_index, ArmImm5 imm5, ShiftType shift, ArmReg Rm_index) {
    cond_manager.CompileCond((ConditionCode)cond);

    Gen::X64Reg tmp = CompileDataProcessingHelper_reg(imm5, shift, Rm_index, false);

    CompileDataProcessingHelper(Rn_index, Rd_index, [&](X64Reg Rd) {
        code->BT(32, MJitStateCFlag(), Imm8(0));
        code->CMC();
        code->SBB(32, R(Rd), R(tmp));
    });

    reg_alloc.UnlockTemp(tmp);

    if (S) {
        UpdateFlagsZVN();
        UpdateFlagsC_complement();
    }

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
        UpdateFlagsZVN();
        UpdateFlagsC_complement();
    }

    current.arm_pc += GetInstSize();
    if (Rd_index == 15) {
        CompileReturnToDispatch();
    }
}

void JitX64::SUB_reg(Cond cond, bool S, ArmReg Rn_index, ArmReg Rd_index, ArmImm5 imm5, ShiftType shift, ArmReg Rm_index) {
    cond_manager.CompileCond((ConditionCode)cond);

    Gen::X64Reg tmp = CompileDataProcessingHelper_reg(imm5, shift, Rm_index, false);

    CompileDataProcessingHelper(Rn_index, Rd_index, [&](X64Reg Rd) {
        code->SUB(32, R(Rd), R(tmp));
    });

    reg_alloc.UnlockTemp(tmp);

    if (S) {
        UpdateFlagsZVN();
        UpdateFlagsC_complement();
    }

    current.arm_pc += GetInstSize();
    if (Rd_index == 15) {
        CompileReturnToDispatch();
    }
}

void JitX64::TEQ_imm(Cond cond, ArmReg Rn_index, int rotate, ArmImm8 imm8) {
    cond_manager.CompileCond((ConditionCode)cond);

    u32 immediate = rotr(imm8, rotate * 2);

    X64Reg Rn_tmp = reg_alloc.AllocTemp();

    if (Rn_index == 15) {
        code->MOV(32, R(Rn_tmp), Imm32(GetReg15Value()));
    } else {
        Gen::OpArg Rn_real = reg_alloc.LockArmForRead(Rn_index);
        code->MOV(32, R(Rn_tmp), Rn_real);
        reg_alloc.UnlockArm(Rn_index);
    }

    code->XOR(32, R(Rn_tmp), Imm32(immediate));

    reg_alloc.UnlockTemp(Rn_tmp);

    UpdateFlagsZN();
    if (rotate != 0) {
        code->MOV(32, MJitStateCFlag(), Imm32(immediate & 0x80000000 ? 1 : 0));
    }

    current.arm_pc += GetInstSize();
}

void JitX64::TEQ_reg(Cond cond, ArmReg Rn_index, ArmImm5 imm5, ShiftType shift, ArmReg Rm_index) {
    cond_manager.CompileCond((ConditionCode)cond);

    Gen::X64Reg tmp = CompileDataProcessingHelper_reg(imm5, shift, Rm_index, true);

    if (Rn_index == 15) {
        code->XOR(32, R(tmp), Imm32(GetReg15Value()));
    } else {
        Gen::OpArg Rn = reg_alloc.LockArmForRead(Rn_index);
        code->XOR(32, R(tmp), Rn);
        reg_alloc.UnlockArm(Rn_index);
    }

    reg_alloc.UnlockTemp(tmp);

    UpdateFlagsZN();

    current.arm_pc += GetInstSize();
}

void JitX64::TST_imm(Cond cond, ArmReg Rn_index, int rotate, ArmImm8 imm8) {
    cond_manager.CompileCond((ConditionCode)cond);

    u32 immediate = rotr(imm8, rotate * 2);

    X64Reg Rn;

    if (Rn_index == 15) {
        Rn = reg_alloc.AllocTemp();
        code->MOV(32, R(Rn), Imm32(GetReg15Value()));
    } else {
        Rn = reg_alloc.BindArmForRead(Rn_index);
    }

    code->TEST(32, R(Rn), Imm32(immediate));

    if (Rn_index == 15) {
        reg_alloc.UnlockTemp(Rn);
    } else {
        reg_alloc.UnlockArm(Rn_index);
    }

    UpdateFlagsZN();
    if (rotate != 0) {
        code->MOV(32, MJitStateCFlag(), Imm32(immediate & 0x80000000 ? 1 : 0));
    }

    current.arm_pc += GetInstSize();
}

void JitX64::TST_reg(Cond cond, ArmReg Rn_index, ArmImm5 imm5, ShiftType shift, ArmReg Rm_index) {
    cond_manager.CompileCond((ConditionCode)cond);

    Gen::X64Reg tmp = CompileDataProcessingHelper_reg(imm5, shift, Rm_index, true);

    if (Rn_index == 15) {
        code->TEST(32, R(tmp), Imm32(GetReg15Value()));
    } else {
        OpArg Rn = reg_alloc.LockArmForRead(Rn_index);
        code->TEST(32, R(tmp), Rn);
        reg_alloc.UnlockArm(Rn_index);
    }

    reg_alloc.UnlockTemp(tmp);

    UpdateFlagsZN();

    current.arm_pc += GetInstSize();
}

void JitX64::ADC_rsr(Cond cond, bool S, ArmReg Rn_index, ArmReg Rd_index, ArmReg Rs_index, ShiftType shift, ArmReg Rm_index) { CompileInterpretInstruction(); }
void JitX64::ADD_rsr(Cond cond, bool S, ArmReg Rn_index, ArmReg Rd_index, ArmReg Rs_index, ShiftType shift, ArmReg Rm_index) { CompileInterpretInstruction(); }
void JitX64::AND_rsr(Cond cond, bool S, ArmReg Rn_index, ArmReg Rd_index, ArmReg Rs_index, ShiftType shift, ArmReg Rm_index) { CompileInterpretInstruction(); }
void JitX64::BIC_rsr(Cond cond, bool S, ArmReg Rn_index, ArmReg Rd_index, ArmReg Rs_index, ShiftType shift, ArmReg Rm_index) { CompileInterpretInstruction(); }
void JitX64::CMN_rsr(Cond cond, ArmReg Rn_index, ArmReg Rs_index, ShiftType shift, ArmReg Rm_index) { CompileInterpretInstruction(); }
void JitX64::CMP_rsr(Cond cond, ArmReg Rn_index, ArmReg Rs_index, ShiftType shift, ArmReg Rm_index) { CompileInterpretInstruction(); }
void JitX64::EOR_rsr(Cond cond, bool S, ArmReg Rn_index, ArmReg Rd_index, ArmReg Rs_index, ShiftType shift, ArmReg Rm_index) { CompileInterpretInstruction(); }
void JitX64::MOV_rsr(Cond cond, bool S, ArmReg Rd_index, ArmReg Rs_index, ShiftType shift, ArmReg Rm_index) { CompileInterpretInstruction(); }
void JitX64::MVN_rsr(Cond cond, bool S, ArmReg Rd_index, ArmReg Rs_index, ShiftType shift, ArmReg Rm_index) { CompileInterpretInstruction(); }
void JitX64::ORR_rsr(Cond cond, bool S, ArmReg Rn_index, ArmReg Rd_index, ArmReg Rs_index, ShiftType shift, ArmReg Rm_index) { CompileInterpretInstruction(); }
void JitX64::RSB_rsr(Cond cond, bool S, ArmReg Rn_index, ArmReg Rd_index, ArmReg Rs_index, ShiftType shift, ArmReg Rm_index) { CompileInterpretInstruction(); }
void JitX64::RSC_rsr(Cond cond, bool S, ArmReg Rn_index, ArmReg Rd_index, ArmReg Rs_index, ShiftType shift, ArmReg Rm_index) { CompileInterpretInstruction(); }
void JitX64::SBC_rsr(Cond cond, bool S, ArmReg Rn_index, ArmReg Rd_index, ArmReg Rs_index, ShiftType shift, ArmReg Rm_index) { CompileInterpretInstruction(); }
void JitX64::SUB_rsr(Cond cond, bool S, ArmReg Rn_index, ArmReg Rd_index, ArmReg Rs_index, ShiftType shift, ArmReg Rm_index) { CompileInterpretInstruction(); }
void JitX64::TEQ_rsr(Cond cond, ArmReg Rn_index, ArmReg Rs_index, ShiftType shift, ArmReg Rm_index) { CompileInterpretInstruction(); }
void JitX64::TST_rsr(Cond cond, ArmReg Rn_index, ArmReg Rs_index, ShiftType shift, ArmReg Rm_index) { CompileInterpretInstruction(); }

}
