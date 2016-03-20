// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <functional>
#include <memory>

#include "common/common_types.h"
#include "common/common_funcs.h"

namespace ArmDecoder {

// This is a generic ARMv6 decoder using double dispatch.

class Instruction;
class Visitor;

const Instruction& DecodeArm(u32 instruction);
const Instruction& DecodeThumb(u16 instruction);

struct Matcher {
    u32 bit_mask;
    u32 expected;
    FORCE_INLINE bool Match(u32 x) const {
        return (x & bit_mask) == expected;
    }
    virtual void visit(Visitor* v, u32 inst) = 0;
};

class Instruction {
private:
    const std::unique_ptr<Matcher> matcher;

public:
    Instruction(char* name, std::unique_ptr<Matcher> matcher) : name(name), matcher(std::move(matcher)) {}

    const char* const name;

    FORCE_INLINE bool Match(u32 instruction) const {
        return (instruction & matcher->bit_mask) == matcher->expected;
    }

    FORCE_INLINE void Visit(Visitor* v, u32 instruction) const {
        matcher->visit(v, instruction);
    }
};

using Cond = u8;
using Imm5 = u32;
using Imm8 = u32;
using Imm11 = u32;
using Imm24 = u32;
using Register = int;
using ShiftType = int;

class Visitor {
public:
    virtual ~Visitor() = default;

    // Barrier instructions
    virtual void DMB() = 0;
    virtual void DSB() = 0;
    virtual void ISB() = 0;

    // Branch instructions
    virtual void B(Cond cond, Imm24 imm24) = 0;
    virtual void BL(Cond cond, Imm24 imm24) = 0;
    virtual void BLX_imm(bool H, Imm24 imm24) = 0;
    virtual void BLX_reg(Cond cond, Register Rm) = 0;
    virtual void BX(Cond cond, Register Rm) = 0;
    virtual void BXJ(Cond cond, Register Rm) = 0;

    // Coprocessor instructions
    virtual void CDP() = 0;
    virtual void LDC() = 0;
    virtual void MCR() = 0;
    virtual void MCRR() = 0;
    virtual void MRC() = 0;
    virtual void MRRC() = 0;
    virtual void STC() = 0;

