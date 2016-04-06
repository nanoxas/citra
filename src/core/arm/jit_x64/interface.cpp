
// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <cstring>

#include "common/assert.h"
#include "common/make_unique.h"
#include "common/x64/abi.h"
#include "common/x64/emitter.h"

#include "core/arm/jit_x64/common.h"
#include "core/arm/jit_x64/interface.h"
#include "core/arm/jit_x64/jit_x64.h"
#include "core/core.h"
#include "core/core_timing.h"

namespace Gen {

struct RunJittedCode final : private Gen::XCodeBlock {
private:
    using RunJitFuncType = void(*)(JitX64::JitState*, void*);
    RunJitFuncType run_jit;
    u64 return_from_run_jit;

public:
    RunJittedCode() {
        AllocCodeSpace(1024);

        run_jit = RunJitFuncType(this->GetCodePtr());

        // This serves two purposes:
        // 1. It saves all the registers we as a callee need to save.
        // 2. It aligns the stack so that the code the JIT emits can assume
        //    that the stack is appropriately aligned for CALLs.
        ABI_PushRegistersAndAdjustStack(ABI_ALL_CALLEE_SAVED, 8);

        MOV(64, MDisp(ABI_PARAM1, offsetof(JitX64::JitState, save_host_RSP)), R(RSP));
        MOV(64, R(R15), R(ABI_PARAM1));

        JMPptr(R(ABI_PARAM2));
        return_from_run_jit = reinterpret_cast<u64>(this->GetCodePtr());

        MOV(64, R(RSP), MDisp(R15, offsetof(JitX64::JitState, save_host_RSP)));
        ABI_PopRegistersAndAdjustStack(ABI_ALL_CALLEE_SAVED, 8);
        RET();
    }

    unsigned CallCode(JitX64::JitState* jit_state, void* bb, unsigned cycles_to_run, u32& new_pc) {
        auto cpu = &jit_state->cpu_state;

        cpu->NFlag = (cpu->Cpsr >> 31);
        cpu->ZFlag = (cpu->Cpsr >> 30) & 1;
        cpu->CFlag = (cpu->Cpsr >> 29) & 1;
        cpu->VFlag = (cpu->Cpsr >> 28) & 1;
        cpu->TFlag = (cpu->Cpsr >> 5) & 1;

        jit_state->cycles_remaining = cycles_to_run;
        jit_state->return_RIP = CallCodeReturnAddress();

        run_jit(jit_state, bb);

        new_pc = cpu->Reg[15];

        cpu->Cpsr = (cpu->Cpsr & 0x0fffffdf) |
                    (cpu->NFlag << 31) |
                    (cpu->ZFlag << 30) |
                    (cpu->CFlag << 29) |
                    (cpu->VFlag << 28) |
                    (cpu->TFlag << 5);

        return cycles_to_run - jit_state->cycles_remaining;
    }

    u64 CallCodeReturnAddress() const {
        return return_from_run_jit;
    }
};

struct BlockOfCode : Gen::XCodeBlock {
    BlockOfCode() {
        AllocCodeSpace(128 * 1024 * 1024);
    }
};

}

