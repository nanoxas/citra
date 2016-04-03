// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/assert.h"
#include "common/logging/log.h"
#include "common/x64/abi.h"

#include "core/arm/jit_x64/jit_x64.h"
#include "core/hle/svc.h"

namespace JitX64 {

using namespace Gen;

static void Breakpoint(u32 imm) {
    LOG_DEBUG(Core_ARM11, "Breakpoint instruction hit. Immediate: 0x%08X", imm);
}

void JitX64::BKPT(Cond cond, ArmImm12 imm12, ArmImm4 imm4) {
    cond_manager.CompileCond((ConditionCode) cond);

    ASSERT_MSG(false, "BKPT instruction @ pc=0x%08X", current.arm_pc);

    reg_alloc.FlushX64(ABI_PARAM1);
    reg_alloc.LockX64(ABI_PARAM1);

    code->MOV(32, R(ABI_PARAM1), Imm32((imm12 << 4) | imm4));
    CompileCallHost(reinterpret_cast<const void* const>(&Breakpoint));

    reg_alloc.UnlockX64(ABI_PARAM1);

    current.arm_pc += GetInstSize();
}

static void ServiceCall(u64 imm) {
    SVC::CallSVC(imm & 0xFFFF);
}

void JitX64::SVC(Cond cond, ArmImm24 imm24) {
    cond_manager.CompileCond((ConditionCode)cond);

    // Flush and write out absolutely everything.
    code->MOV(32, MJitStateArmPC(), Imm32(current.arm_pc));
    reg_alloc.FlushEverything();

    reg_alloc.LockX64(ABI_PARAM1);
    code->MOV(64, R(ABI_PARAM1), Imm32(imm24));
    CompileCallHost(reinterpret_cast<const void* const>(&ServiceCall));
    reg_alloc.UnlockX64(ABI_PARAM1);

    // Some service calls require a task switch, so go back to the dispatcher to check.
    current.arm_pc += GetInstSize();
    code->ADD(32, MJitStateArmPC(), Imm32(GetInstSize()));
    CompileReturnToDispatch();

    stop_compilation = true;
}

void JitX64::UDF() {
    cond_manager.Always();

    ASSERT_MSG(false, "UDF instruction @ pc=0x%08X", current.arm_pc);

    current.arm_pc += GetInstSize();
}

}
