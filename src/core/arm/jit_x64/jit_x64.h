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
        return std::hash<u64>()(static_cast<u64>(x.arm_pc) ^ (static_cast<u64>(x.TFlag) << 32) ^ (static_cast<u64>(x.EFlag) << 33));
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
    explicit JitX64(Gen::XEmitter* code);
    ~JitX64() override {}

    void ClearCache();

    CodePtr GetBB(u32 pc, bool TFlag, bool EFlag);

    /// Returns a pointer to the compiled basic block.
    CodePtr Compile(u32 pc, bool TFlag, bool EFlag);

private:
    LocationDescriptor current = { 0, false, false };
    unsigned instructions_compiled = 0;
    bool stop_compilation = false;

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
    Gen::OpArg MJitStateCpsr();
    Gen::OpArg MJitStateExclusiveTag();
    Gen::OpArg MJitStateExclusiveState();

    u32 GetReg15Value() const {
        return (current.arm_pc & ~0x1) + static_cast<u32>(GetInstSize() * 2);
    }
    u32 GetReg15Value_WordAligned() const {
        return (current.arm_pc & ~0x3) + static_cast<u32>(GetInstSize() * 2);
    }

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
        Cond current_cond;
        bool flags_dirty;
        Gen::FixupBranch current_cond_fixup;
    public:
        void Init(JitX64* jit_);
        void CompileCond(Cond cond);
        void Always();
        void FlagsDirty();
        Cond CurrentCond() const;
    } cond_manager;

