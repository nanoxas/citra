// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <cstddef>
#include <map>

#include "common/assert.h"
#include "common/x64/abi.h"

#include "core/arm/jit_x64/reg_alloc.h"

namespace JitX64 {

const std::map<Gen::X64Reg, size_t> RegAlloc::x64_reg_to_index = {
    // Ordered such that the caller saved registers are on top,
    // and registers often flushed at the bottom.
    { Gen::R10, 0 },
    { Gen::R11, 1 },
    { Gen::RBX, 2 },
    { Gen::RBP, 3 },
    { Gen::R12, 4 },
    { Gen::R13, 5 },
    { Gen::R14, 6 },
    { Gen::RAX, 7 },
    { Gen::R9,  8 },
    { Gen::R8,  9 },
    { Gen::RSI, 10 },
    { Gen::RDX, 11 },
    { Gen::RDI, 12 },
    { Gen::RCX, 13 },
    { Gen::RSP, 14 },
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
        const ArmReg arm_reg = static_cast<ArmReg>(i);
        arm_gpr[i] = { arm_reg, MJitStateCpuReg(arm_reg), false };
    }

    for (const auto& entry : x64_reg_to_index) {
        const Gen::X64Reg x64_reg = entry.first;
        const size_t i = entry.second;
        x64_gpr[i] = { x64_reg, false, X64State::State::Free, ArmReg::INVALID_REG };
    }
}

void RegAlloc::FlushX64(Gen::X64Reg x64_reg) {
    X64State& x64_state = GetState(x64_reg);

    ASSERT(!x64_state.locked);

    switch (x64_state.state) {
    case X64State::State::Free:
    case X64State::State::Temp:
        x64_state.state = X64State::State::Free;
        break;
    case X64State::State::CleanArmReg: {
        ArmState& arm_state = GetState(x64_state.arm_reg);
        AssertStatesConsistent(x64_state, arm_state);

        // We ignore the value in the x64 register since it's not dirty.
        x64_state.state = X64State::State::Free;
        arm_state.location = MJitStateCpuReg(x64_state.arm_reg);
        break;
    }
    case X64State::State::DirtyArmReg: {
        AssertStatesConsistent(x64_state, GetState(x64_state.arm_reg));

        // Flush the value in the x64 register back into ARMul_State since it's dirty.
        FlushArm(x64_state.arm_reg);
        break;
    }
    default:
        UNREACHABLE();
        break;
    }

    ASSERT(x64_state.state == X64State::State::Free);
    ASSERT(!x64_state.locked);
}

void RegAlloc::LockX64(Gen::X64Reg x64_reg) {
    X64State& x64_state = GetState(x64_reg);

    ASSERT(!x64_state.locked && x64_state.state == X64State::State::Free);

    x64_state.locked = true;
    x64_state.state = X64State::State::UserManuallyLocked;
}

void RegAlloc::UnlockX64(Gen::X64Reg x64_reg) {
    X64State& x64_state = GetState(x64_reg);

    ASSERT(x64_state.locked && x64_state.state == X64State::State::UserManuallyLocked);

    x64_state.locked = false;
    x64_state.state = X64State::State::Free;
}

void RegAlloc::FlushArm(ArmReg arm_reg) {
    ArmState& arm_state = GetState(arm_reg);

    ASSERT(!arm_state.locked);

    if (!arm_state.IsInX64Register())
        return; // Nothing to do

    X64State& x64_state = GetState(GetX64For(arm_reg));

    ASSERT(!x64_state.locked);

    if (x64_state.state == X64State::State::DirtyArmReg) {
        code->MOV(32, MJitStateCpuReg(arm_reg), R(x64_state.x64_reg));
    }

    x64_state.state = X64State::State::Free;
    arm_state.location = MJitStateCpuReg(arm_reg);
}

Gen::OpArg RegAlloc::LockArmForRead(ArmReg arm_reg) {
    ASSERT(arm_reg != ArmReg::PC); // Not valid for R15 (cannot read from it)

    ArmState& arm_state = GetState(arm_reg);

    if (arm_state.IsInX64Register()) {
        X64State& x64_state = GetState(GetX64For(arm_reg));
        ASSERT(!x64_state.locked);
        x64_state.locked = true;
    }

    ASSERT(!arm_state.locked);
    arm_state.locked = true;

    return arm_state.location;
}

