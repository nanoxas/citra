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
         * Where is the current value of this register? There are two cases:
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
     * Temp (locked must be true):     This x64 reg is being used as a temporary in a calculation.
     * DirtyArmReg (arm_reg is valid): This x64 reg is bound to an ARM reg.
     *                                 It is marked as dirty (value has changed).
     *                                 This value MUST be flushed back to memory.
     * CleanArmReg (arm_reg is valid): This x64 reg is bound to an ARM reg.
     *                                 It hasn't been written to (i.e.: value is still the same as the in-memory version).
     *                                 This value WILL NOT be flushed back to memory.
     * MemoryMap:                      This value holds a pointer to the ARM page table (currently unimplemented).
     * UserManuallyLocked:             User has called LockX64 on this register. User must call UnlockX64 to unlock.
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

    std::array<ArmState, 16> arm_gpr;
    std::array<X64State, 16> x64_gpr;

    Gen::XEmitter* code = nullptr;

public:
    RegAlloc() { Init(nullptr); }

    /// Initialise register allocator (call before compiling a basic block as it resets internal state)
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
     * Locks an ARM register so it doesn't move; ARM reg may either be in a x64 reg or in memory.
     * We're going to read from it only.
     * Call UnlockArm when done.
     */
    Gen::OpArg LockArmForRead(ArmReg arm_reg);
    /**
     * Locks an ARM register so it doesn't move; ARM reg may either be in a x64 reg or in memory.
     * We're going to read and/or write to it.
     * Call UnlockArm when done.
     */
    Gen::OpArg LockArmForReadWrite(ArmReg arm_reg) {
        Gen::OpArg ret = LockArmForRead(arm_reg);
        if (IsBoundToX64(arm_reg)) {
            MarkDirty(arm_reg);
        }
        return ret;
    }
    /**
     * Locks an ARM register so it doesn't move; ARM reg may either be in a x64 reg or in memory.
     * We're going to write to it only.
     * Call UnlockArm when done.
     */
    Gen::OpArg LockArmForWrite(ArmReg arm_reg);

    /**
     * Binds an ARM register to a x64 register.
     * We're going to read from it only.
     * Call UnlockArm when done.
     */
    Gen::X64Reg BindArmForRead(ArmReg arm_reg);
    /**
     * Binds an ARM register to a x64 register.
     * We're going to read and/or write to it.
     * Call UnlockArm when done.
     */
    Gen::X64Reg BindArmForReadWrite(ArmReg arm_reg) {
        Gen::X64Reg ret = BindArmForRead(arm_reg);
        MarkDirty(arm_reg);
        return ret;
    }
    /**
     * Binds an ARM register to a x64 register.
     * We're going to write to it only.
     * Call UnlockArm when done.
     */
    Gen::X64Reg BindArmForWrite(ArmReg arm_reg);

    /// Unlock ARM register.
    void UnlockArm(ArmReg arm_reg);

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

    // Flush:

    /**
     * Flush absolutely everything.
     * You MUST always flush everything:
     * - just before a branch occurs
     * - just before calling into the interpreter
     * - just before calling a host function
     * - just before returning to the dispatcher
     * - just before jumping to a new BB
     * If unsure, flush. (Only cost is performance.)
     */
    void FlushEverything();

    /**
     * Flush only those registers that are caller-saved in the ABI.
     * All registers must be unlocked except those locked by LockX64.
     * (We assume you know what you're doing if you've manually locked registers.)
     */
    void FlushABICallerSaved();

    // Debug:

    void AssertNoLocked();

private:
    /// INTERNAL: Gets the x64 register this ArmReg is currently bound to.
    Gen::X64Reg GetX64For(ArmReg arm_reg);
    /// INTERNAL: Ensures that this ARM register is not in an x64 register.
    void FlushArm(ArmReg arm_reg);
    /// INTERNAL: Is this ARM register currently in an x64 register?
    bool IsBoundToX64(ArmReg arm_reg);
    /// INTERNAL: Marks register as dirty. Ensures that it is written back to memory if it's in a x64 register.
    void MarkDirty(ArmReg arm_reg);
    /// INTERNAL: Allocates a register that is free. Flushes registers that are not locked if necessary.
    Gen::X64Reg AllocReg();
    /// INTERNAL: Binds an ARM register to an X64 register. Retrieves binding if already bound.
    Gen::X64Reg BindArmToX64(ArmReg arm_reg, bool load);
};

}
