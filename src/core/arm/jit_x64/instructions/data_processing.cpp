// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/common_funcs.h"

#include "core/arm/jit_x64/jit_x64.h"

namespace JitX64 {

using namespace Gen;

void JitX64::ADC_imm(Cond cond, bool S, ArmReg Rn_index, ArmReg Rd_index, int rotate, ArmImm8 imm8) {
    cond_manager.CompileCond((ConditionCode)cond);

    u32 immediate = _rotr(imm8, rotate * 2);

    if (Rd_index == 15) {
        X64Reg Rd = reg_alloc.AllocAndLockTemp();
        reg_alloc.LockArm(Rn_index);

        if (Rn_index == 15) {
            code->MOV(32, R(Rd), Imm32(GetReg15Value()));
        } else {
            code->MOV(32, R(Rd), reg_alloc.ArmR(Rd_index));
        }
        code->BT(32, MJitStateCFlag(), Imm8(0));
        code->ADC(32, R(Rd), Imm32(immediate));

        reg_alloc.UnlockArm(Rn_index);

        if (S)
            UNIMPLEMENTED();

        code->MOV(32, MJitStateArmPC(), R(Rd));
        reg_alloc.UnlockTemp(Rd);
        current.arm_pc += GetInstSize();
        CompileReturnToDispatch();
        return;
    } else if (Rn_index == 15) {
        X64Reg Rd = reg_alloc.BindNoLoadAndLockArm(Rd_index);
        reg_alloc.MarkDirty(Rd_index);

        code->MOV(32, R(Rd), Imm32(GetReg15Value()));
        code->BT(32, MJitStateCFlag(), Imm8(0));
        code->ADC(32, R(Rd), Imm32(immediate));

        reg_alloc.UnlockArm(Rd_index);
    } else if (Rn_index == Rd_index) {
        X64Reg Rd = reg_alloc.BindAndLockArm(Rd_index);
        reg_alloc.MarkDirty(Rd_index);

        code->BT(32, MJitStateCFlag(), Imm8(0));
        code->ADC(32, R(Rd), Imm32(immediate));

        reg_alloc.UnlockArm(Rd_index);
    } else {
        X64Reg Rd = reg_alloc.BindNoLoadAndLockArm(Rd_index);
        reg_alloc.MarkDirty(Rd_index);
        reg_alloc.LockArm(Rn_index);

        code->MOV(32, R(Rd), reg_alloc.ArmR(Rn_index));
        code->BT(32, MJitStateCFlag(), Imm8(0));
        code->ADC(32, R(Rd), Imm32(immediate));

        reg_alloc.UnlockArm(Rd_index);
        reg_alloc.UnlockArm(Rn_index);
    }

    if (S) {
        cond_manager.FlagsDirty();
        UpdateFlagsZVCN();
    }

    current.arm_pc += GetInstSize();
}

void JitX64::ADC_reg(Cond cond, bool S, ArmReg Rn, ArmReg Rd, ArmImm5 imm5, ShiftType shift, ArmReg Rm) { CompileInterpretInstruction(); }
void JitX64::ADC_rsr(Cond cond, bool S, ArmReg Rn, ArmReg Rd, ArmReg Rs, ShiftType shift, ArmReg Rm) { CompileInterpretInstruction(); }
void JitX64::ADD_imm(Cond cond, bool S, ArmReg Rn, ArmReg Rd, int rotate, ArmImm8 imm8) { CompileInterpretInstruction(); }
void JitX64::ADD_reg(Cond cond, bool S, ArmReg Rn, ArmReg Rd, ArmImm5 imm5, ShiftType shift, ArmReg Rm) { CompileInterpretInstruction(); }
void JitX64::ADD_rsr(Cond cond, bool S, ArmReg Rn, ArmReg Rd, ArmReg Rs, ShiftType shift, ArmReg Rm) { CompileInterpretInstruction(); }
void JitX64::AND_imm(Cond cond, bool S, ArmReg Rn, ArmReg Rd, int rotate, ArmImm8 imm8) { CompileInterpretInstruction(); }
void JitX64::AND_reg(Cond cond, bool S, ArmReg Rn, ArmReg Rd, ArmImm5 imm5, ShiftType shift, ArmReg Rm) { CompileInterpretInstruction(); }
void JitX64::AND_rsr(Cond cond, bool S, ArmReg Rn, ArmReg Rd, ArmReg Rs, ShiftType shift, ArmReg Rm) { CompileInterpretInstruction(); }
void JitX64::BIC_imm(Cond cond, bool S, ArmReg Rn, ArmReg Rd, int rotate, ArmImm8 imm8) { CompileInterpretInstruction(); }
void JitX64::BIC_reg(Cond cond, bool S, ArmReg Rn, ArmReg Rd, ArmImm5 imm5, ShiftType shift, ArmReg Rm) { CompileInterpretInstruction(); }
void JitX64::BIC_rsr(Cond cond, bool S, ArmReg Rn, ArmReg Rd, ArmReg Rs, ShiftType shift, ArmReg Rm) { CompileInterpretInstruction(); }
void JitX64::CMN_imm(Cond cond, ArmReg Rn, int rotate, ArmImm8 imm8) { CompileInterpretInstruction(); }
void JitX64::CMN_reg(Cond cond, ArmReg Rn, ArmImm5 imm5, ShiftType shift, ArmReg Rm) { CompileInterpretInstruction(); }
void JitX64::CMN_rsr(Cond cond, ArmReg Rn, ArmReg Rs, ShiftType shift, ArmReg Rm) { CompileInterpretInstruction(); }
void JitX64::CMP_imm(Cond cond, ArmReg Rn, int rotate, ArmImm8 imm8) { CompileInterpretInstruction(); }
void JitX64::CMP_reg(Cond cond, ArmReg Rn, ArmImm5 imm5, ShiftType shift, ArmReg Rm) { CompileInterpretInstruction(); }
void JitX64::CMP_rsr(Cond cond, ArmReg Rn, ArmReg Rs, ShiftType shift, ArmReg Rm) { CompileInterpretInstruction(); }
void JitX64::EOR_imm(Cond cond, bool S, ArmReg Rn, ArmReg Rd, int rotate, ArmImm8 imm8) { CompileInterpretInstruction(); }
void JitX64::EOR_reg(Cond cond, bool S, ArmReg Rn, ArmReg Rd, ArmImm5 imm5, ShiftType shift, ArmReg Rm) { CompileInterpretInstruction(); }
void JitX64::EOR_rsr(Cond cond, bool S, ArmReg Rn, ArmReg Rd, ArmReg Rs, ShiftType shift, ArmReg Rm) { CompileInterpretInstruction(); }
void JitX64::MOV_imm(Cond cond, bool S, ArmReg Rd, int rotate, ArmImm8 imm8) { CompileInterpretInstruction(); }
void JitX64::MOV_reg(Cond cond, bool S, ArmReg Rd, ArmImm5 imm5, ShiftType shift, ArmReg Rm) { CompileInterpretInstruction(); }
void JitX64::MOV_rsr(Cond cond, bool S, ArmReg Rd, ArmReg Rs, ShiftType shift, ArmReg Rm) { CompileInterpretInstruction(); }
void JitX64::MVN_imm(Cond cond, bool S, ArmReg Rd, int rotate, ArmImm8 imm8) { CompileInterpretInstruction(); }
void JitX64::MVN_reg(Cond cond, bool S, ArmReg Rd, ArmImm5 imm5, ShiftType shift, ArmReg Rm) { CompileInterpretInstruction(); }
void JitX64::MVN_rsr(Cond cond, bool S, ArmReg Rd, ArmReg Rs, ShiftType shift, ArmReg Rm) { CompileInterpretInstruction(); }
void JitX64::ORR_imm(Cond cond, bool S, ArmReg Rn, ArmReg Rd, int rotate, ArmImm8 imm8) { CompileInterpretInstruction(); }
void JitX64::ORR_reg(Cond cond, bool S, ArmReg Rn, ArmReg Rd, ArmImm5 imm5, ShiftType shift, ArmReg Rm) { CompileInterpretInstruction(); }
void JitX64::ORR_rsr(Cond cond, bool S, ArmReg Rn, ArmReg Rd, ArmReg Rs, ShiftType shift, ArmReg Rm) { CompileInterpretInstruction(); }
void JitX64::RSB_imm(Cond cond, bool S, ArmReg Rn, ArmReg Rd, int rotate, ArmImm8 imm8) { CompileInterpretInstruction(); }
void JitX64::RSB_reg(Cond cond, bool S, ArmReg Rn, ArmReg Rd, ArmImm5 imm5, ShiftType shift, ArmReg Rm) { CompileInterpretInstruction(); }
void JitX64::RSB_rsr(Cond cond, bool S, ArmReg Rn, ArmReg Rd, ArmReg Rs, ShiftType shift, ArmReg Rm) { CompileInterpretInstruction(); }
void JitX64::RSC_imm(Cond cond, bool S, ArmReg Rn, ArmReg Rd, int rotate, ArmImm8 imm8) { CompileInterpretInstruction(); }
void JitX64::RSC_reg(Cond cond, bool S, ArmReg Rn, ArmReg Rd, ArmImm5 imm5, ShiftType shift, ArmReg Rm) { CompileInterpretInstruction(); }
void JitX64::RSC_rsr(Cond cond, bool S, ArmReg Rn, ArmReg Rd, ArmReg Rs, ShiftType shift, ArmReg Rm) { CompileInterpretInstruction(); }
void JitX64::SBC_imm(Cond cond, bool S, ArmReg Rn, ArmReg Rd, int rotate, ArmImm8 imm8) { CompileInterpretInstruction(); }
void JitX64::SBC_reg(Cond cond, bool S, ArmReg Rn, ArmReg Rd, ArmImm5 imm5, ShiftType shift, ArmReg Rm) { CompileInterpretInstruction(); }
void JitX64::SBC_rsr(Cond cond, bool S, ArmReg Rn, ArmReg Rd, ArmReg Rs, ShiftType shift, ArmReg Rm) { CompileInterpretInstruction(); }
void JitX64::SUB_imm(Cond cond, bool S, ArmReg Rn, ArmReg Rd, int rotate, ArmImm8 imm8) { CompileInterpretInstruction(); }
void JitX64::SUB_reg(Cond cond, bool S, ArmReg Rn, ArmReg Rd, ArmImm5 imm5, ShiftType shift, ArmReg Rm) { CompileInterpretInstruction(); }
void JitX64::SUB_rsr(Cond cond, bool S, ArmReg Rn, ArmReg Rd, ArmReg Rs, ShiftType shift, ArmReg Rm) { CompileInterpretInstruction(); }
void JitX64::TEQ_imm(Cond cond, ArmReg Rn, int rotate, ArmImm8 imm8) { CompileInterpretInstruction(); }
void JitX64::TEQ_reg(Cond cond, ArmReg Rn, ArmImm5 imm5, ShiftType shift, ArmReg Rm) { CompileInterpretInstruction(); }
void JitX64::TEQ_rsr(Cond cond, ArmReg Rn, ArmReg Rs, ShiftType shift, ArmReg Rm) { CompileInterpretInstruction(); }
void JitX64::TST_imm(Cond cond, ArmReg Rn, int rotate, ArmImm8 imm8) { CompileInterpretInstruction(); }
void JitX64::TST_reg(Cond cond, ArmReg Rn, ArmImm5 imm5, ShiftType shift, ArmReg Rm) { CompileInterpretInstruction(); }
void JitX64::TST_rsr(Cond cond, ArmReg Rn, ArmReg Rs, ShiftType shift, ArmReg Rm) { CompileInterpretInstruction(); }

}