Gen::OpArg RegAlloc::LockArmForWrite(ArmReg arm_reg) {
    // Valid for R15 (write-only)

    ArmState& arm_state = GetState(arm_reg);

    if (arm_state.IsInX64Register()) {
        X64State& x64_state = GetState(GetX64For(arm_reg));
        ASSERT(!x64_state.locked);
        x64_state.locked = true;
        x64_state.state = X64State::State::DirtyArmReg;
    }

    ASSERT(!arm_state.locked);
    arm_state.locked = true;

    return arm_state.location;
}

Gen::X64Reg RegAlloc::BindArmToX64(ArmReg arm_reg, bool load) {
    ArmState& arm_state = GetState(arm_reg);

    ASSERT(!arm_state.locked);

    if (arm_state.IsInX64Register()) {
        X64State& x64_state = GetState(GetX64For(arm_reg));
        ASSERT(!x64_state.locked);
        arm_state.locked = true;
        x64_state.locked = true;
        return x64_state.x64_reg;
    }

    const Gen::X64Reg x64_reg = AllocReg();
    X64State& x64_state = GetState(x64_reg);

    ASSERT(!x64_state.locked && x64_state.state == X64State::State::Free);

    x64_state.locked = true;
    x64_state.arm_reg = arm_reg;
    x64_state.state = X64State::State::CleanArmReg;

    if (load)
        code->MOV(32, R(x64_reg), MJitStateCpuReg(arm_reg));

    arm_state.locked = true;
    arm_state.location = R(x64_reg);

    return x64_reg;
}

Gen::X64Reg RegAlloc::BindArmForRead(ArmReg arm_reg) {
    ASSERT(arm_reg != ArmReg::PC); // Not valid for R15 (cannot read from it)

    return BindArmToX64(arm_reg, true);
}

Gen::X64Reg RegAlloc::BindArmForWrite(ArmReg arm_reg) {
    // Valid for R15 (we're not reading from it)

    const Gen::X64Reg x64_reg = BindArmToX64(arm_reg, false);

    MarkDirty(arm_reg);

    return x64_reg;
}

void RegAlloc::UnlockArm(ArmReg arm_reg) {
    ArmState& arm_state = GetState(arm_reg);

    if (arm_state.IsInX64Register()) {
        X64State& x64_state = GetState(GetX64For(arm_reg));
        ASSERT(x64_state.locked);
        x64_state.locked = false;
    }

    ASSERT(arm_state.locked);
    arm_state.locked = false;
}

void RegAlloc::MarkDirty(ArmReg arm_reg) {
    const ArmState& arm_state = GetState(arm_reg);

    ASSERT(arm_state.locked && arm_state.IsInX64Register());

    X64State& x64_state = GetState(GetX64For(arm_reg));

    ASSERT(x64_state.locked);

    x64_state.state = X64State::State::DirtyArmReg;
}

void RegAlloc::FlushEverything() {
    for (const X64State& x64_state : x64_gpr) {
        ASSERT(!x64_state.locked);
        FlushX64(x64_state.x64_reg);
        ASSERT(x64_state.state == X64State::State::Free);
    }
}

void RegAlloc::FlushABICallerSaved() {
    for (const X64State& x64_state : x64_gpr) {
        if (!ABI_ALL_CALLER_SAVED[x64_state.x64_reg])
            continue;
        if (x64_state.state != X64State::State::UserManuallyLocked) {
            ASSERT(!x64_state.locked);
            FlushX64(x64_state.x64_reg);
            ASSERT(x64_state.state == X64State::State::Free);
        } else {
            ASSERT(x64_state.locked);
        }
    }

    ASSERT(!ABI_ALL_CALLER_SAVED[JitStateReg()]);
}

RegAlloc::X64State& RegAlloc::GetState(Gen::X64Reg x64_reg) {
    ASSERT(x64_reg_to_index.find(x64_reg) != x64_reg_to_index.end());
    const size_t x64_index = x64_reg_to_index.at(x64_reg);
    ASSERT(x64_gpr[x64_index].x64_reg == x64_reg);
    return x64_gpr[x64_index];
}

