// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <unordered_map>
#include <vector>

#include "common/common_types.h"
#include "common/x64/emitter.h"

#include "core/arm/decoder/decoder.h"
#include "core/arm/jit_x64/common.h"
#include "core/arm/jit_x64/reg_alloc.h"

namespace JitX64 {

using CodePtr = u8*;

struct LocationDescriptor {
    u32 arm_pc;
    bool TFlag; ///< Thumb / ARM
    bool EFlag; ///< Big / Little Endian

    bool operator == (const LocationDescriptor& o) const {
        return std::tie(arm_pc, TFlag, EFlag) == std::tie(o.arm_pc, o.TFlag, o.EFlag);
    }
};

struct LocationDescriptorHash {
    size_t operator()(const LocationDescriptor& x) const {
        return std::hash<u64>()((u64)x.arm_pc ^ ((u64)x.TFlag << 32) ^ ((u64)x.EFlag << 33));
    }
};

class JitX64 final : private ArmDecoder::Visitor {
private:
    Gen::XEmitter* code;

    RegAlloc reg_alloc;

    /// ARM pc -> x64 code block
    std::unordered_map<LocationDescriptor, CodePtr, LocationDescriptorHash> basic_blocks;

public:
    JitX64() = delete;
    JitX64(Gen::XEmitter* code_);
    virtual ~JitX64() override {}

    void ClearCache();

    CodePtr GetBB(u32 pc, bool TFlag, bool EFlag);

    /// Returns a pointer to the compiled basic block.
    CodePtr Compile(u32 pc, bool TFlag, bool EFlag);

private:
    LocationDescriptor current;
    unsigned instructions_compiled;
    bool stop_compilation;

    size_t GetInstSize() { return current.TFlag ? 2 : 4; }
    void CompileSingleArmInstruction();
    void CompileSingleThumbInstruction();

    /// Updates the cycle count in JitState and sets instructions_compiled to zero.
    void CompileUpdateCycles();

    /// If a basic_block starting at ARM pc is compiled -> these locations need to be patched
    std::unordered_map<LocationDescriptor, std::vector<CodePtr>, LocationDescriptorHash> patch_jmp_locations;
    /// Update JitState cycle count before calling this function. This function may instead update JitState PC and return to dispatcher.
    void CompileJumpToBB(u32 arm_pc);
    /// Patch missing jumps (fill in after CompileJumpToBB).
    void Patch(LocationDescriptor desc, CodePtr bb);

private:
    /// Convenience functions
    Gen::OpArg MJitStateCycleCount();
    Gen::OpArg MJitStateArmPC();
    Gen::OpArg MJitStateHostReturnRIP();
    Gen::OpArg MJitStateHostReturnRSP();
    Gen::OpArg MJitStateZFlag();
    Gen::OpArg MJitStateCFlag();
    Gen::OpArg MJitStateNFlag();
    Gen::OpArg MJitStateVFlag();
    Gen::OpArg MJitStateExclusiveTag();
    Gen::OpArg MJitStateExclusiveState();

private:
    struct CondManager {
    private:
        JitX64* jit;
        ConditionCode current_cond;
        bool flags_dirty;
        Gen::FixupBranch current_cond_fixup;
    public:
        void Init(JitX64* jit_);
        void CompileCond(ConditionCode cond);
        void Always();
        void FlagsDirty();
        ConditionCode CurrentCond();
    } cond_manager;

private:
    void CompileInterpretInstruction();

    // Barrier instructions
    virtual void DMB() override;
    virtual void DSB() override;
    virtual void ISB() override;

    // Branch instructions
    virtual void B(Cond cond, ArmImm24 imm24) override;
    virtual void BL(Cond cond, ArmImm24 imm24) override;
    virtual void BLX_imm(bool H, ArmImm24 imm24) override;
    virtual void BLX_reg(Cond cond, ArmReg Rm) override;
    virtual void BX(Cond cond, ArmReg Rm) override;
    virtual void BXJ(Cond cond, ArmReg Rm) override;

    // Coprocessor instructions
    virtual void CDP() override;
    virtual void LDC() override;
    virtual void MCR() override;
    virtual void MCRR() override;
    virtual void MRC() override;
    virtual void MRRC() override;
    virtual void STC() override;