private:
    void CompileInterpretInstruction();
    void CompileCallHost(const void* const fn);
    /// dest must be a temporary that contains a copy of the value of Rm
    void CompileShifter_imm(Gen::X64Reg dest, ArmImm5 imm5, ShiftType shift, bool do_shifter_carry_out);

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
    void CompileDataProcessingHelper(ArmReg Rn, ArmReg Rd, std::function<void(Gen::X64Reg)> body);
    void CompileDataProcessingHelper_Reverse(ArmReg Rn, ArmReg Rd, std::function<void(Gen::X64Reg)> body);
    Gen::X64Reg CompileDataProcessingHelper_reg(ArmImm5 imm5, ShiftType shift, ArmReg Rm, bool do_shifter_carry_out);
    Gen::X64Reg CompileDataProcessingHelper_rsr(ArmReg Rs, ShiftType shift, ArmReg Rm, bool do_shifter_carry_out);
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
    void BKPT(Cond cond, ArmImm12 imm12, ArmImm4 imm4) override;
    void SVC(Cond cond, ArmImm24 imm24) override;
    void UDF() override;

    // Extension functions
    void SXTAB(Cond cond, ArmReg Rn, ArmReg Rd, SignExtendRotation rotate, ArmReg Rm) override;
    void SXTAB16(Cond cond, ArmReg Rn, ArmReg Rd, SignExtendRotation rotate, ArmReg Rm) override;
    void SXTAH(Cond cond, ArmReg Rn, ArmReg Rd, SignExtendRotation rotate, ArmReg Rm) override;
    void SXTB(Cond cond, ArmReg Rd, SignExtendRotation rotate, ArmReg Rm) override;
    void SXTB16(Cond cond, ArmReg Rd, SignExtendRotation rotate, ArmReg Rm) override;
    void SXTH(Cond cond, ArmReg Rd, SignExtendRotation rotate, ArmReg Rm) override;
    void UXTAB(Cond cond, ArmReg Rn, ArmReg Rd, SignExtendRotation rotate, ArmReg Rm) override;
    void UXTAB16(Cond cond, ArmReg Rn, ArmReg Rd, SignExtendRotation rotate, ArmReg Rm) override;
    void UXTAH(Cond cond, ArmReg Rn, ArmReg Rd, SignExtendRotation rotate, ArmReg Rm) override;
    void UXTB(Cond cond, ArmReg Rd, SignExtendRotation rotate, ArmReg Rm) override;
    void UXTB16(Cond cond, ArmReg Rd, SignExtendRotation rotate, ArmReg Rm) override;
    void UXTH(Cond cond, ArmReg Rd, SignExtendRotation rotate, ArmReg Rm) override;

    // Hint instructions
    void PLD() override;
    void SEV() override;
    void WFE() override;
    void WFI() override;
    void YIELD() override;

    // Load/Store instructions
    void LoadAndStoreWordOrUnsignedByte_Immediate_Helper(Gen::X64Reg dest, bool U, ArmReg Rn, ArmImm12 imm12);
    void LoadAndStoreWordOrUnsignedByte_Register_Helper(Gen::X64Reg dest, bool U, ArmReg Rn, ArmReg Rm);
    void LoadAndStoreWordOrUnsignedByte_ScaledRegister_Helper(Gen::X64Reg dest, bool U, ArmReg Rn, ArmImm5 imm5, ShiftType shift, ArmReg Rm);
    void LoadAndStoreWordOrUnsignedByte_ImmediateOffset(Gen::X64Reg dest, bool U, ArmReg Rn, ArmImm12 imm12);
    void LoadAndStoreWordOrUnsignedByte_ImmediatePreIndexed(Gen::X64Reg dest, bool U, ArmReg Rn_index, ArmImm12 imm12);
    void LoadAndStoreWordOrUnsignedByte_ImmediatePostIndexed(Gen::X64Reg dest, bool U, ArmReg Rn_index, ArmImm12 imm12);
    void LoadAndStoreWordOrUnsignedByte_ScaledRegisterOffset(Gen::X64Reg dest, bool U, ArmReg Rn, ArmImm5 imm5, ShiftType shift, ArmReg Rm);
    void LoadAndStoreWordOrUnsignedByte_ScaledRegisterPreIndexed(Gen::X64Reg dest, bool U, ArmReg Rn, ArmImm5 imm5, ShiftType shift, ArmReg Rm);
    void LoadAndStoreWordOrUnsignedByte_ScaledRegisterPostIndexed(Gen::X64Reg dest, bool U, ArmReg Rn, ArmImm5 imm5, ShiftType shift, ArmReg Rm);
    void LDR_imm(Cond cond, bool P, bool U, bool W, ArmReg Rn, ArmReg Rd, ArmImm11 imm11) override;
    void LDR_reg(Cond cond, bool P, bool U, bool W, ArmReg Rn, ArmReg Rd, ArmImm5 imm5, ShiftType shift, ArmReg Rm) override;
    void LDRB_imm(Cond cond, bool P, bool U, bool W, ArmReg Rn, ArmReg Rd, ArmImm11 imm11) override;
    void LDRB_reg(Cond cond, bool P, bool U, bool W, ArmReg Rn, ArmReg Rd, ArmImm5 imm5, ShiftType shift, ArmReg Rm) override;
    void LDRBT() override;
    void LDRD_imm(Cond cond, bool P, bool U, bool W, ArmReg Rn, ArmReg Rd, ArmImm4 imm8a, ArmImm4 imm8b) override;
    void LDRD_reg(Cond cond, bool P, bool U, bool W, ArmReg Rn, ArmReg Rd, ArmReg Rm) override;
    void LDRH_imm(Cond cond, bool P, bool U, bool W, ArmReg Rn, ArmReg Rd, ArmImm4 imm8a, ArmImm4 imm8b) override;
    void LDRH_reg(Cond cond, bool P, bool U, bool W, ArmReg Rn, ArmReg Rd, ArmReg Rm) override;
    void LDRHT() override;
    void LDRSB_imm(Cond cond, bool P, bool U, bool W, ArmReg Rn, ArmReg Rd, ArmImm4 imm8a, ArmImm4 imm8b) override;
    void LDRSB_reg(Cond cond, bool P, bool U, bool W, ArmReg Rn, ArmReg Rd, ArmReg Rm) override;
    void LDRSBT() override;
    void LDRSH_imm(Cond cond, bool P, bool U, bool W, ArmReg Rn, ArmReg Rd, ArmImm4 imm8a, ArmImm4 imm8b) override;
    void LDRSH_reg(Cond cond, bool P, bool U, bool W, ArmReg Rn, ArmReg Rd, ArmReg Rm) override;
    void LDRSHT() override;
    void LDRT() override;
    void STR_imm(Cond cond, bool P, bool U, bool W, ArmReg Rn, ArmReg Rd, ArmImm11 imm11) override;
    void STR_reg(Cond cond, bool P, bool U, bool W, ArmReg Rn, ArmReg Rd, ArmImm5 imm5, ShiftType shift, ArmReg Rm) override;
    void STRB_imm(Cond cond, bool P, bool U, bool W, ArmReg Rn, ArmReg Rd, ArmImm11 imm11) override;
    void STRB_reg(Cond cond, bool P, bool U, bool W, ArmReg Rn, ArmReg Rd, ArmImm5 imm5, ShiftType shift, ArmReg Rm) override;
    void STRBT() override;
    void STRD_imm(Cond cond, bool P, bool U, bool W, ArmReg Rn, ArmReg Rd, ArmImm4 imm8a, ArmImm4 imm8b) override;
    void STRD_reg(Cond cond, bool P, bool U, bool W, ArmReg Rn, ArmReg Rd, ArmReg Rm) override;
    void STRH_imm(Cond cond, bool P, bool U, bool W, ArmReg Rn, ArmReg Rd, ArmImm4 imm8a, ArmImm4 imm8b) override;
    void STRH_reg(Cond cond, bool P, bool U, bool W, ArmReg Rn, ArmReg Rd, ArmReg Rm) override;
    void STRHT() override;
    void STRT() override;

    // Load/Store multiple instructions
    void LDM(Cond cond, bool P, bool U, bool W, ArmReg Rn, ArmRegList list) override;
    void LDM_usr() override;
    void LDM_eret() override;
    void STM(Cond cond, bool P, bool U, bool W, ArmReg Rn, ArmRegList list) override;
    void STM_usr() override;

    // Miscellaneous instructions
    void CLZ(Cond cond, ArmReg Rd, ArmReg Rm) override;
    void NOP() override;
    void SEL(Cond cond, ArmReg Rn, ArmReg Rd, ArmReg Rm) override;

    // Unsigned sum of absolute difference functions
    void USAD8(Cond cond, ArmReg Rd, ArmReg Rm, ArmReg Rn) override;
    void USADA8(Cond cond, ArmReg Rd, ArmReg Ra, ArmReg Rm, ArmReg Rn) override;

    // Packing instructions
    void PKHBT(Cond cond, ArmReg Rn, ArmReg Rd, ArmImm5 imm5, ArmReg Rm) override;
    void PKHTB(Cond cond, ArmReg Rn, ArmReg Rd, ArmImm5 imm5, ArmReg Rm) override;

    // Reversal instructions
    void REV(Cond cond, ArmReg Rd, ArmReg Rm) override;
    void REV16(Cond cond, ArmReg Rd, ArmReg Rm) override;
    void REVSH(Cond cond, ArmReg Rd, ArmReg Rm) override;

    // Saturation instructions
    void SSAT(Cond cond, ArmImm5 sat_imm, ArmReg Rd, ArmImm5 imm5, bool sh, ArmReg Rn) override;
    void SSAT16(Cond cond, ArmImm4 sat_imm, ArmReg Rd, ArmReg Rn) override;
    void USAT(Cond cond, ArmImm5 sat_imm, ArmReg Rd, ArmImm5 imm5, bool sh, ArmReg Rn) override;
    void USAT16(Cond cond, ArmImm4 sat_imm, ArmReg Rd, ArmReg Rn) override;

    // Multiply (Normal) instructions
    void MLA(Cond cond, bool S, ArmReg Rd, ArmReg Ra, ArmReg Rm, ArmReg Rn) override;
    void MUL(Cond cond, bool S, ArmReg Rd, ArmReg Rm, ArmReg Rn) override;

    // Multiply (Long) instructions
    void SMLAL(Cond cond, bool S, ArmReg RdHi, ArmReg RdLo, ArmReg Rm, ArmReg Rn) override;
    void SMULL(Cond cond, bool S, ArmReg RdHi, ArmReg RdLo, ArmReg Rm, ArmReg Rn) override;
    void UMAAL(Cond cond, ArmReg RdHi, ArmReg RdLo, ArmReg Rm, ArmReg Rn) override;
    void UMLAL(Cond cond, bool S, ArmReg RdHi, ArmReg RdLo, ArmReg Rm, ArmReg Rn) override;
    void UMULL(Cond cond, bool S, ArmReg RdHi, ArmReg RdLo, ArmReg Rm, ArmReg Rn) override;

    // Multiply (Halfword) instructions
    void SMLALxy(Cond cond, ArmReg RdHi, ArmReg RdLo, ArmReg Rm, bool M, bool N, ArmReg Rn) override;
    void SMLAxy(Cond cond, ArmReg Rd, ArmReg Ra, ArmReg Rm, bool M, bool N, ArmReg Rn) override;
    void SMULxy(Cond cond, ArmReg Rd, ArmReg Rm, bool M, bool N, ArmReg Rn) override;

    // Multiply (word by halfword) instructions
    void SMLAWy(Cond cond, ArmReg Rd, ArmReg Ra, ArmReg Rm, bool M, ArmReg Rn) override;
    void SMULWy(Cond cond, ArmReg Rd, ArmReg Rm, bool M, ArmReg Rn) override;

    // Multiply (Most significant word) instructions
    void SMMLA(Cond cond, ArmReg Rd, ArmReg Ra, ArmReg Rm, bool R, ArmReg Rn) override;
    void SMMLS(Cond cond, ArmReg Rd, ArmReg Ra, ArmReg Rm, bool R, ArmReg Rn) override;
    void SMMUL(Cond cond, ArmReg Rd, ArmReg Rm, bool R, ArmReg Rn) override;

    // Multiply (Dual) instructions
    void SMLAD(Cond cond, ArmReg Rd, ArmReg Ra, ArmReg Rm, bool M, ArmReg Rn) override;
    void SMLALD(Cond cond, ArmReg RdHi, ArmReg RdLo, ArmReg Rm, bool M, ArmReg Rn) override;
    void SMLSD(Cond cond, ArmReg Rd, ArmReg Ra, ArmReg Rm, bool M, ArmReg Rn) override;
    void SMLSLD(Cond cond, ArmReg RdHi, ArmReg RdLo, ArmReg Rm, bool M, ArmReg Rn) override;
    void SMUAD(Cond cond, ArmReg Rd, ArmReg Rm, bool M, ArmReg Rn) override;
    void SMUSD(Cond cond, ArmReg Rd, ArmReg Rm, bool M, ArmReg Rn) override;

    // Parallel Add/Subtract (Modulo arithmetic) instructions
    void SADD8(Cond cond, ArmReg Rn, ArmReg Rd, ArmReg Rm) override;
    void SADD16(Cond cond, ArmReg Rn, ArmReg Rd, ArmReg Rm) override;
    void SASX(Cond cond, ArmReg Rn, ArmReg Rd, ArmReg Rm) override;
    void SSAX(Cond cond, ArmReg Rn, ArmReg Rd, ArmReg Rm) override;
    void SSUB8(Cond cond, ArmReg Rn, ArmReg Rd, ArmReg Rm) override;
    void SSUB16(Cond cond, ArmReg Rn, ArmReg Rd, ArmReg Rm) override;
    void UADD8(Cond cond, ArmReg Rn, ArmReg Rd, ArmReg Rm) override;
    void UADD16(Cond cond, ArmReg Rn, ArmReg Rd, ArmReg Rm) override;
    void UASX(Cond cond, ArmReg Rn, ArmReg Rd, ArmReg Rm) override;
    void USAX(Cond cond, ArmReg Rn, ArmReg Rd, ArmReg Rm) override;
    void USUB8(Cond cond, ArmReg Rn, ArmReg Rd, ArmReg Rm) override;
    void USUB16(Cond cond, ArmReg Rn, ArmReg Rd, ArmReg Rm) override;

    // Parallel Add/Subtract (Saturating) instructions
    void QADD8(Cond cond, ArmReg Rn, ArmReg Rd, ArmReg Rm) override;
    void QADD16(Cond cond, ArmReg Rn, ArmReg Rd, ArmReg Rm) override;
    void QASX(Cond cond, ArmReg Rn, ArmReg Rd, ArmReg Rm) override;
    void QSAX(Cond cond, ArmReg Rn, ArmReg Rd, ArmReg Rm) override;
    void QSUB8(Cond cond, ArmReg Rn, ArmReg Rd, ArmReg Rm) override;
    void QSUB16(Cond cond, ArmReg Rn, ArmReg Rd, ArmReg Rm) override;
    void UQADD8(Cond cond, ArmReg Rn, ArmReg Rd, ArmReg Rm) override;
    void UQADD16(Cond cond, ArmReg Rn, ArmReg Rd, ArmReg Rm) override;
    void UQASX(Cond cond, ArmReg Rn, ArmReg Rd, ArmReg Rm) override;
    void UQSAX(Cond cond, ArmReg Rn, ArmReg Rd, ArmReg Rm) override;
    void UQSUB8(Cond cond, ArmReg Rn, ArmReg Rd, ArmReg Rm) override;
    void UQSUB16(Cond cond, ArmReg Rn, ArmReg Rd, ArmReg Rm) override;

    // Parallel Add/Subtract (Halving) instructions
    void SHADD8(Cond cond, ArmReg Rn, ArmReg Rd, ArmReg Rm) override;
    void SHADD16(Cond cond, ArmReg Rn, ArmReg Rd, ArmReg Rm) override;
    void SHASX(Cond cond, ArmReg Rn, ArmReg Rd, ArmReg Rm) override;
    void SHSAX(Cond cond, ArmReg Rn, ArmReg Rd, ArmReg Rm) override;
    void SHSUB8(Cond cond, ArmReg Rn, ArmReg Rd, ArmReg Rm) override;
    void SHSUB16(Cond cond, ArmReg Rn, ArmReg Rd, ArmReg Rm) override;
    void UHADD8(Cond cond, ArmReg Rn, ArmReg Rd, ArmReg Rm) override;
    void UHADD16(Cond cond, ArmReg Rn, ArmReg Rd, ArmReg Rm) override;
    void UHASX(Cond cond, ArmReg Rn, ArmReg Rd, ArmReg Rm) override;
    void UHSAX(Cond cond, ArmReg Rn, ArmReg Rd, ArmReg Rm) override;
    void UHSUB8(Cond cond, ArmReg Rn, ArmReg Rd, ArmReg Rm) override;
    void UHSUB16(Cond cond, ArmReg Rn, ArmReg Rd, ArmReg Rm) override;

    // Saturated Add/Subtract instructions
    void QADD(Cond cond, ArmReg Rn, ArmReg Rd, ArmReg Rm) override;
    void QSUB(Cond cond, ArmReg Rn, ArmReg Rd, ArmReg Rm) override;
    void QDADD(Cond cond, ArmReg Rn, ArmReg Rd, ArmReg Rm) override;
    void QDSUB(Cond cond, ArmReg Rn, ArmReg Rd, ArmReg Rm) override;

    // Synchronization Primitive instructions
    void CLREX() override;
    void LDREX(Cond cond, ArmReg Rn, ArmReg Rd) override;
    void LDREXB(Cond cond, ArmReg Rn, ArmReg Rd) override;
    void LDREXD(Cond cond, ArmReg Rn, ArmReg Rd) override;
    void LDREXH(Cond cond, ArmReg Rn, ArmReg Rd) override;
    void STREX(Cond cond, ArmReg Rn, ArmReg Rd, ArmReg Rm) override;
    void STREXB(Cond cond, ArmReg Rn, ArmReg Rd, ArmReg Rm) override;
    void STREXD(Cond cond, ArmReg Rn, ArmReg Rd, ArmReg Rm) override;
    void STREXH(Cond cond, ArmReg Rn, ArmReg Rd, ArmReg Rm) override;
    void SWP(Cond cond, ArmReg Rn, ArmReg Rd, ArmReg Rm) override;
    void SWPB(Cond cond, ArmReg Rn, ArmReg Rd, ArmReg Rm) override;

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
    void thumb_BLX_prefix(ArmImm11 imm11) override;
    void thumb_BLX_suffix(bool L, ArmImm11 imm11) override;

    ArmImm11 thumb_BLX_prefix_imm11 = 0;
    bool thumb_BLX_prefix_executed = false;
    bool thumb_BLX_suffix_executed = false;
};

} // namespace JitX64