const RegAlloc::X64State& RegAlloc::GetState(Gen::X64Reg x64_reg) const {
    ASSERT(x64_reg_to_index.find(x64_reg) != x64_reg_to_index.end());
    const size_t x64_index = x64_reg_to_index.at(x64_reg);
    ASSERT(x64_gpr[x64_index].x64_reg == x64_reg);
    return x64_gpr[x64_index];
}

RegAlloc::ArmState& RegAlloc::GetState(ArmReg arm_reg) {
    ASSERT(IsValidArmReg(arm_reg));
    const size_t arm_index = static_cast<size_t>(arm_reg);
    ASSERT(arm_gpr[arm_index].arm_reg == arm_reg);
    return arm_gpr[arm_index];
}

const RegAlloc::ArmState& RegAlloc::GetState(ArmReg arm_reg) const {
    ASSERT(IsValidArmReg(arm_reg));
    const size_t arm_index = static_cast<size_t>(arm_reg);
    ASSERT(arm_gpr[arm_index].arm_reg == arm_reg);
    return arm_gpr[arm_index];
}

void RegAlloc::AssertStatesConsistent(const X64State& x64_state, const ArmState& arm_state) const {
    ASSERT(arm_state.locked == x64_state.locked);

    ASSERT(arm_state.IsInX64Register());
    ASSERT(arm_state.location.GetSimpleReg() == x64_state.x64_reg);

    ASSERT(x64_state.arm_reg == arm_state.arm_reg);
    ASSERT(x64_state.state == X64State::State::CleanArmReg || x64_state.state == X64State::State::DirtyArmReg);
}

Gen::X64Reg RegAlloc::GetX64For(ArmReg arm_reg) const {
    const ArmState& arm_state = GetState(arm_reg);

    ASSERT(arm_state.IsInX64Register());

    const Gen::X64Reg x64_reg = arm_state.location.GetSimpleReg();

    AssertStatesConsistent(GetState(x64_reg), arm_state);

    return x64_reg;
}

Gen::X64Reg RegAlloc::AllocTemp() {
    const Gen::X64Reg x64_reg = AllocReg();
    X64State& x64_state = GetState(x64_reg);

    ASSERT(!x64_state.locked && x64_state.state == X64State::State::Free);

    x64_state.locked = true;
    x64_state.state = X64State::State::Temp;

    return x64_reg;
}

void RegAlloc::UnlockTemp(Gen::X64Reg x64_reg) {
    X64State& x64_state = GetState(x64_reg);

    ASSERT(x64_state.locked);
    ASSERT(x64_state.state == X64State::State::Temp);

    x64_state.locked = false;
    x64_state.state = X64State::State::Free;
}

void RegAlloc::AssertNoLocked() const {
    ASSERT(std::all_of(arm_gpr.begin(), arm_gpr.end(), [&](const ArmState& arm_state) {
        if (arm_state.IsInX64Register()) {
            const X64State& x64_state = GetState(GetX64For(arm_state.arm_reg));
            AssertStatesConsistent(x64_state, arm_state);
        }
        return !arm_state.locked;
    }));

    ASSERT(std::all_of(x64_gpr.begin(), x64_gpr.end(), [](const X64State& x64_state) {
        ASSERT(x64_state.state != X64State::State::Temp); // Temp is never unlocked.
        return !x64_state.locked;
    }));
}

Gen::X64Reg RegAlloc::AllocReg() {
    // TODO: Improve with an actual register allocator as this is terrible.

    // First check to see if there anything free.
    auto free_x64 = std::find_if(x64_gpr.begin(), x64_gpr.end(), [](const auto& x64_state) {
        return !x64_state.locked && x64_state.state == X64State::State::Free;
    });

    if (free_x64 != x64_gpr.end()) {
        return free_x64->x64_reg;
    }

    // Otherwise flush something.
    auto flushable_x64 = std::find_if(x64_gpr.begin(), x64_gpr.end(), [](const auto& x64_state) {
        return !x64_state.locked;
    });

    if (flushable_x64 != x64_gpr.end()) {
        FlushX64(flushable_x64->x64_reg);
        return flushable_x64->x64_reg;
    }

    ASSERT_MSG(false, "Ran out of x64 registers");
    UNREACHABLE();
}

}