    // Data processing instructions
    virtual void ADC_imm(Cond cond, bool S, ArmReg Rn, ArmReg Rd, int rotate, ArmImm8 imm8) override;
    virtual void ADC_reg(Cond cond, bool S, ArmReg Rn, ArmReg Rd, ArmImm5 imm5, ShiftType shift, ArmReg Rm) override;
    virtual void ADC_rsr(Cond cond, bool S, ArmReg Rn, ArmReg Rd, ArmReg Rs, ShiftType shift, ArmReg Rm) override;
    virtual void ADD_imm(Cond cond, bool S, ArmReg Rn, ArmReg Rd, int rotate, ArmImm8 imm8) override;
    virtual void ADD_reg(Cond cond, bool S, ArmReg Rn, ArmReg Rd, ArmImm5 imm5, ShiftType shift, ArmReg Rm) override;
    virtual void ADD_rsr(Cond cond, bool S, ArmReg Rn, ArmReg Rd, ArmReg Rs, ShiftType shift, ArmReg Rm) override;
    virtual void AND_imm(Cond cond, bool S, ArmReg Rn, ArmReg Rd, int rotate, ArmImm8 imm8) override;
    virtual void AND_reg(Cond cond, bool S, ArmReg Rn, ArmReg Rd, ArmImm5 imm5, ShiftType shift, ArmReg Rm) override;
    virtual void AND_rsr(Cond cond, bool S, ArmReg Rn, ArmReg Rd, ArmReg Rs, ShiftType shift, ArmReg Rm) override;
    virtual void BIC_imm(Cond cond, bool S, ArmReg Rn, ArmReg Rd, int rotate, ArmImm8 imm8) override;
    virtual void BIC_reg(Cond cond, bool S, ArmReg Rn, ArmReg Rd, ArmImm5 imm5, ShiftType shift, ArmReg Rm) override;
    virtual void BIC_rsr(Cond cond, bool S, ArmReg Rn, ArmReg Rd, ArmReg Rs, ShiftType shift, ArmReg Rm) override;
    virtual void CMN_imm(Cond cond,         ArmReg Rn,            int rotate, ArmImm8 imm8) override;
    virtual void CMN_reg(Cond cond,         ArmReg Rn,            ArmImm5 imm5, ShiftType shift, ArmReg Rm) override;
    virtual void CMN_rsr(Cond cond,         ArmReg Rn,            ArmReg Rs, ShiftType shift, ArmReg Rm) override;
    virtual void CMP_imm(Cond cond,         ArmReg Rn,            int rotate, ArmImm8 imm8) override;
    virtual void CMP_reg(Cond cond,         ArmReg Rn,            ArmImm5 imm5, ShiftType shift, ArmReg Rm) override;
    virtual void CMP_rsr(Cond cond,         ArmReg Rn,            ArmReg Rs, ShiftType shift, ArmReg Rm) override;
    virtual void EOR_imm(Cond cond, bool S, ArmReg Rn, ArmReg Rd, int rotate, ArmImm8 imm8) override;
    virtual void EOR_reg(Cond cond, bool S, ArmReg Rn, ArmReg Rd, ArmImm5 imm5, ShiftType shift, ArmReg Rm) override;
    virtual void EOR_rsr(Cond cond, bool S, ArmReg Rn, ArmReg Rd, ArmReg Rs, ShiftType shift, ArmReg Rm) override;
    virtual void MOV_imm(Cond cond, bool S,            ArmReg Rd, int rotate, ArmImm8 imm8) override;
    virtual void MOV_reg(Cond cond, bool S,            ArmReg Rd, ArmImm5 imm5, ShiftType shift, ArmReg Rm) override;
    virtual void MOV_rsr(Cond cond, bool S,            ArmReg Rd, ArmReg Rs, ShiftType shift, ArmReg Rm) override;
    virtual void MVN_imm(Cond cond, bool S,            ArmReg Rd, int rotate, ArmImm8 imm8) override;
    virtual void MVN_reg(Cond cond, bool S,            ArmReg Rd, ArmImm5 imm5, ShiftType shift, ArmReg Rm) override;
    virtual void MVN_rsr(Cond cond, bool S,            ArmReg Rd, ArmReg Rs, ShiftType shift, ArmReg Rm) override;
    virtual void ORR_imm(Cond cond, bool S, ArmReg Rn, ArmReg Rd, int rotate, ArmImm8 imm8) override;
    virtual void ORR_reg(Cond cond, bool S, ArmReg Rn, ArmReg Rd, ArmImm5 imm5, ShiftType shift, ArmReg Rm) override;
    virtual void ORR_rsr(Cond cond, bool S, ArmReg Rn, ArmReg Rd, ArmReg Rs, ShiftType shift, ArmReg Rm) override;
    virtual void RSB_imm(Cond cond, bool S, ArmReg Rn, ArmReg Rd, int rotate, ArmImm8 imm8) override;
    virtual void RSB_reg(Cond cond, bool S, ArmReg Rn, ArmReg Rd, ArmImm5 imm5, ShiftType shift, ArmReg Rm) override;
    virtual void RSB_rsr(Cond cond, bool S, ArmReg Rn, ArmReg Rd, ArmReg Rs, ShiftType shift, ArmReg Rm) override;
    virtual void RSC_imm(Cond cond, bool S, ArmReg Rn, ArmReg Rd, int rotate, ArmImm8 imm8) override;
    virtual void RSC_reg(Cond cond, bool S, ArmReg Rn, ArmReg Rd, ArmImm5 imm5, ShiftType shift, ArmReg Rm) override;
    virtual void RSC_rsr(Cond cond, bool S, ArmReg Rn, ArmReg Rd, ArmReg Rs, ShiftType shift, ArmReg Rm) override;
    virtual void SBC_imm(Cond cond, bool S, ArmReg Rn, ArmReg Rd, int rotate, ArmImm8 imm8) override;
    virtual void SBC_reg(Cond cond, bool S, ArmReg Rn, ArmReg Rd, ArmImm5 imm5, ShiftType shift, ArmReg Rm) override;
    virtual void SBC_rsr(Cond cond, bool S, ArmReg Rn, ArmReg Rd, ArmReg Rs, ShiftType shift, ArmReg Rm) override;
    virtual void SUB_imm(Cond cond, bool S, ArmReg Rn, ArmReg Rd, int rotate, ArmImm8 imm8) override;
    virtual void SUB_reg(Cond cond, bool S, ArmReg Rn, ArmReg Rd, ArmImm5 imm5, ShiftType shift, ArmReg Rm) override;
    virtual void SUB_rsr(Cond cond, bool S, ArmReg Rn, ArmReg Rd, ArmReg Rs, ShiftType shift, ArmReg Rm) override;
    virtual void TEQ_imm(Cond cond,         ArmReg Rn,            int rotate, ArmImm8 imm8) override;
    virtual void TEQ_reg(Cond cond,         ArmReg Rn,            ArmImm5 imm5, ShiftType shift, ArmReg Rm) override;
    virtual void TEQ_rsr(Cond cond,         ArmReg Rn,            ArmReg Rs, ShiftType shift, ArmReg Rm) override;
    virtual void TST_imm(Cond cond,         ArmReg Rn,            int rotate, ArmImm8 imm8) override;
    virtual void TST_reg(Cond cond,         ArmReg Rn,            ArmImm5 imm5, ShiftType shift, ArmReg Rm) override;
    virtual void TST_rsr(Cond cond,         ArmReg Rn,            ArmReg Rs, ShiftType shift, ArmReg Rm) override;

