// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/bit_set.h"
#include "common/swap.h"
#include "common/x64/abi.h"

#include "core/arm/jit_x64/jit_x64.h"
#include "core/memory.h"

namespace JitX64 {

// TODO: Loads from constant memory regions can be turned into immediate constant loads.

using namespace Gen;

/// This function assumes that the value of Rn is already in dest except for R15.
void JitX64::LoadAndStoreWordOrUnsignedByte_Immediate_Helper(X64Reg dest, bool U, ArmReg Rn_index, ArmImm12 imm12) {
    // address = Rn +/- imm12

    if (Rn_index == 15) {
        u32 address;
        if (U) {
            address = GetReg15Value_WordAligned() + imm12;
        } else {
            address = GetReg15Value_WordAligned() - imm12;
        }
        code->MOV(32, R(dest), Imm32(address));
    } else {
        if (U) {
            code->ADD(32, R(dest), Imm32(imm12));
        } else {
            code->SUB(32, R(dest), Imm32(imm12));
        }
    }
}

void JitX64::LoadAndStoreWordOrUnsignedByte_ImmediateOffset(X64Reg dest, bool U, ArmReg Rn_index, ArmImm12 imm12) {
    if (Rn_index != 15) {
        OpArg Rn = reg_alloc.LockArmForRead(Rn_index);
        code->MOV(32, R(dest), Rn);
        reg_alloc.UnlockArm(Rn_index);
    }

    LoadAndStoreWordOrUnsignedByte_Immediate_Helper(dest, U, Rn_index, imm12);
}

void JitX64::LoadAndStoreWordOrUnsignedByte_ImmediatePreIndexed(X64Reg dest, bool U, ArmReg Rn_index, ArmImm12 imm12) {
    ASSERT_MSG(Rn_index != 15, "UNPREDICTABLE");

    X64Reg Rn = reg_alloc.BindArmForReadWrite(Rn_index);

    LoadAndStoreWordOrUnsignedByte_Immediate_Helper(Rn, U, Rn_index, imm12);

    code->MOV(32, R(dest), R(Rn));
    reg_alloc.UnlockArm(Rn_index);
}

void JitX64::LoadAndStoreWordOrUnsignedByte_ImmediatePostIndexed(X64Reg dest, bool U, ArmReg Rn_index, ArmImm12 imm12) {
    ASSERT_MSG(Rn_index != 15, "UNPREDICTABLE");

    X64Reg Rn = reg_alloc.BindArmForReadWrite(Rn_index);
    code->MOV(32, R(dest), R(Rn));

    LoadAndStoreWordOrUnsignedByte_Immediate_Helper(Rn, U, Rn_index, imm12);

    reg_alloc.UnlockArm(Rn_index);
}

/// This function assumes that the value of Rn is already in dest.
void JitX64::LoadAndStoreWordOrUnsignedByte_Register_Helper(X64Reg dest, bool U, ArmReg Rn_index, ArmReg Rm_index) {
    // address = Rn +/- Rm

    ASSERT_MSG(Rm_index != 15, "UNPREDICTABLE");

    if (Rm_index == Rn_index) {
        if (U) {
            // address = Rn + Rn
            code->SHL(32, R(dest), Imm8(1));
        } else {
            // address = Rn - Rn
            code->MOV(32, R(dest), Imm32(0));
        }
        return;
    }

    OpArg Rm = reg_alloc.LockArmForRead(Rm_index);

    if (U) {
        // address = Rn + Rm
        code->ADD(32, R(dest), Rm);
    } else {
        // address = Rn - Rm
        code->SUB(32, R(dest), Rm);
    }

    reg_alloc.UnlockArm(Rm_index);
}

/// This function assumes that the value of Rn is already in dest.
void JitX64::LoadAndStoreWordOrUnsignedByte_ScaledRegister_Helper(X64Reg dest, bool U, ArmReg Rn_index, ArmImm5 imm5, ShiftType shift, ArmReg Rm_index) {
    if (imm5 == 0 && shift == 0) {
        LoadAndStoreWordOrUnsignedByte_Register_Helper(dest, U, Rn_index, Rm_index);
        return;
    }

    // index = Rm LSL imm5 / Rm LSR imm5 / Rm ASR imm5 / Rm ROR imm5 / Rm RRX
    // address = Rn +/- index

    ASSERT_MSG(Rm_index != 15, "UNPREDICTABLE");

    // TODO: Optimizations when Rn_index == Rm_index maybe.

    X64Reg index = reg_alloc.AllocTemp();
    if (Rn_index == Rm_index) {
        code->MOV(32, R(index), R(dest));
    } else {
        OpArg Rm = reg_alloc.LockArmForRead(Rm_index);
        code->MOV(32, R(index), Rm);
        reg_alloc.UnlockArm(Rm_index);
    }

    CompileShifter_imm(index, imm5, shift, false);

    if (U) {
        // address = Rn + index
        code->ADD(32, R(dest), R(index));
    } else {
        // address = Rn - index
        code->SUB(32, R(dest), R(index));
    }

    reg_alloc.UnlockTemp(index);
}

void JitX64::LoadAndStoreWordOrUnsignedByte_ScaledRegisterOffset(X64Reg dest, bool U, ArmReg Rn_index, ArmImm5 imm5, ShiftType shift, ArmReg Rm_index) {
    if (Rn_index != 15) {
        OpArg Rn = reg_alloc.LockArmForRead(Rn_index);
        code->MOV(32, R(dest), Rn);
        reg_alloc.UnlockArm(Rn_index);
    } else {
        code->MOV(32, R(dest), Imm32(GetReg15Value_WordAligned()));
    }

    LoadAndStoreWordOrUnsignedByte_ScaledRegister_Helper(dest, U, Rn_index, imm5, shift, Rm_index);
}

void JitX64::LoadAndStoreWordOrUnsignedByte_ScaledRegisterPreIndexed(X64Reg dest, bool U, ArmReg Rn_index, ArmImm5 imm5, ShiftType shift, ArmReg Rm_index) {
    ASSERT_MSG(Rn_index != 15, "UNPREDICTABLE");

    X64Reg Rn = reg_alloc.BindArmForReadWrite(Rn_index);

    LoadAndStoreWordOrUnsignedByte_ScaledRegister_Helper(Rn, U, Rn_index, imm5, shift, Rm_index);

    code->MOV(32, R(dest), R(Rn));
    reg_alloc.UnlockArm(Rn_index);
}

void JitX64::LoadAndStoreWordOrUnsignedByte_ScaledRegisterPostIndexed(X64Reg dest, bool U, ArmReg Rn_index, ArmImm5 imm5, ShiftType shift, ArmReg Rm_index) {
    ASSERT_MSG(Rn_index != 15, "UNPREDICTABLE");

    X64Reg Rn = reg_alloc.BindArmForReadWrite(Rn_index);
    code->MOV(32, R(dest), R(Rn));

    LoadAndStoreWordOrUnsignedByte_ScaledRegister_Helper(Rn, U, Rn_index, imm5, shift, Rm_index);

    reg_alloc.UnlockArm(Rn_index);
}

// TODO: Set up an appropriately mapped region of memory to use instead of compiling CALL instructions.

static u64 Load64LE(u32 addr) {
    // TODO: Improve this.
    return Memory::Read32(addr) | (static_cast<u64>(Memory::Read32(addr + 4)) << 32);
}

static u64 Load64BE(u32 addr) {
    // TODO: Improve this.
    return Common::swap32(Memory::Read32(addr)) | (static_cast<u64>(Common::swap32(Memory::Read32(addr + 4))) << 32);
}

static void Store64LE(u32 addr, u32 v1, u32 v2) {
    Memory::Write32(addr, v1);
    Memory::Write32(addr + 4, v2);
}

static void Store64BE(u32 addr, u32 v1, u32 v2) {
    Memory::Write32(addr, Common::swap32(v1));
    Memory::Write32(addr+4, Common::swap32(v2));
}

static u32 Load32LE(u32 addr) {
    return Memory::Read32(addr);
}

static u32 Load32BE(u32 addr) {
    return Common::swap32(Memory::Read32(addr));
}

static void Store32LE(u32 addr, u32 value) {
    Memory::Write32(addr, value);
}

static void Store32BE(u32 addr, u32 value) {
    Memory::Write32(addr, Common::swap32(value));
}

static u16 Load16LE(u32 addr) {
    return Memory::Read16(addr);
}

static u16 Load16BE(u32 addr) {
    return Common::swap16(Memory::Read16(addr));
}

static void Store16LE(u32 addr, u16 value) {
    Memory::Write16(addr, value);
}

static void Store16BE(u32 addr, u16 value) {
    Memory::Write16(addr, Common::swap16(value));
}

static u32 Load8(u32 addr) {
    return Memory::Read8(addr);
}

static void Store8(u32 addr, u8 value) {
    Memory::Write8(addr, value);
}

static void GetValueOfRegister(XEmitter* code, RegAlloc& reg_alloc, u32 r15_value, X64Reg x64_reg, ArmReg arm_reg) {
    if (arm_reg != 15) {
        OpArg Rd = reg_alloc.LockArmForRead(arm_reg);
        code->MOV(32, R(x64_reg), Rd);
        reg_alloc.UnlockArm(arm_reg);
    } else {
        // The following is IMPLEMENTATION DEFINED
        code->MOV(32, R(x64_reg), Imm32(r15_value));
    }
}

/**
* This function implements address resolution logic common to all the addressing mode 2 store/load instructions.
* The address is loaded into ABI_PARAM1.
*/
template <typename OffsetFn, typename PreFn, typename PostFn, typename... Args>
static void LoadStoreCommon_AddrMode2(JitX64* jit, RegAlloc& reg_alloc, bool P, bool W, OffsetFn offset_fn, PreFn pre_fn, PostFn post_fn, Args... args) {
    constexpr X64Reg address = ABI_PARAM1;
    reg_alloc.FlushX64(address);
    reg_alloc.LockX64(address);

    if (P) {
        if (!W) {
            (jit->*offset_fn)(address, args...);
        } else {
            (jit->*pre_fn)(address, args...);
        }
    } else {
        if (!W) {
            (jit->*post_fn)(address, args...);
        } else {
            ASSERT_MSG(false, "Translate load/store instructions are unsupported");
        }
    }
}

// Load/Store Instructions: Addressing Mode 2

void JitX64::LDR_imm(Cond cond, bool P, bool U, bool W, ArmReg Rn_index, ArmReg Rd_index, ArmImm12 imm12) {
    cond_manager.CompileCond((ConditionCode)cond);

    // Rd == R15 is UNPREDICTABLE only if address[1:0] is not 0b00 or if value loaded into R15[1:0] is 0b10.
    if (W)
        ASSERT_MSG(Rn_index != Rd_index, "UNPREDICTABLE");

    LoadStoreCommon_AddrMode2(this, reg_alloc, P, W,
        &JitX64::LoadAndStoreWordOrUnsignedByte_ImmediateOffset,
        &JitX64::LoadAndStoreWordOrUnsignedByte_ImmediatePreIndexed,
        &JitX64::LoadAndStoreWordOrUnsignedByte_ImmediatePostIndexed,
        U, Rn_index, imm12);

    CompileCallHost(reinterpret_cast<const void* const>(!current.EFlag ? &Load32LE : &Load32BE));

    reg_alloc.UnlockX64(ABI_PARAM1);
    reg_alloc.LockX64(ABI_RETURN);

    // TODO: Could be optimized as a rebind instead.
    X64Reg Rd = reg_alloc.BindArmForWrite(Rd_index);
    code->MOV(32, R(Rd), R(ABI_RETURN));
    reg_alloc.UnlockArm(Rd_index);

    reg_alloc.UnlockX64(ABI_RETURN);

    current.arm_pc += GetInstSize();
    if (Rd_index == 15) {
        code->AND(32, MJitStateArmPC(), Imm32(0xFFFFFFFE));
        code->BT(32, R(ABI_RETURN), Imm8(0));
        code->SETcc(CC_C, MJitStateTFlag());
        CompileReturnToDispatch();
    }
}

void JitX64::LDR_reg(Cond cond, bool P, bool U, bool W, ArmReg Rn_index, ArmReg Rd_index, ArmImm5 imm5, ShiftType shift, ArmReg Rm_index) {
    cond_manager.CompileCond((ConditionCode)cond);

    LoadStoreCommon_AddrMode2(this, reg_alloc, P, W,
        &JitX64::LoadAndStoreWordOrUnsignedByte_ScaledRegisterOffset,
        &JitX64::LoadAndStoreWordOrUnsignedByte_ScaledRegisterPreIndexed,
        &JitX64::LoadAndStoreWordOrUnsignedByte_ScaledRegisterPostIndexed,
        U, Rn_index, imm5, shift, Rm_index);

    CompileCallHost(reinterpret_cast<const void* const>(!current.EFlag ? &Load32LE : &Load32BE));

    reg_alloc.UnlockX64(ABI_PARAM1);
    reg_alloc.LockX64(ABI_RETURN);

    // TODO: Could be optimized as a rebind instead.
    X64Reg Rd = reg_alloc.BindArmForWrite(Rd_index);
    code->MOV(32, R(Rd), R(ABI_RETURN));
    reg_alloc.UnlockArm(Rd_index);

    reg_alloc.UnlockX64(ABI_RETURN);

    current.arm_pc += GetInstSize();
    if (Rd_index == 15) {
        code->AND(32, MJitStateArmPC(), Imm32(0xFFFFFFFE));
        code->BT(32, R(ABI_RETURN), Imm8(0));
        code->SETcc(CC_C, MJitStateTFlag());
        CompileReturnToDispatch();
    }
}

void JitX64::LDRB_imm(Cond cond, bool P, bool U, bool W, ArmReg Rn_index, ArmReg Rd_index, ArmImm12 imm12) {
    cond_manager.CompileCond((ConditionCode)cond);

    ASSERT_MSG(Rd_index != 15, "UNPREDICTABLE");
    if (W)
        ASSERT_MSG(Rn_index != Rd_index, "UNPREDICTABLE");

    LoadStoreCommon_AddrMode2(this, reg_alloc, P, W,
        &JitX64::LoadAndStoreWordOrUnsignedByte_ImmediateOffset,
        &JitX64::LoadAndStoreWordOrUnsignedByte_ImmediatePreIndexed,
        &JitX64::LoadAndStoreWordOrUnsignedByte_ImmediatePostIndexed,
        U, Rn_index, imm12);

    CompileCallHost(reinterpret_cast<const void* const>(&Load8));

    reg_alloc.UnlockX64(ABI_PARAM1);
    reg_alloc.LockX64(ABI_RETURN);

    // TODO: Could be optimized as a rebind instead.
    X64Reg Rd = reg_alloc.BindArmForWrite(Rd_index);
    code->MOVZX(32, 8, Rd, R(ABI_RETURN));
    reg_alloc.UnlockArm(Rd_index);

    reg_alloc.UnlockX64(ABI_RETURN);

    current.arm_pc += GetInstSize();
}

void JitX64::LDRB_reg(Cond cond, bool P, bool U, bool W, ArmReg Rn_index, ArmReg Rd_index, ArmImm5 imm5, ShiftType shift, ArmReg Rm_index) {
    cond_manager.CompileCond((ConditionCode)cond);

    ASSERT_MSG(Rd_index != 15, "UNPREDICTABLE");
    if (W)
        ASSERT_MSG(Rn_index != Rd_index, "UNPREDICTABLE");

    LoadStoreCommon_AddrMode2(this, reg_alloc, P, W,
        &JitX64::LoadAndStoreWordOrUnsignedByte_ScaledRegisterOffset,
        &JitX64::LoadAndStoreWordOrUnsignedByte_ScaledRegisterPreIndexed,
        &JitX64::LoadAndStoreWordOrUnsignedByte_ScaledRegisterPostIndexed,
        U, Rn_index, imm5, shift, Rm_index);

    CompileCallHost(reinterpret_cast<const void* const>(&Load8));

    reg_alloc.UnlockX64(ABI_PARAM1);
    reg_alloc.LockX64(ABI_RETURN);

    // TODO: Could be optimized as a rebind instead.
    X64Reg Rd = reg_alloc.BindArmForWrite(Rd_index);
    code->MOVZX(32, 8, Rd, R(ABI_RETURN));
    reg_alloc.UnlockArm(Rd_index);

    reg_alloc.UnlockX64(ABI_RETURN);

    current.arm_pc += GetInstSize();
}

void JitX64::STR_imm(Cond cond, bool P, bool U, bool W, ArmReg Rn_index, ArmReg Rd_index, ArmImm12 imm12) {
    CompileInterpretInstruction();
    return;

    cond_manager.CompileCond((ConditionCode)cond);

    // Rd_index == R15 is IMPLEMENTATION DEFINED
    if (W)
        ASSERT_MSG(Rn_index != Rd_index, "UNPREDICTABLE");

    LoadStoreCommon_AddrMode2(this, reg_alloc, P, W,
        &JitX64::LoadAndStoreWordOrUnsignedByte_ImmediateOffset,
        &JitX64::LoadAndStoreWordOrUnsignedByte_ImmediatePreIndexed,
        &JitX64::LoadAndStoreWordOrUnsignedByte_ImmediatePostIndexed,
        U, Rn_index, imm12);

    reg_alloc.FlushX64(ABI_PARAM2);
    reg_alloc.LockX64(ABI_PARAM2);

    GetValueOfRegister(code, reg_alloc, GetReg15Value(), ABI_PARAM2, Rd_index);

    CompileCallHost(reinterpret_cast<const void* const>(!current.EFlag ? &Store32LE : &Store32BE));

    reg_alloc.UnlockX64(ABI_PARAM1);
    reg_alloc.UnlockX64(ABI_PARAM2);

    // TODO: Exclusive stuff

    current.arm_pc += GetInstSize();
}

void JitX64::STR_reg(Cond cond, bool P, bool U, bool W, ArmReg Rn_index, ArmReg Rd_index, ArmImm5 imm5, ShiftType shift, ArmReg Rm_index) {
    CompileInterpretInstruction();
    return;

    cond_manager.CompileCond((ConditionCode)cond);

    // Rd_index == R15 is IMPLEMENTATION DEFINED
    if (W)
        ASSERT_MSG(Rn_index != Rd_index, "UNPREDICTABLE");

    LoadStoreCommon_AddrMode2(this, reg_alloc, P, W,
        &JitX64::LoadAndStoreWordOrUnsignedByte_ScaledRegisterOffset,
        &JitX64::LoadAndStoreWordOrUnsignedByte_ScaledRegisterPreIndexed,
        &JitX64::LoadAndStoreWordOrUnsignedByte_ScaledRegisterPostIndexed,
        U, Rn_index, imm5, shift, Rm_index);

    reg_alloc.FlushX64(ABI_PARAM2);
    reg_alloc.LockX64(ABI_PARAM2);

    GetValueOfRegister(code, reg_alloc, GetReg15Value(), ABI_PARAM2, Rd_index);

    CompileCallHost(reinterpret_cast<const void* const>(!current.EFlag ? &Store32LE : &Store32BE));

    reg_alloc.UnlockX64(ABI_PARAM1);
    reg_alloc.UnlockX64(ABI_PARAM2);

    // TODO: Exclusive stuff

    current.arm_pc += GetInstSize();
}

void JitX64::STRB_imm(Cond cond, bool P, bool U, bool W, ArmReg Rn_index, ArmReg Rd_index, ArmImm12 imm12) {
    CompileInterpretInstruction();
    return;

    cond_manager.CompileCond((ConditionCode)cond);

    ASSERT_MSG(Rd_index != 15, "UNPREDICTABLE");
    if (W)
        ASSERT_MSG(Rn_index != Rd_index, "UNPREDICTABLE");

    LoadStoreCommon_AddrMode2(this, reg_alloc, P, W,
        &JitX64::LoadAndStoreWordOrUnsignedByte_ImmediateOffset,
        &JitX64::LoadAndStoreWordOrUnsignedByte_ImmediatePreIndexed,
        &JitX64::LoadAndStoreWordOrUnsignedByte_ImmediatePostIndexed,
        U, Rn_index, imm12);

    reg_alloc.FlushX64(ABI_PARAM2);
    reg_alloc.LockX64(ABI_PARAM2);

    GetValueOfRegister(code, reg_alloc, GetReg15Value(), ABI_PARAM2, Rd_index);

    CompileCallHost(reinterpret_cast<const void* const>(&Store8));

    reg_alloc.UnlockX64(ABI_PARAM1);
    reg_alloc.UnlockX64(ABI_PARAM2);

    //TODO: Exclusive stuff

    current.arm_pc += GetInstSize();
}

void JitX64::STRB_reg(Cond cond, bool P, bool U, bool W, ArmReg Rn_index, ArmReg Rd_index, ArmImm5 imm5, ShiftType shift, ArmReg Rm_index) {
    CompileInterpretInstruction();
    return;

    cond_manager.CompileCond((ConditionCode)cond);

    ASSERT_MSG(Rd_index != 15, "UNPREDICTABLE");
    if (W)
        ASSERT_MSG(Rn_index != Rd_index, "UNPREDICTABLE");

    LoadStoreCommon_AddrMode2(this, reg_alloc, P, W,
        &JitX64::LoadAndStoreWordOrUnsignedByte_ScaledRegisterOffset,
        &JitX64::LoadAndStoreWordOrUnsignedByte_ScaledRegisterPreIndexed,
        &JitX64::LoadAndStoreWordOrUnsignedByte_ScaledRegisterPostIndexed,
        U, Rn_index, imm5, shift, Rm_index);

    reg_alloc.FlushX64(ABI_PARAM2);
    reg_alloc.LockX64(ABI_PARAM2);

    GetValueOfRegister(code, reg_alloc, GetReg15Value(), ABI_PARAM2, Rd_index);

    CompileCallHost(reinterpret_cast<const void* const>(&Store8));

    reg_alloc.UnlockX64(ABI_PARAM1);
    reg_alloc.UnlockX64(ABI_PARAM2);

    //TODO: Exclusive stuff

    current.arm_pc += GetInstSize();
}

/**
* This function implements address resolution logic common to all the addressing mode 3 store/load instructions.
* The address is loaded into ABI_PARAM1.
*/
template <typename OffsetFn, typename PreFn, typename PostFn, typename... Args>
static void LoadStoreCommon_AddrMode3(JitX64* jit, RegAlloc& reg_alloc, bool P, bool W, OffsetFn offset_fn, PreFn pre_fn, PostFn post_fn, Args... args) {
    constexpr X64Reg address = ABI_PARAM1;
    reg_alloc.FlushX64(address);
    reg_alloc.LockX64(address);

    if (P) {
        if (!W) {
            (jit->*offset_fn)(address, args...);
        } else {
            (jit->*pre_fn)(address, args...);
        }
    } else {
        if (!W) {
            (jit->*post_fn)(address, args...);
        } else {
            ASSERT_MSG(false, "UNPREDICTABLE");
        }
    }
}

static ArmImm8 CombineImm8ab(ArmImm4 imm8a, ArmImm4 imm8b) {
    return (imm8a << 4) | imm8b;
}

// Load/Store Instructions: Addressing Mode 3

void JitX64::LDRD_imm(Cond cond, bool P, bool U, bool W, ArmReg Rn_index, ArmReg Rd_index, ArmImm4 imm8a, ArmImm4 imm8b) {
    cond_manager.CompileCond((ConditionCode)cond);

    ASSERT_MSG(Rd_index < 14, "UNPREDICTABLE");
    ASSERT_MSG(Rd_index % 2 == 0, "UNDEFINED");
    if (W)
        ASSERT_MSG(Rn_index != Rd_index && Rn_index != Rd_index + 1, "UNPREDICTABLE");

    LoadStoreCommon_AddrMode3(this, reg_alloc, P, W,
        &JitX64::LoadAndStoreWordOrUnsignedByte_ImmediateOffset,
        &JitX64::LoadAndStoreWordOrUnsignedByte_ImmediatePreIndexed,
        &JitX64::LoadAndStoreWordOrUnsignedByte_ImmediatePostIndexed,
        U, Rn_index, CombineImm8ab(imm8a, imm8b));

    CompileCallHost(reinterpret_cast<const void* const>(!current.EFlag ? &Load64LE : &Load64BE));

    reg_alloc.UnlockX64(ABI_PARAM1);
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

void JitX64::LDRD_reg(Cond cond, bool P, bool U, bool W, ArmReg Rn_index, ArmReg Rd_index, ArmReg Rm_index) {
    cond_manager.CompileCond((ConditionCode)cond);

    ASSERT_MSG(Rd_index < 14, "UNPREDICTABLE");
    ASSERT_MSG(Rd_index % 2 == 0, "UNDEFINED");
    ASSERT_MSG(Rm_index != Rd_index && Rm_index != Rd_index + 1, "UNPREDICTABLE");
    if (W)
        ASSERT_MSG(Rn_index != Rd_index && Rn_index != Rd_index + 1, "UNPREDICTABLE");

    LoadStoreCommon_AddrMode3(this, reg_alloc, P, W,
        &JitX64::LoadAndStoreWordOrUnsignedByte_ScaledRegisterOffset,
        &JitX64::LoadAndStoreWordOrUnsignedByte_ScaledRegisterPreIndexed,
        &JitX64::LoadAndStoreWordOrUnsignedByte_ScaledRegisterPostIndexed,
        U, Rn_index, 0, 0, Rm_index);

    CompileCallHost(reinterpret_cast<const void* const>(!current.EFlag ? &Load64LE : &Load64BE));

    reg_alloc.UnlockX64(ABI_PARAM1);
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

void JitX64::LDRH_imm(Cond cond, bool P, bool U, bool W, ArmReg Rn_index, ArmReg Rd_index, ArmImm4 imm8a, ArmImm4 imm8b) {
    cond_manager.CompileCond((ConditionCode)cond);

    ASSERT_MSG(Rd_index != 15, "UNPREDICTABLE");
    if (W)
        ASSERT_MSG(Rn_index != Rd_index, "UNPREDICTABLE");

    LoadStoreCommon_AddrMode3(this, reg_alloc, P, W,
        &JitX64::LoadAndStoreWordOrUnsignedByte_ImmediateOffset,
        &JitX64::LoadAndStoreWordOrUnsignedByte_ImmediatePreIndexed,
        &JitX64::LoadAndStoreWordOrUnsignedByte_ImmediatePostIndexed,
        U, Rn_index, CombineImm8ab(imm8a, imm8b));

    CompileCallHost(reinterpret_cast<const void* const>(!current.EFlag ? &Load16LE : &Load16BE));

    reg_alloc.UnlockX64(ABI_PARAM1);
    reg_alloc.LockX64(ABI_RETURN);

    X64Reg Rd = reg_alloc.BindArmForWrite(Rd_index);
    code->MOVZX(32, 16, Rd, R(ABI_RETURN));
    reg_alloc.UnlockArm(Rd_index);

    reg_alloc.UnlockX64(ABI_RETURN);

    current.arm_pc += GetInstSize();
}

void JitX64::LDRH_reg(Cond cond, bool P, bool U, bool W, ArmReg Rn_index, ArmReg Rd_index, ArmReg Rm_index) {
    cond_manager.CompileCond((ConditionCode)cond);

    ASSERT_MSG(Rd_index != 15, "UNPREDICTABLE");
    if (W)
        ASSERT_MSG(Rn_index != Rd_index, "UNPREDICTABLE");

    LoadStoreCommon_AddrMode3(this, reg_alloc, P, W,
        &JitX64::LoadAndStoreWordOrUnsignedByte_ScaledRegisterOffset,
        &JitX64::LoadAndStoreWordOrUnsignedByte_ScaledRegisterPreIndexed,
        &JitX64::LoadAndStoreWordOrUnsignedByte_ScaledRegisterPostIndexed,
        U, Rn_index, 0, 0, Rm_index);

    CompileCallHost(reinterpret_cast<const void* const>(!current.EFlag ? &Load16LE : &Load16BE));

    reg_alloc.UnlockX64(ABI_PARAM1);
    reg_alloc.LockX64(ABI_RETURN);

    X64Reg Rd = reg_alloc.BindArmForWrite(Rd_index);
    code->MOVZX(32, 16, Rd, R(ABI_RETURN));
    reg_alloc.UnlockArm(Rd_index);

    reg_alloc.UnlockX64(ABI_RETURN);

    current.arm_pc += GetInstSize();
}

void JitX64::LDRSB_imm(Cond cond, bool P, bool U, bool W, ArmReg Rn_index, ArmReg Rd_index, ArmImm4 imm8a, ArmImm4 imm8b) {
    cond_manager.CompileCond((ConditionCode)cond);

    ASSERT_MSG(Rd_index != 15, "UNPREDICTABLE");
    if (W)
        ASSERT_MSG(Rn_index != Rd_index, "UNPREDICTABLE");

    LoadStoreCommon_AddrMode3(this, reg_alloc, P, W,
        &JitX64::LoadAndStoreWordOrUnsignedByte_ImmediateOffset,
        &JitX64::LoadAndStoreWordOrUnsignedByte_ImmediatePreIndexed,
        &JitX64::LoadAndStoreWordOrUnsignedByte_ImmediatePostIndexed,
        U, Rn_index, CombineImm8ab(imm8a, imm8b));

    CompileCallHost(reinterpret_cast<const void* const>(&Load8));

    reg_alloc.UnlockX64(ABI_PARAM1);
    reg_alloc.LockX64(ABI_RETURN);

    X64Reg Rd = reg_alloc.BindArmForWrite(Rd_index);
    code->MOVSX(32, 8, Rd, R(ABI_RETURN));
    reg_alloc.UnlockArm(Rd_index);

    reg_alloc.UnlockX64(ABI_RETURN);

    current.arm_pc += GetInstSize();
}

void JitX64::LDRSB_reg(Cond cond, bool P, bool U, bool W, ArmReg Rn_index, ArmReg Rd_index, ArmReg Rm_index) {
    cond_manager.CompileCond((ConditionCode)cond);

    ASSERT_MSG(Rd_index != 15, "UNPREDICTABLE");
    if (W)
        ASSERT_MSG(Rn_index != Rd_index, "UNPREDICTABLE");

    LoadStoreCommon_AddrMode3(this, reg_alloc, P, W,
        &JitX64::LoadAndStoreWordOrUnsignedByte_ScaledRegisterOffset,
        &JitX64::LoadAndStoreWordOrUnsignedByte_ScaledRegisterPreIndexed,
        &JitX64::LoadAndStoreWordOrUnsignedByte_ScaledRegisterPostIndexed,
        U, Rn_index, 0, 0, Rm_index);

    CompileCallHost(reinterpret_cast<const void* const>(&Load8));

    reg_alloc.UnlockX64(ABI_PARAM1);
    reg_alloc.LockX64(ABI_RETURN);

    X64Reg Rd = reg_alloc.BindArmForWrite(Rd_index);
    code->MOVSX(32, 8, Rd, R(ABI_RETURN));
    reg_alloc.UnlockArm(Rd_index);

    reg_alloc.UnlockX64(ABI_RETURN);

    current.arm_pc += GetInstSize();
}

void JitX64::LDRSH_imm(Cond cond, bool P, bool U, bool W, ArmReg Rn_index, ArmReg Rd_index, ArmImm4 imm8a, ArmImm4 imm8b) {
    cond_manager.CompileCond((ConditionCode)cond);

    ASSERT_MSG(Rd_index != 15, "UNPREDICTABLE");
    if (W)
        ASSERT_MSG(Rn_index != Rd_index, "UNPREDICTABLE");

    LoadStoreCommon_AddrMode3(this, reg_alloc, P, W,
        &JitX64::LoadAndStoreWordOrUnsignedByte_ImmediateOffset,
        &JitX64::LoadAndStoreWordOrUnsignedByte_ImmediatePreIndexed,
        &JitX64::LoadAndStoreWordOrUnsignedByte_ImmediatePostIndexed,
        U, Rn_index, CombineImm8ab(imm8a, imm8b));

    CompileCallHost(reinterpret_cast<const void* const>(!current.EFlag ? &Load16LE : &Load16BE));

    reg_alloc.UnlockX64(ABI_PARAM1);
    reg_alloc.LockX64(ABI_RETURN);

    X64Reg Rd = reg_alloc.BindArmForWrite(Rd_index);
    code->MOVSX(32, 16, Rd, R(ABI_RETURN));
    reg_alloc.UnlockArm(Rd_index);

    reg_alloc.UnlockX64(ABI_RETURN);

    current.arm_pc += GetInstSize();
}

void JitX64::LDRSH_reg(Cond cond, bool P, bool U, bool W, ArmReg Rn_index, ArmReg Rd_index, ArmReg Rm_index) {
    cond_manager.CompileCond((ConditionCode)cond);

    ASSERT_MSG(Rd_index != 15, "UNPREDICTABLE");
    if (W)
        ASSERT_MSG(Rn_index != Rd_index, "UNPREDICTABLE");

    LoadStoreCommon_AddrMode3(this, reg_alloc, P, W,
        &JitX64::LoadAndStoreWordOrUnsignedByte_ScaledRegisterOffset,
        &JitX64::LoadAndStoreWordOrUnsignedByte_ScaledRegisterPreIndexed,
        &JitX64::LoadAndStoreWordOrUnsignedByte_ScaledRegisterPostIndexed,
        U, Rn_index, 0, 0, Rm_index);

    CompileCallHost(reinterpret_cast<const void* const>(!current.EFlag ? &Load16LE : &Load16BE));

    reg_alloc.UnlockX64(ABI_PARAM1);
    reg_alloc.LockX64(ABI_RETURN);

    X64Reg Rd = reg_alloc.BindArmForWrite(Rd_index);
    code->MOVSX(32, 16, Rd, R(ABI_RETURN));
    reg_alloc.UnlockArm(Rd_index);

    reg_alloc.UnlockX64(ABI_RETURN);

    current.arm_pc += GetInstSize();
}

void JitX64::STRD_imm(Cond cond, bool P, bool U, bool W, ArmReg Rn_index, ArmReg Rd_index, ArmImm4 imm8a, ArmImm4 imm8b) {
    CompileInterpretInstruction();
    return;

    cond_manager.CompileCond((ConditionCode)cond);

    ASSERT_MSG(Rd_index < 14, "UNPREDICTABLE");
    ASSERT_MSG(Rd_index % 2 == 0, "UNDEFINED");

    LoadStoreCommon_AddrMode3(this, reg_alloc, P, W,
        &JitX64::LoadAndStoreWordOrUnsignedByte_ImmediateOffset,
        &JitX64::LoadAndStoreWordOrUnsignedByte_ImmediatePreIndexed,
        &JitX64::LoadAndStoreWordOrUnsignedByte_ImmediatePostIndexed,
        U, Rn_index, CombineImm8ab(imm8a, imm8b));

    reg_alloc.FlushX64(ABI_PARAM2);
    reg_alloc.LockX64(ABI_PARAM2);
    reg_alloc.FlushX64(ABI_PARAM3);
    reg_alloc.LockX64(ABI_PARAM3);

    GetValueOfRegister(code, reg_alloc, GetReg15Value(), ABI_PARAM2, Rd_index + 0);
    GetValueOfRegister(code, reg_alloc, GetReg15Value(), ABI_PARAM3, Rd_index + 1);

    CompileCallHost(reinterpret_cast<const void* const>(!current.EFlag ? &Store64LE : &Store64BE));

    reg_alloc.UnlockX64(ABI_PARAM1);
    reg_alloc.UnlockX64(ABI_PARAM2);
    reg_alloc.UnlockX64(ABI_PARAM3);

    // TODO: Exclusive stuff.

    current.arm_pc += GetInstSize();
}

void JitX64::STRD_reg(Cond cond, bool P, bool U, bool W, ArmReg Rn_index, ArmReg Rd_index, ArmReg Rm_index) {
    CompileInterpretInstruction();
    return;

    cond_manager.CompileCond((ConditionCode)cond);

    ASSERT_MSG(Rd_index < 14, "UNPREDICTABLE");
    ASSERT_MSG(Rd_index % 2 == 0, "UNDEFINED");
    if (W)
        ASSERT_MSG(Rn_index != Rd_index && Rn_index != Rd_index + 1, "UNPREDICTABLE");

    LoadStoreCommon_AddrMode3(this, reg_alloc, P, W,
        &JitX64::LoadAndStoreWordOrUnsignedByte_ScaledRegisterOffset,
        &JitX64::LoadAndStoreWordOrUnsignedByte_ScaledRegisterPreIndexed,
        &JitX64::LoadAndStoreWordOrUnsignedByte_ScaledRegisterPostIndexed,
        U, Rn_index, 0, 0, Rm_index);

    reg_alloc.FlushX64(ABI_PARAM2);
    reg_alloc.LockX64(ABI_PARAM2);
    reg_alloc.FlushX64(ABI_PARAM3);
    reg_alloc.LockX64(ABI_PARAM3);

    GetValueOfRegister(code, reg_alloc, GetReg15Value(), ABI_PARAM2, Rd_index + 0);
    GetValueOfRegister(code, reg_alloc, GetReg15Value(), ABI_PARAM3, Rd_index + 1);

    CompileCallHost(reinterpret_cast<const void* const>(!current.EFlag ? &Store64LE : &Store64BE));

    reg_alloc.UnlockX64(ABI_PARAM1);
    reg_alloc.UnlockX64(ABI_PARAM2);
    reg_alloc.UnlockX64(ABI_PARAM3);

    // TODO: Exclusive stuff.

    current.arm_pc += GetInstSize();
}

void JitX64::STRH_imm(Cond cond, bool P, bool U, bool W, ArmReg Rn_index, ArmReg Rd_index, ArmImm4 imm8a, ArmImm4 imm8b) {
    CompileInterpretInstruction();
    return;

    cond_manager.CompileCond((ConditionCode)cond);

    ASSERT_MSG(Rd_index != 15, "UNPREDICTABLE");
    if (W)
        ASSERT_MSG(Rd_index != Rn_index, "UNPREDICTABLE");

    LoadStoreCommon_AddrMode3(this, reg_alloc, P, W,
        &JitX64::LoadAndStoreWordOrUnsignedByte_ImmediateOffset,
        &JitX64::LoadAndStoreWordOrUnsignedByte_ImmediatePreIndexed,
        &JitX64::LoadAndStoreWordOrUnsignedByte_ImmediatePostIndexed,
        U, Rn_index, CombineImm8ab(imm8a, imm8b));

    reg_alloc.FlushX64(ABI_PARAM2);
    reg_alloc.LockX64(ABI_PARAM2);

    GetValueOfRegister(code, reg_alloc, GetReg15Value(), ABI_PARAM2, Rd_index);

    CompileCallHost(reinterpret_cast<const void* const>(!current.EFlag ? &Store16LE : &Store16BE));

    reg_alloc.UnlockX64(ABI_PARAM1);
    reg_alloc.UnlockX64(ABI_PARAM2);

    // TODO: Exclusive stuff.

    current.arm_pc += GetInstSize();
}

void JitX64::STRH_reg(Cond cond, bool P, bool U, bool W, ArmReg Rn_index, ArmReg Rd_index, ArmReg Rm_index) {
    CompileInterpretInstruction();
    return;

    cond_manager.CompileCond((ConditionCode)cond);

    ASSERT_MSG(Rd_index != 15, "UNPREDICTABLE");
    if (W)
        ASSERT_MSG(Rd_index != Rn_index, "UNPREDICTABLE");

    LoadStoreCommon_AddrMode3(this, reg_alloc, P, W,
        &JitX64::LoadAndStoreWordOrUnsignedByte_ScaledRegisterOffset,
        &JitX64::LoadAndStoreWordOrUnsignedByte_ScaledRegisterPreIndexed,
        &JitX64::LoadAndStoreWordOrUnsignedByte_ScaledRegisterPostIndexed,
        U, Rn_index, 0, 0, Rm_index);

    reg_alloc.FlushX64(ABI_PARAM2);
    reg_alloc.LockX64(ABI_PARAM2);

    GetValueOfRegister(code, reg_alloc, GetReg15Value(), ABI_PARAM2, Rd_index);

    CompileCallHost(reinterpret_cast<const void* const>(!current.EFlag ? &Store16LE : &Store16BE));

    reg_alloc.UnlockX64(ABI_PARAM1);
    reg_alloc.UnlockX64(ABI_PARAM2);

    // TODO: Exclusive stuff.

    current.arm_pc += GetInstSize();
}

void JitX64::LDRBT() { CompileInterpretInstruction(); }
void JitX64::LDRHT() { CompileInterpretInstruction(); }
void JitX64::LDRSBT() { CompileInterpretInstruction(); }
void JitX64::LDRSHT() { CompileInterpretInstruction(); }
void JitX64::LDRT() { CompileInterpretInstruction(); }
void JitX64::STRBT() { CompileInterpretInstruction(); }
void JitX64::STRHT() { CompileInterpretInstruction(); }
void JitX64::STRT() { CompileInterpretInstruction(); }

// Load/Store multiple instructions

static void LoadAndStoreMultiple_IncrementAfter(XEmitter* code, RegAlloc& reg_alloc, bool W, ArmReg Rn_index, ArmRegList list, std::function<void()> call) {
    if (W) {
        X64Reg Rn = reg_alloc.BindArmForReadWrite(Rn_index);
        code->MOV(32, R(ABI_PARAM1), R(Rn));
        reg_alloc.UnlockArm(Rn_index);

        call();

        Rn = reg_alloc.BindArmForReadWrite(Rn_index);
        code->ADD(32, R(Rn), Imm8(4 * Common::CountSetBits(list)));
        reg_alloc.UnlockArm(Rn_index);
    } else {
        OpArg Rn = reg_alloc.LockArmForReadWrite(Rn_index);
        code->MOV(32, R(ABI_PARAM1), Rn);
        reg_alloc.UnlockArm(Rn_index);
        call();
    }
}

static void LoadAndStoreMultiple_IncrementBefore(XEmitter* code, RegAlloc& reg_alloc, bool W, ArmReg Rn_index, ArmRegList list, std::function<void()> call) {
    if (W) {
        X64Reg Rn = reg_alloc.BindArmForReadWrite(Rn_index);
        code->MOV(32, R(ABI_PARAM1), R(Rn));
        code->ADD(32, R(ABI_PARAM1), Imm8(4));
        reg_alloc.UnlockArm(Rn_index);

        call();

        Rn = reg_alloc.BindArmForReadWrite(Rn_index);
        code->ADD(32, R(Rn), Imm8(4 * Common::CountSetBits(list)));
        reg_alloc.UnlockArm(Rn_index);
    } else {
        OpArg Rn = reg_alloc.LockArmForReadWrite(Rn_index);
        code->MOV(32, R(ABI_PARAM1), Rn);
        code->ADD(32, R(ABI_PARAM1), Imm8(4));
        reg_alloc.UnlockArm(Rn_index);
        call();
    }
}

static void LoadAndStoreMultiple_DecrementAfter(XEmitter* code, RegAlloc& reg_alloc, bool W, ArmReg Rn_index, ArmRegList list, std::function<void()> call) {
    if (W) {
        X64Reg Rn = reg_alloc.BindArmForReadWrite(Rn_index);
        code->MOV(32, R(ABI_PARAM1), R(Rn));
        code->SUB(32, R(ABI_PARAM1), Imm32(4 * Common::CountSetBits(list) - 4));
        reg_alloc.UnlockArm(Rn_index);

        call();

        Rn = reg_alloc.BindArmForReadWrite(Rn_index);
        code->SUB(32, R(Rn), Imm32(4 * Common::CountSetBits(list)));
        reg_alloc.UnlockArm(Rn_index);
    } else {
        OpArg Rn = reg_alloc.LockArmForReadWrite(Rn_index);
        code->MOV(32, R(ABI_PARAM1), Rn);
        code->SUB(32, R(ABI_PARAM1), Imm32(4 * Common::CountSetBits(list) - 4));
        reg_alloc.UnlockArm(Rn_index);
        call();
    }
}

static void LoadAndStoreMultiple_DecrementBefore(XEmitter* code, RegAlloc& reg_alloc, bool W, ArmReg Rn_index, ArmRegList list, std::function<void()> call) {
    if (W && !(list & (1 << Rn_index))) {
        X64Reg Rn = reg_alloc.BindArmForReadWrite(Rn_index);
        code->SUB(32, R(Rn), Imm32(4 * Common::CountSetBits(list)));
        code->MOV(32, R(ABI_PARAM1), R(Rn));
        reg_alloc.UnlockArm(Rn_index);
        call();
    } else if (W) {
        X64Reg Rn = reg_alloc.BindArmForReadWrite(Rn_index);
        code->MOV(32, R(ABI_PARAM1), R(Rn));
        code->SUB(32, R(ABI_PARAM1), Imm32(4 * Common::CountSetBits(list)));
        reg_alloc.UnlockArm(Rn_index);

        call();

        Rn = reg_alloc.BindArmForReadWrite(Rn_index);
        code->SUB(32, R(Rn), Imm32(4 * Common::CountSetBits(list)));
        reg_alloc.UnlockArm(Rn_index);
    } else {
        OpArg Rn = reg_alloc.LockArmForReadWrite(Rn_index);
        code->MOV(32, R(ABI_PARAM1), Rn);
        code->SUB(32, R(ABI_PARAM1), Imm32(4 * Common::CountSetBits(list)));
        reg_alloc.UnlockArm(Rn_index);
        call();
    }
}

static void LoadAndStoreMultiple_Helper(XEmitter* code, RegAlloc& reg_alloc, bool P, bool U, bool W, ArmReg Rn_index, ArmRegList list, std::function<void()> call) {
    reg_alloc.FlushX64(ABI_PARAM1);
    reg_alloc.LockX64(ABI_PARAM1);
    reg_alloc.FlushX64(ABI_PARAM2);
    reg_alloc.LockX64(ABI_PARAM2);
    reg_alloc.FlushX64(ABI_PARAM3);
    reg_alloc.LockX64(ABI_PARAM3);

    for (int i = 0; i < 15; i++) {
        if (list & (1 << i)) {
            reg_alloc.FlushArm(i);
        }
    }

    code->MOV(32, R(ABI_PARAM2), Imm32(list));
    code->MOV(64, R(ABI_PARAM3), R(reg_alloc.JitStateReg()));

    if (!P && !U) {
        LoadAndStoreMultiple_DecrementAfter(code, reg_alloc, W, Rn_index, list, call);
    } else if (!P && U) {
        LoadAndStoreMultiple_IncrementAfter(code, reg_alloc, W, Rn_index, list, call);
    } else if (P && !U) {
        LoadAndStoreMultiple_DecrementBefore(code, reg_alloc, W, Rn_index, list, call);
    } else if (P && U) {
        LoadAndStoreMultiple_IncrementBefore(code, reg_alloc, W, Rn_index, list, call);
    } else {
        UNREACHABLE();
    }

    reg_alloc.UnlockX64(ABI_PARAM1);
    reg_alloc.UnlockX64(ABI_PARAM2);
    reg_alloc.UnlockX64(ABI_PARAM3);
}

static void ExecuteLDMLE(u32 start_address, u16 reg_list, JitState* jit_state) {
    for (int i = 0; i < 16; i++) {
        const u16 bit = 1 << i;
        if (reg_list & bit) {
            jit_state->cpu_state.Reg[i] = Memory::Read32(start_address);
            start_address += 4;
        }
    }
}

static void ExecuteLDMBE(u32 start_address, u16 reg_list, JitState* jit_state) {
    for (int i = 0; i < 16; i++) {
        const u16 bit = 1 << i;
        if (reg_list & bit) {
            jit_state->cpu_state.Reg[i] = Common::swap32(Memory::Read32(start_address));
            start_address += 4;
        }
    }
}

void JitX64::LDM(Cond cond, bool P, bool U, bool W, ArmReg Rn_index, ArmRegList list) {
    cond_manager.CompileCond((ConditionCode)cond);

    ASSERT_MSG(Rn_index != 15, "UNPREDICTABLE");
    ASSERT_MSG(list != 0, "UNPREDICTABLE");
    if (W && (list & (1 << Rn_index)))
        ASSERT_MSG(false, "UNPREDICTABLE");

    // TODO: Optimize

    LoadAndStoreMultiple_Helper(code, reg_alloc, P, U, W, Rn_index, list,
        [this](){ CompileCallHost(reinterpret_cast<const void* const>(!current.EFlag ? &ExecuteLDMLE : &ExecuteLDMBE)); });

    current.arm_pc += GetInstSize();
    if (list & (1 << 15)) {
        code->BT(32, MJitStateArmPC(), Imm8(0));
        code->SETcc(CC_C, MJitStateTFlag());
        code->AND(32, MJitStateArmPC(), Imm32(0xFFFFFFFE));
        CompileReturnToDispatch();
    }
}

static void ExecuteSTMLE(u32 start_address, u16 reg_list, JitState* jit_state) {
    for (int i = 0; i < 15; i++) {
        const u16 bit = 1 << i;
        if (reg_list & bit) {
            Memory::Write32(start_address, jit_state->cpu_state.Reg[i]);
            start_address += 4;
        }
    }
    // Reading R15 here is IMPLEMENTATION DEFINED
    if (reg_list & (1 << 15)) {
        if (!jit_state->cpu_state.TFlag) {
            Memory::Write32(start_address, jit_state->cpu_state.Reg[15] + 8);
        } else {
            Memory::Write32(start_address, jit_state->cpu_state.Reg[15] + 4);
        }
    }
}

static void ExecuteSTMBE(u32 start_address, u16 reg_list, JitState* jit_state) {
    for (int i = 0; i < 16; i++) {
        const u16 bit = 1 << i;
        if (reg_list & bit) {
            Memory::Write32(start_address, Common::swap32(jit_state->cpu_state.Reg[i]));
            start_address += 4;
        }
    }
    // Reading R15 here is IMPLEMENTATION DEFINED
    if (reg_list & (1 << 15)) {
        if (!jit_state->cpu_state.TFlag) {
            Memory::Write32(start_address, Common::swap32(jit_state->cpu_state.Reg[15] + 8));
        } else {
            Memory::Write32(start_address, Common::swap32(jit_state->cpu_state.Reg[15] + 4));
        }
    }
}

void JitX64::STM(Cond cond, bool P, bool U, bool W, ArmReg Rn_index, ArmRegList list) {
    CompileInterpretInstruction();
    return;

    cond_manager.CompileCond((ConditionCode)cond);

    ASSERT_MSG(Rn_index != 15, "UNPREDICTABLE");
    ASSERT_MSG(list != 0, "UNPREDICTABLE");
    if (W && (list & (1 << Rn_index)))
        ASSERT_MSG((list & ((1 << Rn_index) - 1)) == 0, "UNPREDICTABLE");

    // TODO: Optimize

    LoadAndStoreMultiple_Helper(code, reg_alloc, P, U, W, Rn_index, list,
        [this](){ CompileCallHost(reinterpret_cast<const void* const>(!current.EFlag ? &ExecuteSTMLE : &ExecuteSTMBE)); });

    // TODO: Exclusive stuff

    current.arm_pc += GetInstSize();
}

void JitX64::LDM_usr() { CompileInterpretInstruction(); }
void JitX64::LDM_eret() { CompileInterpretInstruction(); }
void JitX64::STM_usr() { CompileInterpretInstruction(); }

}