    // Data processing instructions
    virtual void ADC_imm(Cond cond, bool S, Register Rn, Register Rd, int rotate, Imm8 imm8) = 0;
    virtual void ADC_reg(Cond cond, bool S, Register Rn, Register Rd, Imm5 imm5, ShiftType shift, Register Rm) = 0;
    virtual void ADC_rsr(Cond cond, bool S, Register Rn, Register Rd, Register Rs, ShiftType shift, Register Rm) = 0;
    virtual void ADD_imm(Cond cond, bool S, Register Rn, Register Rd, int rotate, Imm8 imm8) = 0;
    virtual void ADD_reg(Cond cond, bool S, Register Rn, Register Rd, Imm5 imm5, ShiftType shift, Register Rm) = 0;
    virtual void ADD_rsr(Cond cond, bool S, Register Rn, Register Rd, Register Rs, ShiftType shift, Register Rm) = 0;
    virtual void AND_imm(Cond cond, bool S, Register Rn, Register Rd, int rotate, Imm8 imm8) = 0;
    virtual void AND_reg(Cond cond, bool S, Register Rn, Register Rd, Imm5 imm5, ShiftType shift, Register Rm) = 0;
    virtual void AND_rsr(Cond cond, bool S, Register Rn, Register Rd, Register Rs, ShiftType shift, Register Rm) = 0;
    virtual void BIC_imm(Cond cond, bool S, Register Rn, Register Rd, int rotate, Imm8 imm8) = 0;
    virtual void BIC_reg(Cond cond, bool S, Register Rn, Register Rd, Imm5 imm5, ShiftType shift, Register Rm) = 0;
    virtual void BIC_rsr(Cond cond, bool S, Register Rn, Register Rd, Register Rs, ShiftType shift, Register Rm) = 0;
    virtual void CMN_imm(Cond cond, Register Rn, int rotate, Imm8 imm8) = 0;
    virtual void CMN_reg(Cond cond, Register Rn, Imm5 imm5, ShiftType shift, Register Rm) = 0;
    virtual void CMN_rsr(Cond cond, Register Rn, Register Rs, ShiftType shift, Register Rm) = 0;
    virtual void CMP_imm(Cond cond, Register Rn, int rotate, Imm8 imm8) = 0;
    virtual void CMP_reg(Cond cond, Register Rn, Imm5 imm5, ShiftType shift, Register Rm) = 0;
    virtual void CMP_rsr(Cond cond, Register Rn, Register Rs, ShiftType shift, Register Rm) = 0;
    virtual void EOR_imm(Cond cond, bool S, Register Rn, Register Rd, int rotate, Imm8 imm8) = 0;
    virtual void EOR_reg(Cond cond, bool S, Register Rn, Register Rd, Imm5 imm5, ShiftType shift, Register Rm) = 0;
    virtual void EOR_rsr(Cond cond, bool S, Register Rn, Register Rd, Register Rs, ShiftType shift, Register Rm) = 0;
    virtual void MOV_imm(Cond cond, bool S, Register Rd, int rotate, Imm8 imm8) = 0;
    virtual void MOV_reg(Cond cond, bool S, Register Rd, Imm5 imm5, ShiftType shift, Register Rm) = 0;
    virtual void MOV_rsr(Cond cond, bool S, Register Rd, Register Rs, ShiftType shift, Register Rm) = 0;
    virtual void MVN_imm(Cond cond, bool S, Register Rd, int rotate, Imm8 imm8) = 0;
    virtual void MVN_reg(Cond cond, bool S, Register Rd, Imm5 imm5, ShiftType shift, Register Rm) = 0;
    virtual void MVN_rsr(Cond cond, bool S, Register Rd, Register Rs, ShiftType shift, Register Rm) = 0;
    virtual void ORR_imm(Cond cond, bool S, Register Rn, Register Rd, int rotate, Imm8 imm8) = 0;
    virtual void ORR_reg(Cond cond, bool S, Register Rn, Register Rd, Imm5 imm5, ShiftType shift, Register Rm) = 0;
    virtual void ORR_rsr(Cond cond, bool S, Register Rn, Register Rd, Register Rs, ShiftType shift, Register Rm) = 0;
    virtual void RSB_imm(Cond cond, bool S, Register Rn, Register Rd, int rotate, Imm8 imm8) = 0;
    virtual void RSB_reg(Cond cond, bool S, Register Rn, Register Rd, Imm5 imm5, ShiftType shift, Register Rm) = 0;
    virtual void RSB_rsr(Cond cond, bool S, Register Rn, Register Rd, Register Rs, ShiftType shift, Register Rm) = 0;
    virtual void RSC_imm(Cond cond, bool S, Register Rn, Register Rd, int rotate, Imm8 imm8) = 0;
    virtual void RSC_reg(Cond cond, bool S, Register Rn, Register Rd, Imm5 imm5, ShiftType shift, Register Rm) = 0;
    virtual void RSC_rsr(Cond cond, bool S, Register Rn, Register Rd, Register Rs, ShiftType shift, Register Rm) = 0;
    virtual void SBC_imm(Cond cond, bool S, Register Rn, Register Rd, int rotate, Imm8 imm8) = 0;
    virtual void SBC_reg(Cond cond, bool S, Register Rn, Register Rd, Imm5 imm5, ShiftType shift, Register Rm) = 0;
    virtual void SBC_rsr(Cond cond, bool S, Register Rn, Register Rd, Register Rs, ShiftType shift, Register Rm) = 0;
    virtual void SUB_imm(Cond cond, bool S, Register Rn, Register Rd, int rotate, Imm8 imm8) = 0;
    virtual void SUB_reg(Cond cond, bool S, Register Rn, Register Rd, Imm5 imm5, ShiftType shift, Register Rm) = 0;
    virtual void SUB_rsr(Cond cond, bool S, Register Rn, Register Rd, Register Rs, ShiftType shift, Register Rm) = 0;
    virtual void TEQ_imm(Cond cond, Register Rn, int rotate, Imm8 imm8) = 0;
    virtual void TEQ_reg(Cond cond, Register Rn, Imm5 imm5, ShiftType shift, Register Rm) = 0;
    virtual void TEQ_rsr(Cond cond, Register Rn, Register Rs, ShiftType shift, Register Rm) = 0;
    virtual void TST_imm(Cond cond, Register Rn, int rotate, Imm8 imm8) = 0;
    virtual void TST_reg(Cond cond, Register Rn, Imm5 imm5, ShiftType shift, Register Rm) = 0;
    virtual void TST_rsr(Cond cond, Register Rn, Register Rs, ShiftType shift, Register Rm) = 0;

