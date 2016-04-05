// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <functional>

#include "common/assert.h"
#include "common/x64/abi.h"

#include "core/arm/jit_x64/jit_x64.h"
#include "core/arm/jit_x64/instructions/helper/load_store.h"

namespace JitX64 {

using namespace Gen;

void JitX64::CLREX() {
    cond_manager.Always();

    code->MOV(32, MJitStateExclusiveTag(), Imm32(0xFFFFFFFF));
    code->MOV(8, MJitStateExclusiveState(), Imm8(0));

    current.arm_pc += GetInstSize();
}

void ExclusiveLoadCommon(XEmitter* code, RegAlloc& reg_alloc, OpArg exclusive_state, OpArg exclusive_tag, ArmReg Rn_index, ArmReg Rd_index) {
    ASSERT_MSG(Rn_index != ArmReg::PC && Rd_index != ArmReg::PC, "UNPREDICTABLE");

    code->MOV(8, exclusive_state, Imm8(1));

    reg_alloc.FlushX64(ABI_PARAM1);
    reg_alloc.LockX64(ABI_PARAM1);

    OpArg Rn = reg_alloc.LockArmForRead(Rn_index);
    code->MOV(32, R(ABI_PARAM1), Rn);
    reg_alloc.UnlockArm(Rn_index);

    code->MOV(32, exclusive_tag, R(ABI_PARAM1));
    code->AND(32, exclusive_tag, Imm32(RESERVATION_GRANULE_MASK));

    reg_alloc.UnlockX64(ABI_PARAM1);
}

void JitX64::LDREX(Cond cond, ArmReg Rn_index, ArmReg Rd_index) {
    cond_manager.CompileCond(cond);

    ASSERT_MSG(Rn_index != ArmReg::PC && Rd_index != ArmReg::PC, "UNPREDICTABLE");

    ExclusiveLoadCommon(code, reg_alloc, MJitStateExclusiveState(), MJitStateExclusiveTag(), Rn_index, Rd_index);
    CompileCallHost(reinterpret_cast<const void* const>(!current.EFlag ? &Load32LE : &Load32BE));

    reg_alloc.LockX64(ABI_RETURN);

    X64Reg Rd = reg_alloc.BindArmForWrite(Rd_index);
    code->MOV(32, R(Rd), R(ABI_RETURN));
    reg_alloc.UnlockArm(Rd_index);

    reg_alloc.UnlockX64(ABI_RETURN);

    current.arm_pc += GetInstSize();
}

void JitX64::LDREXB(Cond cond, ArmReg Rn_index, ArmReg Rd_index) {
    cond_manager.CompileCond(cond);

    ASSERT_MSG(Rn_index != ArmReg::PC && Rd_index != ArmReg::PC, "UNPREDICTABLE");

    ExclusiveLoadCommon(code, reg_alloc, MJitStateExclusiveState(), MJitStateExclusiveTag(), Rn_index, Rd_index);
    CompileCallHost(reinterpret_cast<const void* const>(&Load8));

    reg_alloc.LockX64(ABI_RETURN);

    X64Reg Rd = reg_alloc.BindArmForWrite(Rd_index);
    code->MOVZX(32, 8, Rd, R(ABI_RETURN));
    reg_alloc.UnlockArm(Rd_index);

    reg_alloc.UnlockX64(ABI_RETURN);

    current.arm_pc += GetInstSize();
}

void JitX64::LDREXD(Cond cond, ArmReg Rn_index, ArmReg Rd_index) {
    cond_manager.CompileCond(cond);

    ASSERT_MSG(IsEvenArmReg(Rd_index), "UNPREDICTABLE");
    ASSERT_MSG(Rd_index < ArmReg::R14, "UNPREDICTABLE");
    ASSERT_MSG(Rn_index != ArmReg::PC, "UNPREDICTABLE");

    ExclusiveLoadCommon(code, reg_alloc, MJitStateExclusiveState(), MJitStateExclusiveTag(), Rn_index, Rd_index);
    CompileCallHost(reinterpret_cast<const void* const>(!current.EFlag ? Load64LE : Load64BE));

    reg_alloc.LockX64(ABI_RETURN);

    X64Reg Rd0 = reg_alloc.BindArmForWrite(Rd_index + 0);
    X64Reg Rd1 = reg_alloc.BindArmForWrite(Rd_index + 1);
    code->MOV(64, R(Rd0), R(ABI_RETURN));
    code->SHR(64, R(ABI_RETURN), Imm8(32));
    code->MOV(32, R(Rd1), R(ABI_RETURN));
    reg_alloc.UnlockArm(Rd_index + 0);
    reg_alloc.UnlockArm(Rd_index + 1);

    reg_alloc.UnlockX64(ABI_RETURN);

    current.arm_pc += GetInstSize();
}

void JitX64::LDREXH(Cond cond, ArmReg Rn_index, ArmReg Rd_index) {
    cond_manager.CompileCond(cond);

    ASSERT_MSG(Rn_index != ArmReg::PC && Rd_index != ArmReg::PC, "UNPREDICTABLE");

    ExclusiveLoadCommon(code, reg_alloc, MJitStateExclusiveState(), MJitStateExclusiveTag(), Rn_index, Rd_index);
    CompileCallHost(reinterpret_cast<const void* const>(!current.EFlag ? Load16LE : Load16BE));

    reg_alloc.LockX64(ABI_RETURN);

    X64Reg Rd = reg_alloc.BindArmForWrite(Rd_index);
    code->MOVZX(32, 16, Rd, R(ABI_RETURN));
    reg_alloc.UnlockArm(Rd_index);

    reg_alloc.UnlockX64(ABI_RETURN);

    current.arm_pc += GetInstSize();
}

void ExclusiveStoreCommon(XEmitter* code, RegAlloc& reg_alloc, OpArg exclusive_state, OpArg exclusive_tag, ArmReg Rn_index, ArmReg Rd_index, std::function<void()> do_memory_access) {
    OpArg Rn = reg_alloc.LockArmForRead(Rn_index);
    OpArg Rd = reg_alloc.LockArmForWrite(Rd_index);

    code->MOV(32, Rd, Imm8(1)); // First, assume we failed.

    code->MOV(32, R(ABI_PARAM1), Rn);

    code->BT(8, exclusive_state, Imm8(0));
    auto jmp_to_end1 = code->J_CC(CC_NC);

    code->MOV(32, R(ABI_PARAM2), R(ABI_PARAM1));
    code->AND(32, R(ABI_PARAM2), Imm32(RESERVATION_GRANULE_MASK));
    code->CMP(32, R(ABI_PARAM2), exclusive_tag);
    auto jmp_to_end2 = code->J_CC(CC_NE);

    // Exclsuive monitor pass
    code->MOV(32, Rd, Imm8(0)); // Okay, actually we passed.
    code->MOV(8, exclusive_state, Imm8(0)); // Unset exclusive memory acecss.
    code->MOV(32, exclusive_tag, Imm32(0xFFFFFFFF)); // Unset exclusive memory acecss.

    do_memory_access();

    code->SetJumpTarget(jmp_to_end1);
    code->SetJumpTarget(jmp_to_end2);

    reg_alloc.UnlockArm(Rd_index);
    reg_alloc.UnlockArm(Rn_index);
}

void JitX64::STREX(Cond cond, ArmReg Rn_index, ArmReg Rd_index, ArmReg Rm_index) {
    CompileInterpretInstruction();
    return;

    cond_manager.CompileCond(cond);

    ASSERT_MSG(Rn_index != ArmReg::PC && Rd_index != ArmReg::PC && Rm_index != ArmReg::PC, "UNPREDICTABLE");
    ASSERT_MSG(Rd_index != Rn_index && Rd_index != Rm_index, "UNPREDICTABLE");

    reg_alloc.FlushX64(ABI_PARAM1);
    reg_alloc.LockX64(ABI_PARAM1);
    reg_alloc.FlushX64(ABI_PARAM2);
    reg_alloc.LockX64(ABI_PARAM2);

    OpArg Rm = reg_alloc.LockArmForRead(Rm_index);

    ExclusiveStoreCommon(code, reg_alloc, MJitStateExclusiveState(), MJitStateExclusiveTag(),
        Rn_index, Rd_index,
        [&](){
            code->MOV(32, R(ABI_PARAM2), Rm);
            CompileCallHost(reinterpret_cast<const void* const>(!current.EFlag ? &Store32LE : &Store32BE));
        });

    reg_alloc.UnlockArm(Rm_index);

    reg_alloc.UnlockX64(ABI_PARAM2);
    reg_alloc.UnlockX64(ABI_PARAM1);

    current.arm_pc += GetInstSize();
}

void JitX64::STREXB(Cond cond, ArmReg Rn_index, ArmReg Rd_index, ArmReg Rm_index) {
    CompileInterpretInstruction();
    return;

    cond_manager.CompileCond(cond);

    ASSERT_MSG(Rn_index != ArmReg::PC && Rd_index != ArmReg::PC && Rm_index != ArmReg::PC, "UNPREDICTABLE");
    ASSERT_MSG(Rd_index != Rn_index && Rd_index != Rm_index, "UNPREDICTABLE");

    reg_alloc.FlushX64(ABI_PARAM1);
    reg_alloc.LockX64(ABI_PARAM1);
    reg_alloc.FlushX64(ABI_PARAM2);
    reg_alloc.LockX64(ABI_PARAM2);

    OpArg Rm = reg_alloc.LockArmForRead(Rm_index);

    ExclusiveStoreCommon(code, reg_alloc, MJitStateExclusiveState(), MJitStateExclusiveTag(),
        Rn_index, Rd_index,
        [&]() {
        code->MOV(32, R(ABI_PARAM2), Rm);
        CompileCallHost(reinterpret_cast<const void* const>(&Store8));
    });

    reg_alloc.UnlockArm(Rm_index);

    reg_alloc.UnlockX64(ABI_PARAM2);
    reg_alloc.UnlockX64(ABI_PARAM1);

    current.arm_pc += GetInstSize();
}

void JitX64::STREXD(Cond cond, ArmReg Rn_index, ArmReg Rd_index, ArmReg Rm_index) {
    CompileInterpretInstruction();
    return;

    cond_manager.CompileCond(cond);

    ASSERT_MSG(Rn_index != ArmReg::PC && Rd_index != ArmReg::PC && Rm_index != ArmReg::PC, "UNPREDICTABLE");
    ASSERT_MSG(Rm_index != ArmReg::R14, "UNPREDICTABLE");
    ASSERT_MSG(Rd_index != Rn_index && Rd_index != Rm_index, "UNPREDICTABLE");
    ASSERT_MSG(Rd_index != Rm_index + 1, "UNPREDICTABLE");
    ASSERT_MSG(IsEvenArmReg(Rm_index), "UNPREDICTABLE");

    reg_alloc.FlushX64(ABI_PARAM1);
    reg_alloc.LockX64(ABI_PARAM1);
    reg_alloc.FlushX64(ABI_PARAM2);
    reg_alloc.LockX64(ABI_PARAM2);
    reg_alloc.FlushX64(ABI_PARAM3);
    reg_alloc.LockX64(ABI_PARAM3);

    OpArg Rm0 = reg_alloc.LockArmForRead(Rm_index + 0);
    OpArg Rm1 = reg_alloc.LockArmForRead(Rm_index + 1);

    ExclusiveStoreCommon(code, reg_alloc, MJitStateExclusiveState(), MJitStateExclusiveTag(),
        Rn_index, Rd_index,
        [&]() {
        code->MOV(32, R(ABI_PARAM2), Rm0);
        code->MOV(32, R(ABI_PARAM3), Rm1);
        CompileCallHost(reinterpret_cast<const void* const>(!current.EFlag ? &Store64LE : &Store64BE));
    });

    reg_alloc.UnlockArm(Rm_index + 1);
    reg_alloc.UnlockArm(Rm_index + 0);

    reg_alloc.UnlockX64(ABI_PARAM3);
    reg_alloc.UnlockX64(ABI_PARAM2);
    reg_alloc.UnlockX64(ABI_PARAM1);

    current.arm_pc += GetInstSize();
}

void JitX64::STREXH(Cond cond, ArmReg Rn_index, ArmReg Rd_index, ArmReg Rm_index) {
    CompileInterpretInstruction();
    return;

    cond_manager.CompileCond(cond);

    ASSERT_MSG(Rn_index != ArmReg::PC && Rd_index != ArmReg::PC && Rm_index != ArmReg::PC, "UNPREDICTABLE");
    ASSERT_MSG(Rd_index != Rn_index && Rd_index != Rm_index, "UNPREDICTABLE");

    reg_alloc.FlushX64(ABI_PARAM1);
    reg_alloc.LockX64(ABI_PARAM1);
    reg_alloc.FlushX64(ABI_PARAM2);
    reg_alloc.LockX64(ABI_PARAM2);

    OpArg Rm = reg_alloc.LockArmForRead(Rm_index);

    ExclusiveStoreCommon(code, reg_alloc, MJitStateExclusiveState(), MJitStateExclusiveTag(),
        Rn_index, Rd_index,
        [&]() {
        code->MOV(32, R(ABI_PARAM2), Rm);
        CompileCallHost(reinterpret_cast<const void* const>(!current.EFlag ? &Store16LE : &Store16BE));
    });

    reg_alloc.UnlockArm(Rm_index);

    reg_alloc.UnlockX64(ABI_PARAM2);
    reg_alloc.UnlockX64(ABI_PARAM1);

    current.arm_pc += GetInstSize();
}

void JitX64::SWP(Cond cond, ArmReg Rn_index, ArmReg Rd_index, ArmReg Rm_index) {
    CompileInterpretInstruction();
}

void JitX64::SWPB(Cond cond, ArmReg Rn_index, ArmReg Rd_index, ArmReg Rm_index) {
    CompileInterpretInstruction();
}

} // namespace JitX64