namespace JitX64 {

struct ARM_Jit::Impl {
    Gen::RunJittedCode run_jit = {};
    Gen::BlockOfCode block_of_code = {};
    JitX64 compiler{ &block_of_code };
};

ARM_Jit::ARM_Jit(PrivilegeMode initial_mode) : impl(std::make_unique<Impl>()), state(Common::make_unique<JitState>()) {
    ASSERT_MSG(initial_mode == PrivilegeMode::USER32MODE, "Unimplemented");
    ClearCache();
}

ARM_Jit::~ARM_Jit() {
}

void ARM_Jit::SetPC(u32 pc) {
    state->cpu_state.Reg[15] = pc;
}

u32 ARM_Jit::GetPC() const {
    return state->cpu_state.Reg[15];
}

u32 ARM_Jit::GetReg(int index) const {
    if (index == 15) return GetPC();
    return state->cpu_state.Reg[index];
}

void ARM_Jit::SetReg(int index, u32 value) {
    if (index == 15) return SetPC(value);
    state->cpu_state.Reg[index] = value;
}

u32 ARM_Jit::GetVFPReg(int index) const {
    return state->cpu_state.ExtReg[index];
}

void ARM_Jit::SetVFPReg(int index, u32 value) {
    state->cpu_state.ExtReg[index] = value;
}

u32 ARM_Jit::GetVFPSystemReg(VFPSystemRegister reg) const {
    return state->cpu_state.VFP[reg];
}

void ARM_Jit::SetVFPSystemReg(VFPSystemRegister reg, u32 value) {
    state->cpu_state.VFP[reg] = value;
}

u32 ARM_Jit::GetCPSR() const {
    return state->cpu_state.Cpsr;
}

void ARM_Jit::SetCPSR(u32 cpsr) {
    state->cpu_state.Cpsr = cpsr;
}

u32 ARM_Jit::GetCP15Register(CP15Register reg) {
    return state->cpu_state.CP15[reg];
}

void ARM_Jit::SetCP15Register(CP15Register reg, u32 value) {
    state->cpu_state.CP15[reg] = value;
}

void ARM_Jit::AddTicks(u64 ticks) {
    down_count -= ticks;
    if (down_count < 0)
        CoreTiming::Advance();
}

void ARM_Jit::ExecuteInstructions(int num_instructions) {
    reschedule = false;

    do {
        bool EFlag = (state->cpu_state.Cpsr >> 9) & 1;
        state->cpu_state.TFlag = (state->cpu_state.Cpsr >> 5) & 1;

        if (!state->cpu_state.NirqSig) {
            UNIMPLEMENTED();
        }

        if (state->cpu_state.TFlag)
            state->cpu_state.Reg[15] &= 0xfffffffe;
        else
            state->cpu_state.Reg[15] &= 0xfffffffc;

        u8* ptr = impl->compiler.GetBB(state->cpu_state.Reg[15], state->cpu_state.TFlag, EFlag);

        unsigned ticks_executed = impl->run_jit.CallCode(state.get(), ptr, num_instructions, state->cpu_state.Reg[15]);
        num_instructions -= ticks_executed;
        AddTicks(ticks_executed);
    } while (!reschedule && num_instructions > 0);
}

void ARM_Jit::ResetContext(Core::ThreadContext& context, u32 stack_top, u32 entry_point, u32 arg) {
    memset(&context, 0, sizeof(Core::ThreadContext));

    context.cpu_registers[0] = arg;
    context.pc = entry_point;
    context.sp = stack_top;
    context.cpsr = 0x1F; // Usermode
}

void ARM_Jit::SaveContext(Core::ThreadContext& ctx) {
    memcpy(ctx.cpu_registers, state->cpu_state.Reg.data(), sizeof(ctx.cpu_registers));
    memcpy(ctx.fpu_registers, state->cpu_state.ExtReg.data(), sizeof(ctx.fpu_registers));

    ctx.sp = state->cpu_state.Reg[13];
    ctx.lr = state->cpu_state.Reg[14];
    ctx.pc = state->cpu_state.Reg[15];

    ctx.cpsr = GetCPSR();

    ctx.fpscr = state->cpu_state.VFP[1];
    ctx.fpexc = state->cpu_state.VFP[2];
}

void ARM_Jit::LoadContext(const Core::ThreadContext& ctx) {
    memcpy(state->cpu_state.Reg.data(), ctx.cpu_registers, sizeof(ctx.cpu_registers));
    memcpy(state->cpu_state.ExtReg.data(), ctx.fpu_registers, sizeof(ctx.fpu_registers));

    state->cpu_state.Reg[13] = ctx.sp;
    state->cpu_state.Reg[14] = ctx.lr;
    state->cpu_state.Reg[15] = ctx.pc;
    SetCPSR(ctx.cpsr);

    state->cpu_state.VFP[1] = ctx.fpscr;
    state->cpu_state.VFP[2] = ctx.fpexc;
}

void ARM_Jit::PrepareReschedule() {
    reschedule = true;
    state->cpu_state.NumInstrsToExecute = 0;
}

void ARM_Jit::ClearCache() {
    impl->compiler.ClearCache();
    impl->block_of_code.ClearCodeSpace();
    state->cpu_state.instruction_cache.clear();
}

void ARM_Jit::FastClearCache() {
    impl->compiler.ClearCache();
    impl->block_of_code.ResetCodePtr();
    state->cpu_state.instruction_cache.clear();
}

}
