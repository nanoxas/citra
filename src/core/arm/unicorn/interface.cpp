// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <cstring>

#include "common/make_unique.h"
#include "common/logging/log.h"

#include "core/arm/unicorn/interface.h"
#include "core/core.h"
#include "core/core_timing.h"
#include "core/hle/svc.h"
#include "core/memory.h"

static void service_handler(uc_engine *uc, u32 intno, void *user_data) {
    LOG_INFO(Core, "Calling service: %d", intno);
    SVC::CallSVC(intno & 0xFFFF);
}

ARM_Unicorn::ARM_Unicorn(PrivilegeMode initial_mode) {
#ifdef _WIN32
#ifdef _DEBUG
    bool dll_loaded = uc_dyn_load("unicorn-d.dll", 0);
#else
    bool dll_loaded = uc_dyn_load("unicorn.dll", 0);
#endif
    if (!dll_loaded) {
        LOG_CRITICAL(Core, "Could not load unicorn.dll");
        return;
    }
#endif
    uc_err err = uc_open(UC_ARCH_ARM, UC_MODE_ARM, &engine);
    if (err) {
        LOG_CRITICAL(Core, "Failed to initialize unicorn! Error code: %d", err);
    }
    // Set stack pointer to the top of the stack
    SetReg(13, 0x10000000);
    SetReg(15, 0);

    err = uc_hook_add(engine, &service, UC_HOOK_INTR, reinterpret_cast<void*>(service_handler), nullptr, 0, 0);
    if (err) {
        LOG_CRITICAL(Core, "Failed to set hook. Error message: %s", uc_strerror(err));
    }
}

ARM_Unicorn::~ARM_Unicorn() {
    uc_err err = uc_close(engine);
    if (err) {
        LOG_CRITICAL(Core, "Failed to close unicorn! Error code: %d", err);
    }
}

void ARM_Unicorn::SetPC(u32 pc) {
    uc_err err = uc_reg_write(engine, UC_ARM_REG_PC, &pc);
    if (err) {
        LOG_ERROR(Core, "Failed to write PC register. Error code: %d", err);
    }
}

u32 ARM_Unicorn::GetPC() const {
    u32 ret;
    uc_err err = uc_reg_read(engine, UC_ARM_REG_PC, &ret);
    if (err) {
        LOG_ERROR(Core, "Failed to read PC register. Error code: %d", err);
    }
    return ret;
}

u32 ARM_Unicorn::GetReg(int index) const {
    u32 ret;
    int reg = 0;
    if (index < 13) {
        reg = index + UC_ARM_REG_R0;
    } else if (index == 13) {
        reg = UC_ARM_REG_SP;
    } else if (index == 14) {
        reg = UC_ARM_REG_LR;
    } else {
        reg = UC_ARM_REG_PC;
    }
    uc_err err = uc_reg_read(engine, reg, &ret);
    if (err) {
        LOG_ERROR(Core, "Failed to read register. Error code: %d", err);
    }
    return ret;
}

void ARM_Unicorn::SetReg(int index, u32 value) {
    int reg = 0;
    if (index < 13) {
        reg = index + UC_ARM_REG_R0;
    }
    else if (index == 13) {
        reg = UC_ARM_REG_SP;
    }
    else if (index == 14) {
        reg = UC_ARM_REG_LR;
    }
    else {
        reg = UC_ARM_REG_PC;
    }
    uc_err err = uc_reg_write(engine, reg, &value);
    if (err) {
        LOG_ERROR(Core, "Failed to write register %d. Error code: %d", index, err);
    }
}

u32 ARM_Unicorn::GetVFPReg(int index) const {
    u32 ret;
    uc_err err = uc_reg_read(engine, index + UC_ARM_REG_S0, &ret);
    if (err) {
        LOG_ERROR(Core, "Failed to read VFP register. Error code: %d", err);
    }
    return ret;
}

void ARM_Unicorn::SetVFPReg(int index, u32 value) {
    uc_err err = uc_reg_write(engine, index + UC_ARM_REG_S0, &value);
    if (err) {
        LOG_ERROR(Core, "Failed to write VFP register. Error code: %d", err);
    }
}

u32 ARM_Unicorn::GetVFPSystemReg(VFPSystemRegister reg) const {
    u32 in, out;
    switch (reg) {
    case VFP_FPSID:
        in = UC_ARM_REG_FPSID;
        break;
    case VFP_FPSCR:
        in = UC_ARM_REG_FPSCR;
        break;
    case VFP_FPEXC:
        in = UC_ARM_REG_FPEXC;
        break;
    case VFP_FPINST:
        in = UC_ARM_REG_FPINST;
        break;
    case VFP_FPINST2:
        in = UC_ARM_REG_FPINST2;
        break;
    case VFP_MVFR0:
        in = UC_ARM_REG_MVFR0;
        break;
    case VFP_MVFR1:
        in = UC_ARM_REG_MVFR1;
        break;
    default:
        LOG_CRITICAL(Core, "Attempting to read a value outside of the enum");
        in = UC_ARM_REG_CPSR;
    }
    uc_err err = uc_reg_read(engine, in, &out);
    if (err) {
        LOG_ERROR(Core, "Failed to read VFP register. Error code: %d", err);
    }
    return out;
}

