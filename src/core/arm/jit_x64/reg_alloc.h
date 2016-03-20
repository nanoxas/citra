// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>

#include "common/common_types.h"
#include "common/common_funcs.h"
#include "common/x64/emitter.h"

#include "core/arm/decoder/decoder.h"
#include "core/arm/jit_x64/common.h"

namespace JitX64 {

// This is the register allocator
// TODO: Better algorithm required, ideally something that does reigster usage lookahead.
//       (Designed so it should be simple to implement later.)

class RegAlloc final {
private:
    struct ArmState {
        Gen::OpArg location;
        bool locked;
    };

    struct X64State {
        enum class State {
            Free,
            Temp,
            DirtyArmReg,
            CleanArmReg,
            MemoryMap
        };

        bool locked;
        State state;
        ArmReg arm_reg;
    };

    std::array<ArmState, 15> arm_gpr;
    std::array<X64State, 16> x64_gpr;

    Gen::XEmitter* code = nullptr;

public:
    /// Initialise register allocator (call compiling a basic block as it resets internal state)
    void Init(Gen::XEmitter* emitter);

    /// Ensures that the state of that register is State::Free.
    void FlushX64(Gen::X64Reg x64_reg);
    /// Locks a register: Marks it as in-use so it isn't allocated.
    void LockX64(Gen::X64Reg x64_reg);
    /// Unlocks a register: Allows it to be used for allocation again.
    void UnlockX64(Gen::X64Reg x64_reg);

    /// Ensures that this ARM register is not in an x64 register.
    void FlushArm(ArmReg arm_reg);
    /// Locks an ARM register so it doesn't move.
    void LockArm(ArmReg arm_reg);
    /// Allocates a x64 register for an ARM register and loads its value into it if necessary.
    Gen::X64Reg BindAndLockArm(ArmReg arm_reg);
    /// Allocates a x64 register for an ARM register but does not load a value.
    Gen::X64Reg BindNoLoadAndLockArm(ArmReg arm_reg);
    /// Unlock ARM register so the register is free to move and the underlying x64 register is available (if any).
    void UnlockArm(ArmReg arm_reg);
    /// Flush all ARM registers.
    void FlushAllArm();
    /// Marks an ARM register as dirty. If you don't mark something as dirty it won't be flushed.
    void MarkDirty(ArmReg arm_reg);

    /// Flush absolutely everything.
    void FlushEverything();

    /// Gets the x64 register which corresponds to that ARM register. (ASSERTS IF NOT IN A x64 REG OR NOT LOCKED!)
    Gen::X64Reg GetX64For(ArmReg arm_reg);
    /// Gets the current location of this ARM register. (ASSERTS IF NOT LOCKED!)
    Gen::OpArg ArmR(ArmReg arm_reg);

    /// Allocates a temporary register
    Gen::X64Reg AllocAndLockTemp();
    /// Releases a temporary register
    void UnlockTemp(Gen::X64Reg x64_reg);

    /// Gets the x64 register with the address of the memory map in it. Allocates one if one doesn't already exist.
    Gen::X64Reg BindAndLockMemoryMap();
    /// Releases the memory map register.
    void UnlockMemoryMap(Gen::X64Reg x64_reg);

    /// Returns the register in which the JitState pointer is stored.
    Gen::X64Reg JitStateReg();

private:
    /// INTERNAL: Allocates a register that is free. Flushes registers that are not locked if necessary.
    Gen::X64Reg AllocReg();
    /// INTERNAL: Implementation of BindNoLoadAndLockArm and BindAndLockArm
    Gen::X64Reg BindMaybeLoadAndLockArm(ArmReg arm_reg, bool load);
};

}