    // Exception generation instructions
    virtual void BKPT() override;
    virtual void HVC() override;
    virtual void SMC() override;
    virtual void SVC() override;
    virtual void UDF() override;

    // Extension functions
    virtual void SXTAB() override;
    virtual void SXTAB16() override;
    virtual void SXTAH() override;
    virtual void SXTB() override;
    virtual void SXTB16() override;
    virtual void SXTH() override;
    virtual void UXTAB() override;
    virtual void UXTAB16() override;
    virtual void UXTAH() override;
    virtual void UXTB() override;
    virtual void UXTB16() override;
    virtual void UXTH() override;

    // Hint instructions
    virtual void DBG() override;
    virtual void PLD() override;
    virtual void PLI() override;

    // Load/Store instructions
    virtual void LDR_imm() override;
    virtual void LDR_reg() override;
    virtual void LDRB_imm() override;
    virtual void LDRB_reg() override;
    virtual void LDRBT() override;
    virtual void LDRD_imm() override;
    virtual void LDRD_reg() override;
    virtual void LDRH_imm() override;
    virtual void LDRH_reg() override;
    virtual void LDRHT() override;
    virtual void LDRSB_imm() override;
    virtual void LDRSB_reg() override;
    virtual void LDRSBT() override;
    virtual void LDRSH_imm() override;
    virtual void LDRSH_reg() override;
    virtual void LDRSHT() override;
    virtual void LDRT() override;
    virtual void STR_imm() override;
    virtual void STR_reg() override;
    virtual void STRB_imm() override;
    virtual void STRB_reg() override;
    virtual void STRBT() override;
    virtual void STRD_imm() override;
    virtual void STRD_reg() override;
    virtual void STRH_imm() override;
    virtual void STRH_reg() override;
    virtual void STRHT() override;
    virtual void STRT() override;