void ARM_Unicorn::SetVFPSystemReg(VFPSystemRegister reg, u32 value) {
    u32 val;
    switch (reg) {
    case VFP_FPSID:
        val = UC_ARM_REG_FPSID;
        break;
    case VFP_FPSCR:
        val = UC_ARM_REG_FPSCR;
        break;
    case VFP_FPEXC:
        val = UC_ARM_REG_FPEXC;
        break;
    case VFP_FPINST:
        val = UC_ARM_REG_FPINST;
        break;
    case VFP_FPINST2:
        val = UC_ARM_REG_FPINST2;
        break;
    case VFP_MVFR0:
        val = UC_ARM_REG_MVFR0;
        break;
    case VFP_MVFR1:
        val = UC_ARM_REG_MVFR1;
        break;
    default:
        LOG_CRITICAL(Core, "Attempting to write a value outside of the enum");
        val = UC_ARM_REG_CPSR;
    }
    uc_err err = uc_reg_write(engine, val, &value);
    if (err) {
        LOG_ERROR(Core, "Failed to write VFP register. Error code: %d", err);
    }
}

u32 ARM_Unicorn::GetCPSR() const {
    u32 ret;
    uc_err err = uc_reg_read(engine, UC_ARM_REG_CPSR, &ret);
    if (err) {
        LOG_ERROR(Core, "Failed to read CPSR register. Error code: %d", err);
    }
    return ret;
}

void ARM_Unicorn::SetCPSR(u32 cpsr) {
    uc_err err = uc_reg_write(engine, UC_ARM_REG_CPSR, &cpsr);
    if (err) {
        LOG_ERROR(Core, "Failed to write CPSR register. Error code: %d", err);
    }
}

u32 ARM_Unicorn::GetCP15Register(CP15Register reg) {
    // TODO Implement this
    return 0;
}

void ARM_Unicorn::SetCP15Register(CP15Register reg, u32 value) {
    // TODO Implement this
}

void ARM_Unicorn::AddTicks(u64 ticks) {
    down_count -= ticks;
    if (down_count < 0)
        CoreTiming::Advance();
}

void ARM_Unicorn::ExecuteInstructions(int num_instructions) {
    //this->NumInstrsToExecute = num_instructions;
    u32 start_address = GetPC();
    //num_instructions = 1;
//    LOG_INFO(Core, "Execute code at 0x%08x", start_address, Memory::Read32());
    LOG_INFO(Core, "CPSR: %d", GetCPSR());
    uc_err err = uc_emu_start(engine, start_address, 0, 0, num_instructions);
    if (err) {
        LOG_INFO(Core, "CPSR: %d", GetCPSR());
        LOG_ERROR(Core, "Failed to execute code at 0x%08x. Error code: %d Memory at 0x00100004 0x%08x", start_address, err, Memory::Read32(0x100004));
    }
    AddTicks(num_instructions);
}

void ARM_Unicorn::ResetContext(Core::ThreadContext& context, u32 stack_top, u32 entry_point, u32 arg) {
    memset(&context, 0, sizeof(Core::ThreadContext));

    context.cpu_registers[0] = arg;
    context.pc = entry_point;
    context.sp = stack_top;
    context.cpsr = 0x1F; // Usermode
}

void ARM_Unicorn::SaveContext(Core::ThreadContext& ctx) {
    //    memcpy(ctx.cpu_registers, state->Reg.data(), sizeof(ctx.cpu_registers));
    for (int i = 0; i < 13; ++i) {
        ctx.cpu_registers[i] = GetReg(i);
    }

//    memcpy(ctx.fpu_registers, state->ExtReg.data(), sizeof(ctx.fpu_registers));
    // TODO

//    ctx.sp = state->Reg[13];
    ctx.sp = GetReg(13);
//    ctx.lr = state->Reg[14];
    ctx.lr = GetReg(14);
//    ctx.pc = state->Reg[15];
    ctx.lr = GetReg(15);
//    ctx.cpsr = state->Cpsr;

//    ctx.fpscr = state->VFP[1];
//    ctx.fpexc = state->VFP[2];
}

void ARM_Unicorn::LoadContext(const Core::ThreadContext& ctx) {
//    memcpy(state->Reg.data(), ctx.cpu_registers, sizeof(ctx.cpu_registers));
    for (int i = 0; i < 13; ++i) {
        SetReg(0, ctx.cpu_registers[i]);
    }
//    memcpy(state->ExtReg.data(), ctx.fpu_registers, sizeof(ctx.fpu_registers));

//    state->Reg[13] = ctx.sp;
    SetReg(13, ctx.sp);
//    state->Reg[14] = ctx.lr;
    SetReg(14, ctx.lr);
//    state->Reg[15] = ctx.pc;
    SetReg(15, ctx.pc);
//    state->Cpsr = ctx.cpsr;

//    state->VFP[1] = ctx.fpscr;
//    state->VFP[2] = ctx.fpexc;
}

void ARM_Unicorn::PrepareReschedule() {
    this->NumInstrsToExecute = 0;
}