    // Exception generation instructions
    virtual void BKPT() = 0;
    virtual void HVC() = 0;
    virtual void SMC() = 0;
    virtual void SVC() = 0;
    virtual void UDF() = 0;

    // Extension functions
    virtual void SXTAB() = 0;
    virtual void SXTAB16() = 0;
    virtual void SXTAH() = 0;
    virtual void SXTB() = 0;
    virtual void SXTB16() = 0;
    virtual void SXTH() = 0;
    virtual void UXTAB() = 0;
    virtual void UXTAB16() = 0;
    virtual void UXTAH() = 0;
    virtual void UXTB() = 0;
    virtual void UXTB16() = 0;
    virtual void UXTH() = 0;

    // Hint instructions
    virtual void DBG() = 0;
    virtual void PLD() = 0;
    virtual void PLI() = 0;

    // Load/Store instructions
    virtual void LDR_imm() = 0;
    virtual void LDR_reg() = 0;
    virtual void LDRB_imm() = 0;
    virtual void LDRB_reg() = 0;
    virtual void LDRBT() = 0;
    virtual void LDRD_imm() = 0;
    virtual void LDRD_reg() = 0;
    virtual void LDRH_imm() = 0;
    virtual void LDRH_reg() = 0;
    virtual void LDRHT() = 0;
    virtual void LDRSB_imm() = 0;
    virtual void LDRSB_reg() = 0;
    virtual void LDRSBT() = 0;
    virtual void LDRSH_imm() = 0;
    virtual void LDRSH_reg() = 0;
    virtual void LDRSHT() = 0;
    virtual void LDRT() = 0;
    virtual void STR_imm() = 0;
    virtual void STR_reg() = 0;
    virtual void STRB_imm() = 0;
    virtual void STRB_reg() = 0;
    virtual void STRBT() = 0;
    virtual void STRD_imm() = 0;
    virtual void STRD_reg() = 0;
    virtual void STRH_imm() = 0;
    virtual void STRH_reg() = 0;
    virtual void STRHT() = 0;
    virtual void STRT() = 0;

    // Load/Store multiple instructions
    virtual void LDM() = 0;
    virtual void STM() = 0;

    // Miscellaneous instructions
    virtual void CLZ() = 0;
    virtual void ERET() = 0;
    virtual void NOP() = 0;
    virtual void SEL() = 0;

    // Unsigned sum of absolute difference functions
    virtual void USAD8() = 0;
    virtual void USADA8() = 0;

    // Packing instructions
    virtual void PKH() = 0;

    // Reversal instructions
    virtual void RBIT() = 0;
    virtual void REV() = 0;
    virtual void REV16() = 0;
    virtual void REVSH() = 0;

