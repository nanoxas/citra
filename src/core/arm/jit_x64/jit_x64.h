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
    LocationDescriptor() = delete;
    LocationDescriptor(u32 arm_pc, bool TFlag, bool EFlag) : arm_pc(arm_pc), TFlag(TFlag), EFlag(EFlag) {}
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
    Gen::XEmitter* code = nullptr;

    RegAlloc reg_alloc;

    /// ARM pc -> x64 code block
    std::unordered_map<LocationDescriptor, CodePtr, LocationDescriptorHash> basic_blocks;

public:
    JitX64() = delete;
    explicit JitX64(Gen::XEmitter* code_);
    ~JitX64() override {}

    void ClearCache();

    CodePtr GetBB(u32 pc, bool TFlag, bool EFlag);

    /// Returns a pointer to the compiled basic block.
    CodePtr Compile(u32 pc, bool TFlag, bool EFlag);

private:
    LocationDescriptor current;
    unsigned instructions_compiled;
    bool stop_compilation;

    size_t GetInstSize() const { return current.TFlag ? 2 : 4; }
    void CompileSingleArmInstruction();
    void CompileSingleThumbInstruction();

    /// Updates the cycle count in JitState and sets instructions_compiled to zero.
    void CompileUpdateCycles(bool reset_cycles = true);

    /// Update MJitStateArmPC and current.TFlag before calling this function.
    void CompileReturnToDispatch();

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
    Gen::OpArg MJitStateTFlag();
    Gen::OpArg MJitStateHostReturnRIP();
    Gen::OpArg MJitStateHostReturnRSP();
    Gen::OpArg MJitStateZFlag();
    Gen::OpArg MJitStateCFlag();
    Gen::OpArg MJitStateNFlag();
    Gen::OpArg MJitStateVFlag();
    Gen::OpArg MJitStateExclusiveTag();
    Gen::OpArg MJitStateExclusiveState();

    u32 GetReg15Value() const { return (current.arm_pc & ~0x1) + GetInstSize() * 2; }

    void UpdateFlagsZVCN() {
        cond_manager.FlagsDirty();
        code->SETcc(Gen::CC_Z, MJitStateZFlag());
        code->SETcc(Gen::CC_C, MJitStateCFlag());
        code->SETcc(Gen::CC_O, MJitStateVFlag());
        code->SETcc(Gen::CC_S, MJitStateNFlag());
    }

    void UpdateFlagsZVN() {
        cond_manager.FlagsDirty();
        code->SETcc(Gen::CC_Z, MJitStateZFlag());
        code->SETcc(Gen::CC_O, MJitStateVFlag());
        code->SETcc(Gen::CC_S, MJitStateNFlag());
    }

    void UpdateFlagsZN() {
        cond_manager.FlagsDirty();
        code->SETcc(Gen::CC_Z, MJitStateZFlag());
        code->SETcc(Gen::CC_S, MJitStateNFlag());
    }

    void UpdateFlagsC_complement() {
        cond_manager.FlagsDirty();
        code->SETcc(Gen::CC_NC, MJitStateCFlag());
    }

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

    // Branch instructions
    void B(Cond cond, ArmImm24 imm24) override;
    void BL(Cond cond, ArmImm24 imm24) override;
    void BLX_imm(bool H, ArmImm24 imm24) override;
    void BLX_reg(Cond cond, ArmReg Rm) override;
    void BX(Cond cond, ArmReg Rm) override;
    void BXJ(Cond cond, ArmReg Rm) override;

    // Coprocessor instructions
    void CDP() override;
    void LDC() override;
    void MCR() override;
    void MCRR() override;
    void MRC() override;
    void MRRC() override;
    void STC() override;

    // Data processing instructions
    void CompileDataProcessingHelper(ArmReg Rn_index, ArmReg Rd_index, std::function<void(Gen::X64Reg)> body);
    void CompileDataProcessingHelper_Reverse(ArmReg Rn_index, ArmReg Rd_index, std::function<void(Gen::X64Reg)> body);
    void ADC_imm(Cond cond, bool S, ArmReg Rn, ArmReg Rd, int rotate, ArmImm8 imm8) override;
    void ADC_reg(Cond cond, bool S, ArmReg Rn, ArmReg Rd, ArmImm5 imm5, ShiftType shift, ArmReg Rm) override;
    void ADC_rsr(Cond cond, bool S, ArmReg Rn, ArmReg Rd, ArmReg Rs, ShiftType shift, ArmReg Rm) override;
    void ADD_imm(Cond cond, bool S, ArmReg Rn, ArmReg Rd, int rotate, ArmImm8 imm8) override;
    void ADD_reg(Cond cond, bool S, ArmReg Rn, ArmReg Rd, ArmImm5 imm5, ShiftType shift, ArmReg Rm) override;
    void ADD_rsr(Cond cond, bool S, ArmReg Rn, ArmReg Rd, ArmReg Rs, ShiftType shift, ArmReg Rm) override;
    void AND_imm(Cond cond, bool S, ArmReg Rn, ArmReg Rd, int rotate, ArmImm8 imm8) override;
    void AND_reg(Cond cond, bool S, ArmReg Rn, ArmReg Rd, ArmImm5 imm5, ShiftType shift, ArmReg Rm) override;
    void AND_rsr(Cond cond, bool S, ArmReg Rn, ArmReg Rd, ArmReg Rs, ShiftType shift, ArmReg Rm) override;
    void BIC_imm(Cond cond, bool S, ArmReg Rn, ArmReg Rd, int rotate, ArmImm8 imm8) override;
    void BIC_reg(Cond cond, bool S, ArmReg Rn, ArmReg Rd, ArmImm5 imm5, ShiftType shift, ArmReg Rm) override;
    void BIC_rsr(Cond cond, bool S, ArmReg Rn, ArmReg Rd, ArmReg Rs, ShiftType shift, ArmReg Rm) override;
    void CMN_imm(Cond cond,         ArmReg Rn,            int rotate, ArmImm8 imm8) override;
    void CMN_reg(Cond cond,         ArmReg Rn,            ArmImm5 imm5, ShiftType shift, ArmReg Rm) override;
    void CMN_rsr(Cond cond,         ArmReg Rn,            ArmReg Rs, ShiftType shift, ArmReg Rm) override;
    void CMP_imm(Cond cond,         ArmReg Rn,            int rotate, ArmImm8 imm8) override;
    void CMP_reg(Cond cond,         ArmReg Rn,            ArmImm5 imm5, ShiftType shift, ArmReg Rm) override;
    void CMP_rsr(Cond cond,         ArmReg Rn,            ArmReg Rs, ShiftType shift, ArmReg Rm) override;
    void EOR_imm(Cond cond, bool S, ArmReg Rn, ArmReg Rd, int rotate, ArmImm8 imm8) override;
    void EOR_reg(Cond cond, bool S, ArmReg Rn, ArmReg Rd, ArmImm5 imm5, ShiftType shift, ArmReg Rm) override;
    void EOR_rsr(Cond cond, bool S, ArmReg Rn, ArmReg Rd, ArmReg Rs, ShiftType shift, ArmReg Rm) override;
    void MOV_imm(Cond cond, bool S,            ArmReg Rd, int rotate, ArmImm8 imm8) override;
    void MOV_reg(Cond cond, bool S,            ArmReg Rd, ArmImm5 imm5, ShiftType shift, ArmReg Rm) override;
    void MOV_rsr(Cond cond, bool S,            ArmReg Rd, ArmReg Rs, ShiftType shift, ArmReg Rm) override;
    void MVN_imm(Cond cond, bool S,            ArmReg Rd, int rotate, ArmImm8 imm8) override;
    void MVN_reg(Cond cond, bool S,            ArmReg Rd, ArmImm5 imm5, ShiftType shift, ArmReg Rm) override;
    void MVN_rsr(Cond cond, bool S,            ArmReg Rd, ArmReg Rs, ShiftType shift, ArmReg Rm) override;
    void ORR_imm(Cond cond, bool S, ArmReg Rn, ArmReg Rd, int rotate, ArmImm8 imm8) override;
    void ORR_reg(Cond cond, bool S, ArmReg Rn, ArmReg Rd, ArmImm5 imm5, ShiftType shift, ArmReg Rm) override;
    void ORR_rsr(Cond cond, bool S, ArmReg Rn, ArmReg Rd, ArmReg Rs, ShiftType shift, ArmReg Rm) override;
    void RSB_imm(Cond cond, bool S, ArmReg Rn, ArmReg Rd, int rotate, ArmImm8 imm8) override;
    void RSB_reg(Cond cond, bool S, ArmReg Rn, ArmReg Rd, ArmImm5 imm5, ShiftType shift, ArmReg Rm) override;
    void RSB_rsr(Cond cond, bool S, ArmReg Rn, ArmReg Rd, ArmReg Rs, ShiftType shift, ArmReg Rm) override;
    void RSC_imm(Cond cond, bool S, ArmReg Rn, ArmReg Rd, int rotate, ArmImm8 imm8) override;
    void RSC_reg(Cond cond, bool S, ArmReg Rn, ArmReg Rd, ArmImm5 imm5, ShiftType shift, ArmReg Rm) override;
    void RSC_rsr(Cond cond, bool S, ArmReg Rn, ArmReg Rd, ArmReg Rs, ShiftType shift, ArmReg Rm) override;
    void SBC_imm(Cond cond, bool S, ArmReg Rn, ArmReg Rd, int rotate, ArmImm8 imm8) override;
    void SBC_reg(Cond cond, bool S, ArmReg Rn, ArmReg Rd, ArmImm5 imm5, ShiftType shift, ArmReg Rm) override;
    void SBC_rsr(Cond cond, bool S, ArmReg Rn, ArmReg Rd, ArmReg Rs, ShiftType shift, ArmReg Rm) override;
    void SUB_imm(Cond cond, bool S, ArmReg Rn, ArmReg Rd, int rotate, ArmImm8 imm8) override;
    void SUB_reg(Cond cond, bool S, ArmReg Rn, ArmReg Rd, ArmImm5 imm5, ShiftType shift, ArmReg Rm) override;
    void SUB_rsr(Cond cond, bool S, ArmReg Rn, ArmReg Rd, ArmReg Rs, ShiftType shift, ArmReg Rm) override;
    void TEQ_imm(Cond cond,         ArmReg Rn,            int rotate, ArmImm8 imm8) override;
    void TEQ_reg(Cond cond,         ArmReg Rn,            ArmImm5 imm5, ShiftType shift, ArmReg Rm) override;
    void TEQ_rsr(Cond cond,         ArmReg Rn,            ArmReg Rs, ShiftType shift, ArmReg Rm) override;
    void TST_imm(Cond cond,         ArmReg Rn,            int rotate, ArmImm8 imm8) override;
    void TST_reg(Cond cond,         ArmReg Rn,            ArmImm5 imm5, ShiftType shift, ArmReg Rm) override;
    void TST_rsr(Cond cond,         ArmReg Rn,            ArmReg Rs, ShiftType shift, ArmReg Rm) override;

    // Exception generation instructions
    void BKPT() override;
    void SVC() override;
    void UDF() override;

    // Extension functions
    void SXTAB() override;
    void SXTAB16() override;
    void SXTAH() override;
    void SXTB() override;
    void SXTB16() override;
    void SXTH() override;
    void UXTAB() override;
    void UXTAB16() override;
    void UXTAH() override;
    void UXTB() override;
    void UXTB16() override;
    void UXTH() override;

    // Hint instructions
    void PLD() override;
    void SEV() override;
    void WFE() override;
    void WFI() override;
    void YIELD() override;

    // Load/Store instructions
    void LDR_imm() override;
    void LDR_reg() override;
    void LDRB_imm() override;
    void LDRB_reg() override;
    void LDRBT() override;
    void LDRD_imm() override;
    void LDRD_reg() override;
    void LDRH_imm() override;
    void LDRH_reg() override;
    void LDRHT() override;
    void LDRSB_imm() override;
    void LDRSB_reg() override;
    void LDRSBT() override;
    void LDRSH_imm() override;
    void LDRSH_reg() override;
    void LDRSHT() override;
    void LDRT() override;
    void STR_imm() override;
    void STR_reg() override;
    void STRB_imm() override;
    void STRB_reg() override;
    void STRBT() override;
    void STRD_imm() override;
    void STRD_reg() override;
    void STRH_imm() override;
    void STRH_reg() override;
    void STRHT() override;
    void STRT() override;

    // Load/Store multiple instructions
    void LDM() override;
    void STM() override;

    // Miscellaneous instructions
    void CLZ() override;
    void NOP() override;
    void SEL() override;

    // Unsigned sum of absolute difference functions
    void USAD8() override;
    void USADA8() override;

    // Packing instructions
    void PKHBT(Cond cond, ArmReg Rn, ArmReg Rd, ArmImm5 imm5, ArmReg Rm) override;
    void PKHTB(Cond cond, ArmReg Rn, ArmReg Rd, ArmImm5 imm5, ArmReg Rm) override;

    // Reversal instructions
    void REV() override;
    void REV16() override;
    void REVSH() override;

    // Saturation instructions
    void SSAT() override;
    void SSAT16() override;
    void USAT() override;
    void USAT16() override;

    // Multiply (Normal) instructions
    void MLA() override;
    void MUL() override;

    // Multiply (Long) instructions
    void SMLAL() override;
    void SMULL() override;
    void UMAAL() override;
    void UMLAL() override;
    void UMULL() override;

    // Multiply (Halfword) instructions
    void SMLALxy() override;
    void SMLAxy() override;
    void SMULxy() override;

    // Multiply (word by halfword) instructions
    void SMLAWy() override;
    void SMULWy() override;

    // Multiply (Most significant word) instructions
    void SMMLA() override;
    void SMMLS() override;
    void SMMUL() override;

    // Multiply (Dual) instructions
    void SMLAD() override;
    void SMLALD() override;
    void SMLSD() override;
    void SMLSLD() override;
    void SMUAD() override;
    void SMUSD() override;

    // Parallel Add/Subtract (Modulo arithmetic) instructions
    void SADD8() override;
    void SADD16() override;
    void SASX() override;
    void SSAX() override;
    void SSUB8() override;
    void SSUB16() override;
    void UADD8() override;
    void UADD16() override;
    void UASX() override;
    void USAX() override;
    void USUB8() override;
    void USUB16() override;

    // Parallel Add/Subtract (Saturating) instructions
    void QADD8() override;
    void QADD16() override;
    void QASX() override;
    void QSAX() override;
    void QSUB8() override;
    void QSUB16() override;
    void UQADD8() override;
    void UQADD16() override;
    void UQASX() override;
    void UQSAX() override;
    void UQSUB8() override;
    void UQSUB16() override;

    // Parallel Add/Subtract (Halving) instructions
    void SHADD8() override;
    void SHADD16() override;
    void SHASX() override;
    void SHSAX() override;
    void SHSUB8() override;
    void SHSUB16() override;
    void UHADD8() override;
    void UHADD16() override;
    void UHASX() override;
    void UHSAX() override;
    void UHSUB8() override;
    void UHSUB16() override;

    // Saturated Add/Subtract instructions
    void QADD() override;
    void QSUB() override;
    void QDADD() override;
    void QDSUB() override;

    // Synchronization Primitive instructions
    void CLREX() override;
    void LDREX() override;
    void LDREXB() override;
    void LDREXD() override;
    void LDREXH() override;
    void STREX() override;
    void STREXB() override;
    void STREXD() override;
    void STREXH() override;
    void SWP() override;

    // Status register access instructions
    void CPS() override;
    void MRS() override;
    void MSR() override;
    void RFE() override;
    void SETEND(bool E) override;
    void SRS() override;

    // Thumb specific instructions
    void thumb_B(Cond cond, ArmImm8 imm8) override;
    void thumb_B(ArmImm11 imm11) override;

    ArmImm11 thumb_BLX_prefix_imm11 = 0;
    bool thumb_BLX_prefix_executed = false;
    bool thumb_BLX_suffix_executed = false;
    void thumb_BLX_prefix(ArmImm11 imm11) override;
    void thumb_BLX_suffix(bool L, ArmImm11 imm11) override;
};

}
