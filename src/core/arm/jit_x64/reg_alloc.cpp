// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <cstddef>
#include <map>

#include "common/assert.h"
#include "common/x64/abi.h"

#include "core/arm/jit_x64/reg_alloc.h"

namespace JitX64 {

static const std::map<Gen::X64Reg, size_t> x64_reg_to_index = {
    { Gen::RAX, 0 },
    { Gen::RBX, 1 },
    { Gen::RCX, 2 },
    { Gen::RDX, 3 },
    { Gen::RBP, 4 },
    { Gen::RSI, 5 },
    { Gen::RDI, 6 },
    { Gen::RSP, 7 },
    { Gen::R8,  8 },
    { Gen::R9,  9 },
    { Gen::R10, 10 },
    { Gen::R11, 11 },
    { Gen::R12, 12 },
    { Gen::R13, 13 },
    { Gen::R14, 14 },
};

constexpr Gen::X64Reg jit_state_reg = Gen::R15;

Gen::X64Reg RegAlloc::JitStateReg() const {
    return jit_state_reg;
}

static Gen::OpArg MJitStateCpuReg(ArmReg arm_reg) {
    // The below pointer arithmetic assumes the following:
    static_assert(std::is_same<decltype(JitState::cpu_state), ARMul_State>::value, "JitState::cpu_state must be ARMul_State");
    static_assert(std::is_same<decltype(ARMul_State::Reg), std::array<u32, 16>>::value, "ARMul_State::Reg must be std::array<u32, 16>");

    ASSERT(IsValidArmReg(arm_reg));

    return Gen::MDisp(jit_state_reg, offsetof(JitState, cpu_state) + offsetof(ARMul_State, Reg) + static_cast<unsigned>(arm_reg) * sizeof(u32));
}

void RegAlloc::Init(Gen::XEmitter* emitter) {
    code = emitter;

    for (size_t i = 0; i < arm_gpr.size(); i++) {
        arm_gpr[i].locked = false;
        arm_gpr[i].location = MJitStateCpuReg(static_cast<ArmReg>(i));
    }

    for (size_t i = 0; i < x64_gpr.size(); i++) {
        x64_gpr[i].locked = false;
        x64_gpr[i].state = X64State::State::Free;
        x64_gpr[i].arm_reg = ArmReg::INVALID_REG;
    }
}

void RegAlloc::FlushX64(Gen::X64Reg x64_reg) {
    const size_t x64_index = x64_reg_to_index.at(x64_reg);
    X64State& state = x64_gpr[x64_index];

    ASSERT(!state.locked);

    switch (state.state) {
    case X64State::State::Free:
    case X64State::State::Temp:
        state.state = X64State::State::Free;
        break;
    case X64State::State::CleanArmReg: {
        state.state = X64State::State::Free;
        ArmState& arm_state = arm_gpr[static_cast<unsigned>(state.arm_reg)];
        arm_state.location = MJitStateCpuReg(state.arm_reg);
        break;
    }
    case X64State::State::DirtyArmReg: {
        ArmState& arm_state = arm_gpr[static_cast<unsigned>(state.arm_reg)];
        ASSERT(arm_state.location.IsSimpleReg());
        ASSERT(arm_state.location.GetSimpleReg() == x64_reg);
        FlushArm(state.arm_reg);
        ASSERT(state.state == X64State::State::Free);
        break;
    }
    default:
        UNREACHABLE();
        break;
    }

    ASSERT(state.state == X64State::State::Free);
    ASSERT(!state.locked);
}

void RegAlloc::LockX64(Gen::X64Reg x64_reg) {
    X64State& x64_state = x64_gpr[x64_reg_to_index.at(x64_reg)];

    ASSERT(!x64_state.locked);
    ASSERT(x64_state.state == X64State::State::Free);
    x64_state.locked = true;
    x64_state.state = X64State::State::UserManuallyLocked;
}

void RegAlloc::UnlockX64(Gen::X64Reg x64_reg) {
    X64State& x64_state = x64_gpr[x64_reg_to_index.at(x64_reg)];

    ASSERT(x64_state.locked);
    ASSERT(x64_state.state == X64State::State::UserManuallyLocked);
    x64_state.locked = false;
    x64_state.state = X64State::State::Free;
}

void RegAlloc::FlushArm(ArmReg arm_reg) {
    ASSERT(IsValidArmReg(arm_reg));

    ArmState& arm_state = arm_gpr[static_cast<unsigned>(arm_reg)];
    ASSERT(!arm_state.locked);
    if (!arm_state.location.IsSimpleReg()) {
        return;
    }

    Gen::X64Reg x64_reg = GetX64For(arm_reg);
    X64State& x64_state = x64_gpr[x64_reg_to_index.at(x64_reg)];

    ASSERT(!x64_state.locked);
    ASSERT(x64_state.state == X64State::State::CleanArmReg || x64_state.state == X64State::State::DirtyArmReg);
    ASSERT(x64_state.arm_reg == arm_reg);

    if (x64_state.state == X64State::State::DirtyArmReg) {
        code->MOV(32, MJitStateCpuReg(arm_reg), R(x64_reg));
    }
    x64_state.state = X64State::State::Free;
    arm_state.location = MJitStateCpuReg(arm_reg);
}

Gen::OpArg RegAlloc::LockArmForRead(ArmReg arm_reg) {
    ASSERT(IsValidArmReg(arm_reg) && arm_reg != ArmReg::PC); // Not valid for R15 (cannot read from it)

    ArmState& arm_state = arm_gpr[static_cast<unsigned>(arm_reg)];

    ASSERT(!arm_state.locked);
    arm_state.locked = true;

    if (arm_state.location.IsSimpleReg()) {
        Gen::X64Reg x64_reg = arm_state.location.GetSimpleReg();
        X64State& x64_state = x64_gpr[x64_reg_to_index.at(x64_reg)];
        ASSERT(!x64_state.locked);
        ASSERT(x64_state.state == X64State::State::CleanArmReg || x64_state.state == X64State::State::DirtyArmReg);
        ASSERT(x64_state.arm_reg == arm_reg);

        x64_state.locked = true;
    }

    return arm_state.location;
}

Gen::OpArg RegAlloc::LockArmForWrite(ArmReg arm_reg) {
    ASSERT(IsValidArmReg(arm_reg)); // Valid for R15 (write-only)

    ArmState& arm_state = arm_gpr[static_cast<unsigned>(arm_reg)];

    ASSERT(!arm_state.locked);
    arm_state.locked = true;

    if (arm_state.location.IsSimpleReg()) {
        Gen::X64Reg x64_reg = arm_state.location.GetSimpleReg();
        X64State& x64_state = x64_gpr[x64_reg_to_index.at(x64_reg)];
        ASSERT(!x64_state.locked);
        ASSERT(x64_state.state == X64State::State::CleanArmReg || x64_state.state == X64State::State::DirtyArmReg);
        ASSERT(x64_state.arm_reg == arm_reg);

        x64_state.locked = true;
        x64_state.state = X64State::State::DirtyArmReg;
    }

    return arm_state.location;
}

Gen::X64Reg RegAlloc::BindArmToX64(ArmReg arm_reg, bool load) {
    ArmState& arm_state = arm_gpr[static_cast<unsigned>(arm_reg)];

    ASSERT(!arm_state.locked);
    arm_state.locked = true;

    if (arm_state.location.IsSimpleReg()) {
        const Gen::X64Reg x64_reg = arm_state.location.GetSimpleReg();
        X64State& x64_state = x64_gpr[x64_reg_to_index.at(x64_reg)];
        ASSERT(!x64_state.locked);
        ASSERT(x64_state.state == X64State::State::CleanArmReg || x64_state.state == X64State::State::DirtyArmReg);
        ASSERT(x64_state.arm_reg == arm_reg);

        x64_state.locked = true;
        return x64_reg;
    }

    const Gen::X64Reg x64_reg = AllocReg();
    X64State& x64_state = x64_gpr[x64_reg_to_index.at(x64_reg)];
    x64_state.locked = true;
    x64_state.arm_reg = arm_reg;
    x64_state.state = X64State::State::CleanArmReg;

    if (load)
        code->MOV(32, R(x64_reg), MJitStateCpuReg(arm_reg));

    arm_state.location = R(x64_reg);

    return x64_reg;
}

Gen::X64Reg RegAlloc::BindArmForRead(ArmReg arm_reg) {
    ASSERT(IsValidArmReg(arm_reg) && arm_reg != ArmReg::PC); // Not valid for R15 (cannot read from it)

    const Gen::X64Reg x64_reg = BindArmToX64(arm_reg, true);

    return x64_reg;
}

Gen::X64Reg RegAlloc::BindArmForWrite(ArmReg arm_reg) {
    ASSERT(IsValidArmReg(arm_reg)); // Valid for R15 (we're not reading from it)

    const Gen::X64Reg x64_reg = BindArmToX64(arm_reg, false);

    MarkDirty(arm_reg);

    return x64_reg;
}

void RegAlloc::UnlockArm(ArmReg arm_reg) {
    ASSERT(IsValidArmReg(arm_reg));

    ArmState& arm_state = arm_gpr[static_cast<unsigned>(arm_reg)];

    ASSERT(arm_state.locked);
    arm_state.locked = false;

    if (arm_state.location.IsSimpleReg()) {
        const Gen::X64Reg x64_reg = arm_state.location.GetSimpleReg();
        X64State& x64_state = x64_gpr[x64_reg_to_index.at(x64_reg)];
        ASSERT(x64_state.locked);
        ASSERT(x64_state.state == X64State::State::CleanArmReg || x64_state.state == X64State::State::DirtyArmReg);
        ASSERT(x64_state.arm_reg == arm_reg);

        x64_state.locked = false;
    }
}

void RegAlloc::MarkDirty(ArmReg arm_reg) {
    const ArmState& arm_state = arm_gpr[static_cast<unsigned>(arm_reg)];

    ASSERT(arm_state.locked);
    ASSERT(arm_state.location.IsSimpleReg());

    const Gen::X64Reg x64_reg = arm_state.location.GetSimpleReg();
    X64State& x64_state = x64_gpr[x64_reg_to_index.at(x64_reg)];

    ASSERT(x64_state.locked);
    ASSERT(x64_state.state == X64State::State::CleanArmReg || x64_state.state == X64State::State::DirtyArmReg);
    ASSERT(x64_state.arm_reg == arm_reg);

    x64_state.state = X64State::State::DirtyArmReg;
}

void RegAlloc::FlushEverything() {
    for (auto i : x64_reg_to_index) {
        X64State& x64_state = x64_gpr[i.second];
        ASSERT(!x64_state.locked || x64_state.state == X64State::State::UserManuallyLocked);
        FlushX64(i.first);
        ASSERT(x64_state.state == X64State::State::Free);
    }
}

void RegAlloc::FlushABICallerSaved() {
    for (auto i : x64_reg_to_index) {
        X64State& x64_state = x64_gpr[i.second];
        if (x64_state.state != X64State::State::UserManuallyLocked) {
            ASSERT(!x64_state.locked);
            FlushX64(i.first);
            ASSERT(x64_state.state == X64State::State::Free);
        } else {
            ASSERT(x64_state.locked);
        }
    }

    ASSERT(!ABI_ALL_CALLER_SAVED[JitStateReg()]);
}

Gen::X64Reg RegAlloc::GetX64For(ArmReg arm_reg) {
    const ArmState& arm_state = arm_gpr[static_cast<unsigned>(arm_reg)];

    ASSERT(arm_state.location.IsSimpleReg());

    const Gen::X64Reg x64_reg = arm_state.location.GetSimpleReg();
    const X64State& x64_state = x64_gpr[x64_reg_to_index.at(x64_reg)];

    ASSERT(x64_state.state == X64State::State::CleanArmReg || x64_state.state == X64State::State::DirtyArmReg);
    ASSERT(x64_state.arm_reg == arm_reg);

    return x64_reg;
}

bool RegAlloc::IsBoundToX64(ArmReg arm_reg) {
    const ArmState& arm_state = arm_gpr[static_cast<unsigned>(arm_reg)];

    return arm_state.location.IsSimpleReg();
}

Gen::X64Reg RegAlloc::AllocTemp() {
    const Gen::X64Reg x64_reg = AllocReg();
    X64State& x64_state = x64_gpr[x64_reg_to_index.at(x64_reg)];
    x64_state.locked = true;
    x64_state.state = X64State::State::Temp;

    return x64_reg;
}

void RegAlloc::UnlockTemp(Gen::X64Reg x64_reg) {
    X64State& x64_state = x64_gpr[x64_reg_to_index.at(x64_reg)];

    ASSERT(x64_state.locked);
    ASSERT(x64_state.state == X64State::State::Temp);

    x64_state.locked = false;
    x64_state.state = X64State::State::Free;
}

void RegAlloc::AssertNoLocked() {
    for (size_t i = 0; i < arm_gpr.size(); i++) {
        ArmState& arm_state = arm_gpr[i];
        ASSERT(!arm_state.locked);
        if (arm_state.location.IsSimpleReg()) {
            X64State& x64_state = x64_gpr[x64_reg_to_index.at(arm_state.location.GetSimpleReg())];
            ASSERT(x64_state.state == X64State::State::CleanArmReg || x64_state.state == X64State::State::DirtyArmReg);
            ASSERT(x64_state.arm_reg == static_cast<ArmReg>(i));
        }
    }

    for (auto i : x64_reg_to_index) {
        X64State& x64_state = x64_gpr[i.second];
        ASSERT(!x64_state.locked);
    }
}

Gen::X64Reg RegAlloc::AllocReg() {
    // TODO: Improve with an actual register allocator as this is terrible.

    // First check to see if there anything free.
    for (auto i : x64_reg_to_index) {
        X64State& x64_state = x64_gpr[i.second];

        if (x64_state.locked)
            continue;

        ASSERT(x64_state.state != X64State::State::Temp); // This can never happen.

        if (x64_state.state == X64State::State::Free) {
            return i.first;
        }
    }

    // Otherwise flush something.
    for (auto i : x64_reg_to_index) {
        X64State& x64_state = x64_gpr[i.second];

        if (x64_state.locked)
            continue;

        FlushX64(i.first);
        return i.first;
    }

    ASSERT_MSG(false, "Ran out of x64 registers");
    UNREACHABLE();
}

}