    // Saturation instructions
    virtual void SSAT() = 0;
    virtual void SSAT16() = 0;
    virtual void USAT() = 0;
    virtual void USAT16() = 0;

    // Multiply (Normal) instructions
    virtual void MLA() = 0;
    virtual void MLS() = 0;
    virtual void MUL() = 0;

    // Multiply (Long) instructions
    virtual void SMLAL() = 0;
    virtual void SMULL() = 0;
    virtual void UMAAL() = 0;
    virtual void UMLAL() = 0;
    virtual void UMULL() = 0;

    // Multiply (Halfword) instructions
    virtual void SMLALxy() = 0;
    virtual void SMLAxy() = 0;
    virtual void SMULxy() = 0;

    // Multiply (word by halfword) instructions
    virtual void SMLAWy() = 0;
    virtual void SMULWy() = 0;

    // Multiply (Most significant word) instructions
    virtual void SMMLA() = 0;
    virtual void SMMLS() = 0;
    virtual void SMMUL() = 0;

    // Multiply (Dual) instructions
    virtual void SMLAD() = 0;
    virtual void SMLALD() = 0;
    virtual void SMLSD() = 0;
    virtual void SMLSLD() = 0;
    virtual void SMUAD() = 0;
    virtual void SMUSD() = 0;

    // Parallel Add/Subtract (Modulo arithmetic) instructions
    virtual void SADD8() = 0;
    virtual void SADD16() = 0;
    virtual void SASX() = 0;
    virtual void SSAX() = 0;
    virtual void SSUB8() = 0;
    virtual void SSUB16() = 0;
    virtual void UADD8() = 0;
    virtual void UADD16() = 0;
    virtual void UASX() = 0;
    virtual void USAX() = 0;
    virtual void USUB8() = 0;
    virtual void USUB16() = 0;

    // Parallel Add/Subtract (Saturating) instructions
    virtual void QADD8() = 0;
    virtual void QADD16() = 0;
    virtual void QASX() = 0;
    virtual void QSAX() = 0;
    virtual void QSUB8() = 0;
    virtual void QSUB16() = 0;
    virtual void UQADD8() = 0;
    virtual void UQADD16() = 0;
    virtual void UQASX() = 0;
    virtual void UQSAX() = 0;
    virtual void UQSUB8() = 0;
    virtual void UQSUB16() = 0;

    // Parallel Add/Subtract (Halving) instructions
    virtual void SHADD8() = 0;
    virtual void SHADD16() = 0;
    virtual void SHASX() = 0;
    virtual void SHSAX() = 0;
    virtual void SHSUB8() = 0;
    virtual void SHSUB16() = 0;
    virtual void UHADD8() = 0;
    virtual void UHADD16() = 0;
    virtual void UHASX() = 0;
    virtual void UHSAX() = 0;
    virtual void UHSUB8() = 0;
    virtual void UHSUB16() = 0;

    // Saturated Add/Subtract instructions
    virtual void QADD() = 0;
    virtual void QSUB() = 0;
    virtual void QDADD() = 0;
    virtual void QDSUB() = 0;

    // Synchronization Primitive instructions
    virtual void CLREX() = 0;
    virtual void LDREX() = 0;
    virtual void LDREXB() = 0;
    virtual void LDREXD() = 0;
    virtual void LDREXH() = 0;
    virtual void STREX() = 0;
    virtual void STREXB() = 0;
    virtual void STREXD() = 0;
    virtual void STREXH() = 0;
    virtual void SWP() = 0;

    // Status register access instructions
    virtual void CPS() = 0;
    virtual void MRS() = 0;
    virtual void MSR() = 0;
    virtual void RFE() = 0;
    virtual void SETEND() = 0;
    virtual void SRS() = 0;

    // Thumb specific instructions
    virtual void thumb_BLX_prefix(Imm11 imm11) = 0;
    virtual void thumb_BLX_suffix(bool L, Imm11 imm11) = 0;
};

};