    // Load/Store multiple instructions
    virtual void LDM() override;
    virtual void STM() override;

    // Miscellaneous instructions
    virtual void CLZ() override;
    virtual void ERET() override;
    virtual void NOP() override;
    virtual void SEL() override;

    // Unsigned sum of absolute difference functions
    virtual void USAD8() override;
    virtual void USADA8() override;

    // Packing instructions
    virtual void PKH() override;

    // Reversal instructions
    virtual void RBIT() override;
    virtual void REV() override;
    virtual void REV16() override;
    virtual void REVSH() override;

    // Saturation instructions
    virtual void SSAT() override;
    virtual void SSAT16() override;
    virtual void USAT() override;
    virtual void USAT16() override;

    // Multiply (Normal) instructions
    virtual void MLA() override;
    virtual void MLS() override;
    virtual void MUL() override;

    // Multiply (Long) instructions
    virtual void SMLAL() override;
    virtual void SMULL() override;
    virtual void UMAAL() override;
    virtual void UMLAL() override;
    virtual void UMULL() override;

    // Multiply (Halfword) instructions
    virtual void SMLALxy() override;
    virtual void SMLAxy() override;
    virtual void SMULxy() override;

    // Multiply (word by halfword) instructions
    virtual void SMLAWy() override;
    virtual void SMULWy() override;

    // Multiply (Most significant word) instructions
    virtual void SMMLA() override;
    virtual void SMMLS() override;
    virtual void SMMUL() override;

    // Multiply (Dual) instructions
    virtual void SMLAD() override;
    virtual void SMLALD() override;
    virtual void SMLSD() override;
    virtual void SMLSLD() override;
    virtual void SMUAD() override;
    virtual void SMUSD() override;

    // Parallel Add/Subtract (Modulo arithmetic) instructions
    virtual void SADD8() override;
    virtual void SADD16() override;
    virtual void SASX() override;
    virtual void SSAX() override;
    virtual void SSUB8() override;
    virtual void SSUB16() override;
    virtual void UADD8() override;
    virtual void UADD16() override;
    virtual void UASX() override;
    virtual void USAX() override;
    virtual void USUB8() override;
    virtual void USUB16() override;

    // Parallel Add/Subtract (Saturating) instructions
    virtual void QADD8() override;
    virtual void QADD16() override;
    virtual void QASX() override;
    virtual void QSAX() override;
    virtual void QSUB8() override;
    virtual void QSUB16() override;
    virtual void UQADD8() override;
    virtual void UQADD16() override;
    virtual void UQASX() override;
    virtual void UQSAX() override;
    virtual void UQSUB8() override;
    virtual void UQSUB16() override;

    // Parallel Add/Subtract (Halving) instructions
    virtual void SHADD8() override;
    virtual void SHADD16() override;
    virtual void SHASX() override;
    virtual void SHSAX() override;
    virtual void SHSUB8() override;
    virtual void SHSUB16() override;
    virtual void UHADD8() override;
    virtual void UHADD16() override;
    virtual void UHASX() override;
    virtual void UHSAX() override;
    virtual void UHSUB8() override;
    virtual void UHSUB16() override;

    // Saturated Add/Subtract instructions
    virtual void QADD() override;
    virtual void QSUB() override;
    virtual void QDADD() override;
    virtual void QDSUB() override;

    // Synchronization Primitive instructions
    virtual void CLREX() override;
    virtual void LDREX() override;
    virtual void LDREXB() override;
    virtual void LDREXD() override;
    virtual void LDREXH() override;
    virtual void STREX() override;
    virtual void STREXB() override;
    virtual void STREXD() override;
    virtual void STREXH() override;
    virtual void SWP() override;

    // Status register access instructions
    virtual void CPS() override;
    virtual void MRS() override;
    virtual void MSR() override;
    virtual void RFE() override;
    virtual void SETEND() override;
    virtual void SRS() override;

    // Thumb specific instructions
    virtual void thumb_BLX_prefix(ArmImm11 imm11) override;
    virtual void thumb_BLX_suffix(bool L, ArmImm11 imm11) override;
};

}
