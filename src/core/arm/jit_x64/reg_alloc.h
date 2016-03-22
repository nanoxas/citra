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
        /**
         * Where is the current value of this register?
         * There are two options:
         * - In an x64 register, in which case location.IsSimpleReg() == true.
         * - In memory in ARMul_State, in which case location == MJitStateCpuReg(arm_reg).
         */
        Gen::OpArg location = { 0, 0, Gen::INVALID_REG, Gen::INVALID_REG };
        bool locked = false;
    };

    /**
     * Possible states of X64State:
     *
     * Free (locked must be false):    This x64 reg is free to be allocated for any purpose.
     * Temp (locked must be true):     This x64 reg is being used as a temporary for a calculation.
     * DirtyArmReg (arm_reg is valid): This x64 reg holds the value of an ARM reg.
     *                                 It's value has been changed since being loaded from memory.
     *                                 This value must be flushed back to memory.
     * CleanArmReg (arm_reg is valid): This x64 reg holds the value of an ARM reg.
     *                                 It's value has not been changed from when it's been loaded from memory.
     *                                 This value may be discarded.
     * MemoryMap:                      This value holds a pointer to the ARM page table (current unimplemented).
     * UserManuallyLocked:             User has called LockX64 on this register. Must call UnlockX64 to unlock.
     */
    struct X64State {
        enum class State {
            Free,
            Temp,
            DirtyArmReg,
            CleanArmReg,
            MemoryMap,
            UserManuallyLocked
        };

        bool locked = false;
        State state = State::Free;
        ArmReg arm_reg = -1; ///< Only holds a valid value when state == DirtyArmReg / CleanArmReg
    };

    std::array<ArmState, 15> arm_gpr;
    std::array<X64State, 16> x64_gpr;

    Gen::XEmitter* code = nullptr;

public:
    RegAlloc() { Init(nullptr); }

    /// Initialise register allocator (call compiling a basic block as it resets internal state)
    void Init(Gen::XEmitter* emitter);

    // Manually load and unlock x64 registers:
    // This is required rarely. The most significant case is when shifting,
    // because shift instructions must use the CL register.

    /// Ensures that the state of that register is State::Free.
    void FlushX64(Gen::X64Reg x64_reg);
    /// Locks a register: Marks it as in-use so it isn't allocated.
    void LockX64(Gen::X64Reg x64_reg);
    /// Unlocks a register: Allows it to be used for allocation again.
    void UnlockX64(Gen::X64Reg x64_reg);

    // Working with ARM registers:

    /**
     * Locks an ARM register so it doesn't move.
     * ARM reg may either be in a x64 reg or in memory.
     * We're going to read from it only. (Use ArmR to do so.)
     * Call UnlockArm when done.
     */
    void LockArm(ArmReg arm_reg);
    /**
     * Locks an ARM register so it doesn't move.
     * ARM reg may either be in a x64 reg or in memory.
     * We're going to read and/or write to it. (Use ArmR to do so.)
     * Call UnlockArm when done.
     */
    void LockAndDirtyArm(ArmReg arm_reg);
    /// Gets the current location of this ARM register. (ASSERTS IF NOT LOCKED!)
    Gen::OpArg ArmR(ArmReg arm_reg);

    /**
     * Allocates a x64 register for an ARM register and ensure it's value is loaded into it.
     * ARM reg is in an x64 reg.
     * We're going to read from it only. (Call MarkDirty if you want to write to it.)
     * Call UnlockArm when done.
     */
    Gen::X64Reg LoadAndLockArm(ArmReg arm_reg);
    /**
     * Allocates a x64 register for an ARM register and doesn't bother loading it's value to it.
     * ARM reg is in an x64 reg.
     * We're going to write to it only. (DO NOT READ, WRITE-ONLY. Also MarkDirty has been called for you.)
     * Call UnlockArm when done.
     */
    Gen::X64Reg WriteOnlyLockArm(ArmReg arm_reg);

    /**
     * Marks an ARM register as dirty.
     * If you don't mark something as dirty it won't be flushed back to memory.
     * May only be called while an ARM register is locked.
     */
    void MarkDirty(ArmReg arm_reg);
    /// Unlock ARM register.
    void UnlockArm(ArmReg arm_reg);

    /// Ensures that this ARM register is not in an x64 register.
    void FlushArm(ArmReg arm_reg);

    /// Flush all ARM registers.
    void FlushAllArm();

    /**
     * Flush absolutely everything.
     * You MUST always flush everything:
     * - just before a branch occurs
     * - just before calling into the interpreter
     * - just before calling a host function
     * - just before returning to the dispatcher
     * - just before jumping to a new BB
     */
    void FlushEverything();

    /// Gets the x64 register which corresponds to that ARM register. (ASSERTS IF NOT IN A x64 REG OR NOT LOCKED!)
    Gen::X64Reg GetX64For(ArmReg arm_reg);

    // Temporaries:

    /// Allocates a temporary register
    Gen::X64Reg AllocTemp();
    /// Releases a temporary register
    void UnlockTemp(Gen::X64Reg x64_reg);

    // Page table:

    /// Gets the x64 register with the address of the memory map in it. Allocates one if one doesn't already exist.
    Gen::X64Reg LoadMemoryMap();
    /// Releases the memory map register.
    void UnlockMemoryMap(Gen::X64Reg x64_reg);

    // JitState pointer:

    /// Returns the register in which the JitState pointer is stored.
    Gen::X64Reg JitStateReg();

    // Debug:

    void AssertNoLocked();

private:
    /// INTERNAL: Allocates a register that is free. Flushes registers that are not locked if necessary.
    Gen::X64Reg AllocReg();
    /// INTERNAL: Binds an ARM register to an X64 register. Retrieves binding if already bound.
    Gen::X64Reg BindArmToX64(ArmReg arm_reg, bool load);
};

}
