// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/assert.h"

#include "core/arm/jit_x64/jit_x64.h"
#include "core/memory.h"

namespace JitX64 {

using namespace Gen;

void JitX64::CondManager::Init(JitX64* jit_) {
    jit = jit_;
    current_cond = Cond::AL;
    flags_dirty = false;
    current_cond_fixup = {};
}

void JitX64::CondManager::CompileCond(const Cond new_cond) {
    if (current_cond == new_cond && !flags_dirty)
        return;

    if (current_cond != Cond::AL && current_cond != Cond::NV) {
        jit->reg_alloc.FlushEverything();
        jit->reg_alloc.AssertNoLocked();
        ASSERT(current_cond_fixup.ptr);
        jit->code->SetJumpTarget(current_cond_fixup);
        current_cond_fixup.ptr = nullptr;
    }

    if (new_cond != Cond::AL && new_cond != Cond::NV) {
        CCFlags cc;

        switch (new_cond) {
        case Cond::EQ: //z
            jit->code->CMP(8, jit->MJitStateZFlag(), Imm8(0));
            cc = CC_E;
            break;
        case Cond::NE: //!z
            jit->code->CMP(8, jit->MJitStateZFlag(), Imm8(0));
            cc = CC_NE;
            break;
        case Cond::CS: //c
            jit->code->CMP(8, jit->MJitStateCFlag(), Imm8(0));
            cc = CC_E;
            break;
        case Cond::CC: //!c
            jit->code->CMP(8, jit->MJitStateCFlag(), Imm8(0));
            cc = CC_NE;
            break;
        case Cond::MI: //n
            jit->code->CMP(8, jit->MJitStateNFlag(), Imm8(0));
            cc = CC_E;
            break;
        case Cond::PL: //!n
            jit->code->CMP(8, jit->MJitStateNFlag(), Imm8(0));
            cc = CC_NE;
            break;
        case Cond::VS: //v
            jit->code->CMP(8, jit->MJitStateVFlag(), Imm8(0));
            cc = CC_E;
            break;
        case Cond::VC: //!v
            jit->code->CMP(8, jit->MJitStateVFlag(), Imm8(0));
            cc = CC_NE;
            break;
        case Cond::HI: { //c & !z
            const X64Reg tmp = jit->reg_alloc.AllocTemp();
            jit->code->MOVZX(64, 8, tmp, jit->MJitStateZFlag());
            jit->code->CMP(8, jit->MJitStateCFlag(), R(tmp));
            cc = CC_BE;
            jit->reg_alloc.UnlockTemp(tmp);
            break;
        }
        case Cond::LS: { //!c | z
            const X64Reg tmp = jit->reg_alloc.AllocTemp();
            jit->code->MOVZX(64, 8, tmp, jit->MJitStateZFlag());
            jit->code->CMP(8, jit->MJitStateCFlag(), R(tmp));
            cc = CC_A;
            jit->reg_alloc.UnlockTemp(tmp);
            break;
        }
        case Cond::GE: { // n == v
            const X64Reg tmp = jit->reg_alloc.AllocTemp();
            jit->code->MOVZX(64, 8, tmp, jit->MJitStateVFlag());
            jit->code->CMP(8, jit->MJitStateNFlag(), R(tmp));
            cc = CC_NE;
            jit->reg_alloc.UnlockTemp(tmp);
            break;
        }
        case Cond::LT: { // n != v
            const X64Reg tmp = jit->reg_alloc.AllocTemp();
            jit->code->MOVZX(64, 8, tmp, jit->MJitStateVFlag());
            jit->code->CMP(8, jit->MJitStateNFlag(), R(tmp));
            cc = CC_E;
            jit->reg_alloc.UnlockTemp(tmp);
            break;
        }
        case Cond::GT: { // !z & (n == v)
            const X64Reg tmp = jit->reg_alloc.AllocTemp();
            jit->code->MOVZX(64, 8, tmp, jit->MJitStateNFlag());
            jit->code->XOR(8, R(tmp), jit->MJitStateVFlag());
            jit->code->OR(8, R(tmp), jit->MJitStateZFlag());
            jit->code->TEST(8, R(tmp), R(tmp));
            cc = CC_NZ;
            jit->reg_alloc.UnlockTemp(tmp);
            break;
        }
        case Cond::LE: { // z | (n != v)
            X64Reg tmp = jit->reg_alloc.AllocTemp();
            jit->code->MOVZX(64, 8, tmp, jit->MJitStateNFlag());
            jit->code->XOR(8, R(tmp), jit->MJitStateVFlag());
            jit->code->OR(8, R(tmp), jit->MJitStateZFlag());
            jit->code->TEST(8, R(tmp), R(tmp));
            cc = CC_Z;
            jit->reg_alloc.UnlockTemp(tmp);
            break;
        }
        default:
            UNREACHABLE();
            break;
        }

        jit->reg_alloc.FlushEverything();
        jit->reg_alloc.AssertNoLocked();
        this->current_cond_fixup = jit->code->J_CC(cc, true);
    }

    current_cond = new_cond;
    flags_dirty = false;
}

void JitX64::CondManager::Always() {
    CompileCond(Cond::AL);
}

void JitX64::CondManager::FlagsDirty() {
    flags_dirty = true;
}

Cond JitX64::CondManager::CurrentCond() const {
    return current_cond;
}

}
