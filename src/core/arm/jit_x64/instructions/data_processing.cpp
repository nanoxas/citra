// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/common_funcs.h"

#include "core/arm/jit_x64/jit_x64.h"

namespace JitX64 {

using namespace Gen;

void JitX64::CompileDataProcessingHelper(ArmReg Rn_index, ArmReg Rd_index, std::function<void(X64Reg)> body) {
    if (Rn_index == ArmReg::PC) {
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

        if (Rd_index != ArmReg::PC) {
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

void JitX64::CompileShifter_imm(X64Reg dest, ArmImm5 imm5, ShiftType shift, bool do_shifter_carry_out) {
    // dest must contain a copy of the value of Rm.
    // if do_shifter_carry_out,
    //    we output code that calculates and puts shifter_carry_out into MJitStateCFlag().

    switch (shift) {
    case ShiftType::LSL: // Logical shift left by immediate
        if (imm5 != 0) {
            code->SHL(32, R(dest), Imm8(imm5));
            if (do_shifter_carry_out) {
                code->SETcc(CC_C, MJitStateCFlag());
            }
        }
        return;
    case ShiftType::LSR: // Logical shift right by immediate
        if (imm5 == 0) {
            if (do_shifter_carry_out) {
                code->BT(32, R(dest), Imm8(31));
                code->SETcc(CC_C, MJitStateCFlag());
            }
            code->MOV(64, R(dest), Imm32(0));
        } else {
            code->SHR(32, R(dest), Imm8(imm5));
            if (do_shifter_carry_out) {
                code->SETcc(CC_C, MJitStateCFlag());
            }
        }
        return;
    case ShiftType::ASR: // Arithmetic shift right by immediate
        if (imm5 == 0) {
            if (do_shifter_carry_out) {
                code->BT(32, R(dest), Imm8(31));
                code->SETcc(CC_C, MJitStateCFlag());
            }
            code->SAR(32, R(dest), Imm8(31));
        } else {
            code->SAR(32, R(dest), Imm8(imm5));
            if (do_shifter_carry_out) {
                code->SETcc(CC_C, MJitStateCFlag());
            }
        }
        return;
    case ShiftType::ROR: // Rotate right by immediate
        if (imm5 == 0) { //RRX
            code->BT(8, MJitStateCFlag(), Imm8(0));
            code->RCR(32, R(dest), Imm8(1));
            if (do_shifter_carry_out) {
                code->SETcc(CC_C, MJitStateCFlag());
            }
        } else {
            code->ROR(32, R(dest), Imm8(imm5));
            if (do_shifter_carry_out) {
                code->SETcc(CC_C, MJitStateCFlag());
            }
        }
        return;
    }

    UNREACHABLE();
}

X64Reg JitX64::CompileDataProcessingHelper_reg(ArmImm5 imm5, ShiftType shift, ArmReg Rm_index, bool do_shifter_carry_out) {
    X64Reg tmp = reg_alloc.AllocTemp();

    if (Rm_index != ArmReg::PC) {
        OpArg Rm = reg_alloc.LockArmForRead(Rm_index);
        code->MOV(32, R(tmp), Rm);
        reg_alloc.UnlockArm(Rm_index);
    } else {
        code->MOV(32, R(tmp), Imm32(GetReg15Value()));
    }

    if (do_shifter_carry_out) {
        cond_manager.FlagsDirty();
    }

    CompileShifter_imm(tmp, imm5, shift, do_shifter_carry_out);

    return tmp;
}

X64Reg JitX64::CompileDataProcessingHelper_rsr(ArmReg Rs_index, ShiftType shift, ArmReg Rm_index, bool do_shifter_carry_out) {
    // Caller must call reg_alloc.UnlockTemp on return value.
    // if do_shifter_carry_out,
    //    we output code that calculates and puts shifter_carry_out into MJitStateCFlag().

    reg_alloc.FlushX64(RCX);
    reg_alloc.LockX64(RCX);

    X64Reg tmp = reg_alloc.AllocTemp();

    if (Rs_index != ArmReg::PC) {
        OpArg Rs = reg_alloc.LockArmForRead(Rs_index);
        code->MOV(32, R(RCX), Rs);
        code->AND(32, R(RCX), Imm32(0xFF));
        reg_alloc.UnlockArm(Rs_index);
    } else {
        code->MOV(32, R(RCX), Imm32(GetReg15Value() & 0xFF));
    }

    if (Rm_index != ArmReg::PC) {
        OpArg Rm = reg_alloc.LockArmForRead(Rm_index);
        code->MOV(32, R(tmp), Rm);
        reg_alloc.UnlockArm(Rm_index);
    } else {
        code->MOV(32, R(tmp), Imm32(GetReg15Value()));
    }

    switch (shift) {
    case ShiftType::LSL: { // Logical shift left by register
        if (!do_shifter_carry_out) {
            code->SHL(32, R(tmp), R(CL));

            code->CMP(32, R(RCX), Imm8(32));
            X64Reg zero = reg_alloc.AllocTemp();
            code->MOV(32, R(zero), Imm32(0));
            code->CMOVcc(32, tmp, R(zero), CC_NB);
            reg_alloc.UnlockTemp(zero);
        } else {
            code->CMP(32, R(RCX), Imm8(32));
            auto Rs_gt32 = code->J_CC(CC_A);
            auto Rs_eq32 = code->J_CC(CC_E);
            // if (Rs & 0xFF == 0) goto end;
            code->TEST(32, R(RCX), R(RCX));
            auto Rs_zero = code->J_CC(CC_Z);
            // if (Rs & 0xFF < 32) {
            code->SHL(32, R(tmp), R(CL));
            code->SETcc(CC_C, MJitStateCFlag());
            auto jmp_to_end_1 = code->J();
            // } else if (Rs & 0xFF > 32) {
            code->SetJumpTarget(Rs_gt32);
            code->MOV(32, R(tmp), Imm32(0));
            code->MOV(8, MJitStateCFlag(), Imm8(0));
            auto jmp_to_end_2 = code->J();
            // } else if (Rs & 0xFF == 32) {
            code->SetJumpTarget(Rs_eq32);
            code->BT(32, R(tmp), Imm8(0));
            code->SETcc(CC_C, MJitStateCFlag());
            code->MOV(32, R(tmp), Imm32(0));
            // }
            code->SetJumpTarget(jmp_to_end_1);
            code->SetJumpTarget(jmp_to_end_2);
            code->SetJumpTarget(Rs_zero);
        }
        break;
    }
    case ShiftType::LSR: { // Logical shift right by register
        if (!do_shifter_carry_out) {
            code->SHR(32, R(tmp), R(RCX));

            code->CMP(32, R(RCX), Imm8(32));
            X64Reg zero = reg_alloc.AllocTemp();
            code->MOV(32, R(zero), Imm32(0));
            code->CMOVcc(32, tmp, R(zero), CC_NB);
            reg_alloc.UnlockTemp(zero);
        } else {
            code->CMP(32, R(RCX), Imm8(32));
            auto Rs_gt32 = code->J_CC(CC_A);
            auto Rs_eq32 = code->J_CC(CC_E);
            // if (Rs & 0xFF == 0) goto end;
            code->TEST(32, R(RCX), R(RCX));
            auto Rs_zero = code->J_CC(CC_Z);
            // if (Rs & 0xFF < 32) {
            code->SHR(32, R(tmp), R(RCX));
            code->SETcc(CC_C, MJitStateCFlag());
            auto jmp_to_end_1 = code->J();
            // } else if (Rs & 0xFF > 32) {
            code->SetJumpTarget(Rs_gt32);
            code->MOV(32, R(tmp), Imm32(0));
            code->MOV(8, MJitStateCFlag(), Imm8(0));
            auto jmp_to_end_2 = code->J();
            // } else if (Rs & 0xFF == 32) {
            code->SetJumpTarget(Rs_eq32);
            code->BT(32, R(tmp), Imm8(31));
            code->SETcc(CC_C, MJitStateCFlag());
            code->MOV(32, R(tmp), Imm32(0));
            // }
            code->SetJumpTarget(jmp_to_end_1);
            code->SetJumpTarget(jmp_to_end_2);
            code->SetJumpTarget(Rs_zero);
        }
        break;
    }
    case ShiftType::ASR: { // Arithmetic shift right by register
        if (!do_shifter_carry_out) {
            code->CMP(32, R(RCX), Imm8(31));
            auto Rs_gt31 = code->J_CC(CC_A);
            // if (Rs & 0xFF <= 31) {
            code->SAR(32, R(tmp), R(CL));
            auto jmp_to_end = code->J();
            // } else {
            code->SetJumpTarget(Rs_gt31);
            code->SAR(32, R(tmp), Imm8(31)); // Verified.
            // }
            code->SetJumpTarget(jmp_to_end);
        } else {
            code->CMP(32, R(RCX), Imm8(31));
            auto Rs_gt31 = code->J_CC(CC_A);
            // if (Rs & 0xFF == 0) goto end;
            code->TEST(32, R(RCX), R(RCX));
            auto Rs_zero = code->J_CC(CC_Z);
            // if (Rs & 0xFF <= 31) {
            code->SAR(32, R(tmp), R(CL));
            code->SETcc(CC_C, MJitStateCFlag());
            auto jmp_to_end = code->J();
            // } else if (Rs & 0xFF > 31) {
            code->SetJumpTarget(Rs_gt31);
            code->SAR(32, R(tmp), Imm8(31)); // Verified.
            code->BT(32, R(tmp), Imm8(31));
            code->SETcc(CC_C, MJitStateCFlag());
            // }
            code->SetJumpTarget(jmp_to_end);
            code->SetJumpTarget(Rs_zero);
        }
        break;
    }
    case ShiftType::ROR: { // Rotate right by register
        if (!do_shifter_carry_out) {
            code->AND(32, R(RCX), Imm32(0x1F));
            code->ROR(32, R(tmp), R(CL));
        } else {
            // if (Rs & 0xFF == 0) goto end;
            code->TEST(32, R(RCX), R(RCX));
            auto Rs_zero = code->J_CC(CC_Z);

            code->AND(32, R(RCX), Imm32(0x1F));
            auto zero_1F = code->J_CC(CC_Z);
            // if (Rs & 0x1F != 0) {
            code->ROR(32, R(tmp), R(CL));
            code->SETcc(CC_C, MJitStateCFlag());
            auto jmp_to_end = code->J();
            // } else {
            code->SetJumpTarget(zero_1F);
            code->BT(32, R(tmp), Imm8(31));
            code->SETcc(CC_C, MJitStateCFlag());
            // }
            code->SetJumpTarget(jmp_to_end);
            code->SetJumpTarget(Rs_zero);
        }
        break;
    }
    default:
        UNREACHABLE();
        break;
    } // switch (shift)

    reg_alloc.UnlockX64(RCX);

    return tmp;
}

void JitX64::ADC_imm(Cond cond, bool S, ArmReg Rn_index, ArmReg Rd_index, int rotate, ArmImm8 imm8) {
    cond_manager.CompileCond(cond);

    u32 immediate = rotr(imm8, rotate * 2);

    CompileDataProcessingHelper(Rn_index, Rd_index, [&](X64Reg Rd) {
        code->BT(32, MJitStateCFlag(), Imm8(0));
        code->ADC(32, R(Rd), Imm32(immediate));
    });

    if (S) {
        UpdateFlagsZVCN();
    }

    current.arm_pc += GetInstSize();
    if (Rd_index == ArmReg::PC) {
        CompileReturnToDispatch();
    }
}

void JitX64::ADC_reg(Cond cond, bool S, ArmReg Rn_index, ArmReg Rd_index, ArmImm5 imm5, ShiftType shift, ArmReg Rm_index) {
    cond_manager.CompileCond(cond);

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
    if (Rd_index == ArmReg::PC) {
        CompileReturnToDispatch();
    }
}

void JitX64::ADC_rsr(Cond cond, bool S, ArmReg Rn_index, ArmReg Rd_index, ArmReg Rs_index, ShiftType shift, ArmReg Rm_index) {
    cond_manager.CompileCond(cond);

    Gen::X64Reg tmp = CompileDataProcessingHelper_rsr(Rs_index, shift, Rm_index, false);

    CompileDataProcessingHelper(Rn_index, Rd_index, [&](X64Reg Rd) {
        code->BT(32, MJitStateCFlag(), Imm8(0));
        code->ADC(32, R(Rd), R(tmp));
    });

    reg_alloc.UnlockTemp(tmp);

    if (S) {
        UpdateFlagsZVCN();
    }

    current.arm_pc += GetInstSize();
    if (Rd_index == ArmReg::PC) {
        CompileReturnToDispatch();
    }
}

void JitX64::ADD_imm(Cond cond, bool S, ArmReg Rn_index, ArmReg Rd_index, int rotate, ArmImm8 imm8) {
    cond_manager.CompileCond(cond);

    u32 immediate = rotr(imm8, rotate * 2);

    CompileDataProcessingHelper(Rn_index, Rd_index, [&](X64Reg Rd) {
        code->ADD(32, R(Rd), Imm32(immediate));
    });

    if (S) {
        UpdateFlagsZVCN();
    }

    current.arm_pc += GetInstSize();
    if (Rd_index == ArmReg::PC) {
        CompileReturnToDispatch();
    }
}

void JitX64::ADD_reg(Cond cond, bool S, ArmReg Rn_index, ArmReg Rd_index, ArmImm5 imm5, ShiftType shift, ArmReg Rm_index) {
    cond_manager.CompileCond(cond);

    Gen::X64Reg tmp = CompileDataProcessingHelper_reg(imm5, shift, Rm_index, false);

    CompileDataProcessingHelper(Rn_index, Rd_index, [&](X64Reg Rd) {
        code->ADD(32, R(Rd), R(tmp));
    });

    reg_alloc.UnlockTemp(tmp);

    if (S) {
        UpdateFlagsZVCN();
    }

    current.arm_pc += GetInstSize();
    if (Rd_index == ArmReg::PC) {
        CompileReturnToDispatch();
    }
}

void JitX64::ADD_rsr(Cond cond, bool S, ArmReg Rn_index, ArmReg Rd_index, ArmReg Rs_index, ShiftType shift, ArmReg Rm_index) {
    cond_manager.CompileCond(cond);

    Gen::X64Reg tmp = CompileDataProcessingHelper_rsr(Rs_index, shift, Rm_index, false);

    CompileDataProcessingHelper(Rn_index, Rd_index, [&](X64Reg Rd) {
        code->ADD(32, R(Rd), R(tmp));
    });

    reg_alloc.UnlockTemp(tmp);

    if (S) {
        UpdateFlagsZVCN();
    }

    current.arm_pc += GetInstSize();
    if (Rd_index == ArmReg::PC) {
        CompileReturnToDispatch();
    }
}

void JitX64::AND_imm(Cond cond, bool S, ArmReg Rn_index, ArmReg Rd_index, int rotate, ArmImm8 imm8) {
    cond_manager.CompileCond(cond);

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
    if (Rd_index == ArmReg::PC) {
        CompileReturnToDispatch();
    }
}

void JitX64::AND_reg(Cond cond, bool S, ArmReg Rn_index, ArmReg Rd_index, ArmImm5 imm5, ShiftType shift, ArmReg Rm_index) {
    cond_manager.CompileCond(cond);

    Gen::X64Reg tmp = CompileDataProcessingHelper_reg(imm5, shift, Rm_index, S);

    CompileDataProcessingHelper(Rn_index, Rd_index, [&](X64Reg Rd) {
        code->AND(32, R(Rd), R(tmp));
    });

    reg_alloc.UnlockTemp(tmp);

    if (S) {
        UpdateFlagsZN();
        // C updated by CompileDataProcessingHelper_reg
    }

    current.arm_pc += GetInstSize();
    if (Rd_index == ArmReg::PC) {
        CompileReturnToDispatch();
    }
}

void JitX64::AND_rsr(Cond cond, bool S, ArmReg Rn_index, ArmReg Rd_index, ArmReg Rs_index, ShiftType shift, ArmReg Rm_index) {
    cond_manager.CompileCond(cond);

    Gen::X64Reg tmp = CompileDataProcessingHelper_rsr(Rs_index, shift, Rm_index, S);

    CompileDataProcessingHelper(Rn_index, Rd_index, [&](X64Reg Rd) {
        code->AND(32, R(Rd), R(tmp));
    });

    reg_alloc.UnlockTemp(tmp);

    if (S) {
        UpdateFlagsZN();
        // C updated by CompileDataProcessingHelper_reg
    }

    current.arm_pc += GetInstSize();
    if (Rd_index == ArmReg::PC) {
        CompileReturnToDispatch();
    }
}

void JitX64::BIC_imm(Cond cond, bool S, ArmReg Rn_index, ArmReg Rd_index, int rotate, ArmImm8 imm8) {
    cond_manager.CompileCond(cond);

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
    if (Rd_index == ArmReg::PC) {
        CompileReturnToDispatch();
    }
}

void JitX64::BIC_reg(Cond cond, bool S, ArmReg Rn_index, ArmReg Rd_index, ArmImm5 imm5, ShiftType shift, ArmReg Rm_index) {
    cond_manager.CompileCond(cond);

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
    if (Rd_index == ArmReg::PC) {
        CompileReturnToDispatch();
    }
}

void JitX64::BIC_rsr(Cond cond, bool S, ArmReg Rn_index, ArmReg Rd_index, ArmReg Rs_index, ShiftType shift, ArmReg Rm_index) {
    cond_manager.CompileCond(cond);

    Gen::X64Reg tmp = CompileDataProcessingHelper_rsr(Rs_index, shift, Rm_index, S);

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
    if (Rd_index == ArmReg::PC) {
        CompileReturnToDispatch();
    }
}

void JitX64::CMN_imm(Cond cond, ArmReg Rn_index, int rotate, ArmImm8 imm8) {
    cond_manager.CompileCond(cond);

    u32 immediate = rotr(imm8, rotate * 2);

    X64Reg tmp = reg_alloc.AllocTemp();
    if (Rn_index != ArmReg::PC) {
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
    cond_manager.CompileCond(cond);

    Gen::X64Reg tmp = CompileDataProcessingHelper_reg(imm5, shift, Rm_index, false);

    if (Rn_index != ArmReg::PC) {
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

void JitX64::CMN_rsr(Cond cond, ArmReg Rn_index, ArmReg Rs_index, ShiftType shift, ArmReg Rm_index) {
    cond_manager.CompileCond(cond);

    Gen::X64Reg tmp = CompileDataProcessingHelper_rsr(Rs_index, shift, Rm_index, false);

    if (Rn_index != ArmReg::PC) {
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
    cond_manager.CompileCond(cond);

    u32 immediate = rotr(imm8, rotate * 2);

    if (Rn_index != ArmReg::PC) {
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
    cond_manager.CompileCond(cond);

    Gen::X64Reg tmp = CompileDataProcessingHelper_reg(imm5, shift, Rm_index, false);

    if (Rn_index != ArmReg::PC) {
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

void JitX64::CMP_rsr(Cond cond, ArmReg Rn_index, ArmReg Rs_index, ShiftType shift, ArmReg Rm_index) {
    cond_manager.CompileCond(cond);

    Gen::X64Reg tmp = CompileDataProcessingHelper_rsr(Rs_index, shift, Rm_index, false);

    if (Rn_index != ArmReg::PC) {
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
    cond_manager.CompileCond(cond);

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
    if (Rd_index == ArmReg::PC) {
        CompileReturnToDispatch();
    }
}

void JitX64::EOR_reg(Cond cond, bool S, ArmReg Rn_index, ArmReg Rd_index, ArmImm5 imm5, ShiftType shift, ArmReg Rm_index) {
    cond_manager.CompileCond(cond);

    Gen::X64Reg tmp = CompileDataProcessingHelper_reg(imm5, shift, Rm_index, S);

    CompileDataProcessingHelper(Rn_index, Rd_index, [&](X64Reg Rd) {
        code->XOR(32, R(Rd), R(tmp));
    });

    reg_alloc.UnlockTemp(tmp);

    if (S) {
        UpdateFlagsZN();
    }

    current.arm_pc += GetInstSize();
    if (Rd_index == ArmReg::PC) {
        CompileReturnToDispatch();
    }
}

void JitX64::EOR_rsr(Cond cond, bool S, ArmReg Rn_index, ArmReg Rd_index, ArmReg Rs_index, ShiftType shift, ArmReg Rm_index) {
    cond_manager.CompileCond(cond);

    Gen::X64Reg tmp = CompileDataProcessingHelper_rsr(Rs_index, shift, Rm_index, S);

    CompileDataProcessingHelper(Rn_index, Rd_index, [&](X64Reg Rd) {
        code->XOR(32, R(Rd), R(tmp));
    });

    reg_alloc.UnlockTemp(tmp);

    if (S) {
        UpdateFlagsZN();
    }

    current.arm_pc += GetInstSize();
    if (Rd_index == ArmReg::PC) {
        CompileReturnToDispatch();
    }
}

void JitX64::MOV_imm(Cond cond, bool S, ArmReg Rd_index, int rotate, ArmImm8 imm8) {
    cond_manager.CompileCond(cond);

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
    if (Rd_index == ArmReg::PC) {
        CompileReturnToDispatch();
    }
}

void JitX64::MOV_reg(Cond cond, bool S, ArmReg Rd_index, ArmImm5 imm5, ShiftType shift, ArmReg Rm_index) {
    cond_manager.CompileCond(cond);

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
    if (Rd_index == ArmReg::PC) {
        CompileReturnToDispatch();
    }
}

void JitX64::MOV_rsr(Cond cond, bool S, ArmReg Rd_index, ArmReg Rs_index, ShiftType shift, ArmReg Rm_index) {
    cond_manager.CompileCond(cond);

    Gen::X64Reg tmp = CompileDataProcessingHelper_rsr(Rs_index, shift, Rm_index, S);

    Gen::OpArg Rd = reg_alloc.LockArmForWrite(Rd_index);
    code->MOV(32, Rd, R(tmp));
    reg_alloc.UnlockArm(Rd_index);

    if (S) {
        code->CMP(32, R(tmp), Imm32(0));
        UpdateFlagsZN();
    }

    reg_alloc.UnlockTemp(tmp);

    current.arm_pc += GetInstSize();
    if (Rd_index == ArmReg::PC) {
        CompileReturnToDispatch();
    }
}

void JitX64::MVN_imm(Cond cond, bool S, ArmReg Rd_index, int rotate, ArmImm8 imm8) {
    cond_manager.CompileCond(cond);

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
    if (Rd_index == ArmReg::PC) {
        CompileReturnToDispatch();
    }
}

void JitX64::MVN_reg(Cond cond, bool S, ArmReg Rd_index, ArmImm5 imm5, ShiftType shift, ArmReg Rm_index) {
    cond_manager.CompileCond(cond);

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
    if (Rd_index == ArmReg::PC) {
        CompileReturnToDispatch();
    }
}

void JitX64::MVN_rsr(Cond cond, bool S, ArmReg Rd_index, ArmReg Rs_index, ShiftType shift, ArmReg Rm_index) {
    cond_manager.CompileCond(cond);

    Gen::X64Reg tmp = CompileDataProcessingHelper_rsr(Rs_index, shift, Rm_index, S);
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
    if (Rd_index == ArmReg::PC) {
        CompileReturnToDispatch();
    }
}

void JitX64::ORR_imm(Cond cond, bool S, ArmReg Rn_index, ArmReg Rd_index, int rotate, ArmImm8 imm8) {
    cond_manager.CompileCond(cond);

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
    if (Rd_index == ArmReg::PC) {
        CompileReturnToDispatch();
    }
}

void JitX64::ORR_reg(Cond cond, bool S, ArmReg Rn_index, ArmReg Rd_index, ArmImm5 imm5, ShiftType shift, ArmReg Rm_index) {
    cond_manager.CompileCond(cond);

    Gen::X64Reg tmp = CompileDataProcessingHelper_reg(imm5, shift, Rm_index, S);

    CompileDataProcessingHelper(Rn_index, Rd_index, [&](X64Reg Rd) {
        code->OR(32, R(Rd), R(tmp));
    });

    reg_alloc.UnlockTemp(tmp);

    if (S) {
        UpdateFlagsZN();
    }

    current.arm_pc += GetInstSize();
    if (Rd_index == ArmReg::PC) {
        CompileReturnToDispatch();
    }
}

void JitX64::ORR_rsr(Cond cond, bool S, ArmReg Rn_index, ArmReg Rd_index, ArmReg Rs_index, ShiftType shift, ArmReg Rm_index) {
    cond_manager.CompileCond(cond);

    Gen::X64Reg tmp = CompileDataProcessingHelper_rsr(Rs_index, shift, Rm_index, S);

    CompileDataProcessingHelper(Rn_index, Rd_index, [&](X64Reg Rd) {
        code->OR(32, R(Rd), R(tmp));
    });

    reg_alloc.UnlockTemp(tmp);

    if (S) {
        UpdateFlagsZN();
    }

    current.arm_pc += GetInstSize();
    if (Rd_index == ArmReg::PC) {
        CompileReturnToDispatch();
    }
}

void JitX64::RSB_imm(Cond cond, bool S, ArmReg Rn_index, ArmReg Rd_index, int rotate, ArmImm8 imm8) {
    cond_manager.CompileCond(cond);

    u32 immediate = rotr(imm8, rotate * 2);

    CompileDataProcessingHelper_Reverse(Rn_index, Rd_index, [&](X64Reg Rd) {
        code->MOV(32, R(Rd), Imm32(immediate));

        if (Rn_index == ArmReg::PC) {
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
    if (Rd_index == ArmReg::PC) {
        CompileReturnToDispatch();
    }
}

void JitX64::RSB_reg(Cond cond, bool S, ArmReg Rn_index, ArmReg Rd_index, ArmImm5 imm5, ShiftType shift, ArmReg Rm_index) {
    cond_manager.CompileCond(cond);

    Gen::X64Reg tmp = CompileDataProcessingHelper_reg(imm5, shift, Rm_index, false);

    CompileDataProcessingHelper_Reverse(Rn_index, Rd_index, [&](X64Reg Rd) {
        code->MOV(32, R(Rd), R(tmp));

        if (Rn_index == ArmReg::PC) {
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
    if (Rd_index == ArmReg::PC) {
        CompileReturnToDispatch();
    }
}

void JitX64::RSB_rsr(Cond cond, bool S, ArmReg Rn_index, ArmReg Rd_index, ArmReg Rs_index, ShiftType shift, ArmReg Rm_index) {
    cond_manager.CompileCond(cond);

    Gen::X64Reg tmp = CompileDataProcessingHelper_rsr(Rs_index, shift, Rm_index, false);

    CompileDataProcessingHelper_Reverse(Rn_index, Rd_index, [&](X64Reg Rd) {
        code->MOV(32, R(Rd), R(tmp));

        if (Rn_index == ArmReg::PC) {
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
    if (Rd_index == ArmReg::PC) {
        CompileReturnToDispatch();
    }
}

void JitX64::RSC_imm(Cond cond, bool S, ArmReg Rn_index, ArmReg Rd_index, int rotate, ArmImm8 imm8) {
    cond_manager.CompileCond(cond);

    u32 immediate = rotr(imm8, rotate * 2);

    CompileDataProcessingHelper_Reverse(Rn_index, Rd_index, [&](X64Reg Rd) {
        code->MOV(32, R(Rd), Imm32(immediate));

        code->BT(32, MJitStateCFlag(), Imm8(0));
        code->CMC();

        if (Rn_index == ArmReg::PC) {
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
    if (Rd_index == ArmReg::PC) {
        CompileReturnToDispatch();
    }
}

void JitX64::RSC_reg(Cond cond, bool S, ArmReg Rn_index, ArmReg Rd_index, ArmImm5 imm5, ShiftType shift, ArmReg Rm_index) {
    cond_manager.CompileCond(cond);

    Gen::X64Reg tmp = CompileDataProcessingHelper_reg(imm5, shift, Rm_index, false);

    CompileDataProcessingHelper_Reverse(Rn_index, Rd_index, [&](X64Reg Rd) {
        code->MOV(32, R(Rd), R(tmp));

        code->BT(32, MJitStateCFlag(), Imm8(0));
        code->CMC();

        if (Rn_index == ArmReg::PC) {
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
    if (Rd_index == ArmReg::PC) {
        CompileReturnToDispatch();
    }
}

void JitX64::RSC_rsr(Cond cond, bool S, ArmReg Rn_index, ArmReg Rd_index, ArmReg Rs_index, ShiftType shift, ArmReg Rm_index) {
    cond_manager.CompileCond(cond);

    Gen::X64Reg tmp = CompileDataProcessingHelper_rsr(Rs_index, shift, Rm_index, false);

    CompileDataProcessingHelper_Reverse(Rn_index, Rd_index, [&](X64Reg Rd) {
        code->MOV(32, R(Rd), R(tmp));

        code->BT(32, MJitStateCFlag(), Imm8(0));
        code->CMC();

        if (Rn_index == ArmReg::PC) {
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
    if (Rd_index == ArmReg::PC) {
        CompileReturnToDispatch();
    }
}

void JitX64::SBC_imm(Cond cond, bool S, ArmReg Rn_index, ArmReg Rd_index, int rotate, ArmImm8 imm8) {
    cond_manager.CompileCond(cond);

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
    if (Rd_index == ArmReg::PC) {
        CompileReturnToDispatch();
    }
}

void JitX64::SBC_reg(Cond cond, bool S, ArmReg Rn_index, ArmReg Rd_index, ArmImm5 imm5, ShiftType shift, ArmReg Rm_index) {
    cond_manager.CompileCond(cond);

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
    if (Rd_index == ArmReg::PC) {
        CompileReturnToDispatch();
    }
}

void JitX64::SBC_rsr(Cond cond, bool S, ArmReg Rn_index, ArmReg Rd_index, ArmReg Rs_index, ShiftType shift, ArmReg Rm_index) {
    cond_manager.CompileCond(cond);

    Gen::X64Reg tmp = CompileDataProcessingHelper_rsr(Rs_index, shift, Rm_index, false);

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
    if (Rd_index == ArmReg::PC) {
        CompileReturnToDispatch();
    }
}

void JitX64::SUB_imm(Cond cond, bool S, ArmReg Rn_index, ArmReg Rd_index, int rotate, ArmImm8 imm8) {
    cond_manager.CompileCond(cond);

    u32 immediate = rotr(imm8, rotate * 2);

    CompileDataProcessingHelper(Rn_index, Rd_index, [&](X64Reg Rd) {
        code->SUB(32, R(Rd), Imm32(immediate));
    });

    if (S) {
        UpdateFlagsZVN();
        UpdateFlagsC_complement();
    }

    current.arm_pc += GetInstSize();
    if (Rd_index == ArmReg::PC) {
        CompileReturnToDispatch();
    }
}

void JitX64::SUB_reg(Cond cond, bool S, ArmReg Rn_index, ArmReg Rd_index, ArmImm5 imm5, ShiftType shift, ArmReg Rm_index) {
    cond_manager.CompileCond(cond);

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
    if (Rd_index == ArmReg::PC) {
        CompileReturnToDispatch();
    }
}

void JitX64::SUB_rsr(Cond cond, bool S, ArmReg Rn_index, ArmReg Rd_index, ArmReg Rs_index, ShiftType shift, ArmReg Rm_index) {
    cond_manager.CompileCond(cond);

    Gen::X64Reg tmp = CompileDataProcessingHelper_rsr(Rs_index, shift, Rm_index, false);

    CompileDataProcessingHelper(Rn_index, Rd_index, [&](X64Reg Rd) {
        code->SUB(32, R(Rd), R(tmp));
    });

    reg_alloc.UnlockTemp(tmp);

    if (S) {
        UpdateFlagsZVN();
        UpdateFlagsC_complement();
    }

    current.arm_pc += GetInstSize();
    if (Rd_index == ArmReg::PC) {
        CompileReturnToDispatch();
    }
}


void JitX64::TEQ_imm(Cond cond, ArmReg Rn_index, int rotate, ArmImm8 imm8) {
    cond_manager.CompileCond(cond);

    u32 immediate = rotr(imm8, rotate * 2);

    X64Reg Rn_tmp = reg_alloc.AllocTemp();

    if (Rn_index == ArmReg::PC) {
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
    cond_manager.CompileCond(cond);

    Gen::X64Reg tmp = CompileDataProcessingHelper_reg(imm5, shift, Rm_index, true);

    if (Rn_index == ArmReg::PC) {
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

void JitX64::TEQ_rsr(Cond cond, ArmReg Rn_index, ArmReg Rs_index, ShiftType shift, ArmReg Rm_index) {
    cond_manager.CompileCond(cond);

    Gen::X64Reg tmp = CompileDataProcessingHelper_rsr(Rs_index, shift, Rm_index, true);

    if (Rn_index == ArmReg::PC) {
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
    cond_manager.CompileCond(cond);

    u32 immediate = rotr(imm8, rotate * 2);

    X64Reg Rn;

    if (Rn_index == ArmReg::PC) {
        Rn = reg_alloc.AllocTemp();
        code->MOV(32, R(Rn), Imm32(GetReg15Value()));
    } else {
        Rn = reg_alloc.BindArmForRead(Rn_index);
    }

    code->TEST(32, R(Rn), Imm32(immediate));

    if (Rn_index == ArmReg::PC) {
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
    cond_manager.CompileCond(cond);

    Gen::X64Reg tmp = CompileDataProcessingHelper_reg(imm5, shift, Rm_index, true);

    if (Rn_index == ArmReg::PC) {
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

void JitX64::TST_rsr(Cond cond, ArmReg Rn_index, ArmReg Rs_index, ShiftType shift, ArmReg Rm_index) {
    cond_manager.CompileCond(cond);

    Gen::X64Reg tmp = CompileDataProcessingHelper_rsr(Rs_index, shift, Rm_index, true);

    if (Rn_index == ArmReg::PC) {
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

} // namespace JitX64
