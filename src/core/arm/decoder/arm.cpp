// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <algorithm>
#include <cstddef>

#include "common/assert.h"
#include "common/common_types.h"
#include "common/make_unique.h"

#include "core/arm/decoder/decoder.h"

namespace ArmDecoder {

namespace Impl {
    template<typename Function, size_t i, typename Container, typename... Args>
    struct Call {
        FORCE_INLINE static void call(Visitor* v, Function fn, const Container& list, Args&&... args) {
            using ArgT = decltype(std::get<i-1>(list));
            Call<Function, i-1, Container, ArgT, Args...>::call(v, fn, list, std::get<i-1>(list), args...);
        }
    };

    template<typename Function, typename Container, typename... Args>
    struct Call<Function, 0, Container, Args...> {
        FORCE_INLINE static void call(Visitor* v, Function fn, const Container& list, Args&&... args) {
            (v->*fn)(args...);
        }
    };

    template<size_t N, typename Function>
    struct MatcherImpl : Matcher {
        std::array<u32, N> masks;
        std::array<size_t, N> shifts;
        Function fn;
        virtual void visit(Visitor *v, u32 inst) override {
            std::array<u32, N> values;
            for (int i = 0; i < N; i++) {
                values[i] = (inst & masks[i]) >> shifts[i];
            }
            Call<Function, N, decltype(values)>::call(v, fn, values);
        }
    };
}

template<size_t N, typename Function>
static std::unique_ptr<Matcher> MakeMatcher(const char* const format, Function fn) {
    ASSERT(strlen(format) == 32);

    auto ret = Common::make_unique<Impl::MatcherImpl<N, Function>>();
    ret->fn = fn;
    ret->masks.fill(0);
    ret->shifts.fill(0);

    char ch = 0;
    int j = -1;

    for (int i = 0; i < 32; i++) {
        const u32 bit = 1 << (31 - i);

        if (format[i] == '0') {
            ret->bit_mask |= bit;
            ch = 0;
            continue;
        } else if (format[i] == '1') {
            ret->bit_mask |= bit;
            ret->expected |= bit;
            ch = 0;
            continue;
        } else if (format[i] == '-') {
            ch = 0;
            continue;
        }

        // Ban some characters
        ASSERT(format[i] != 'I');
        ASSERT(format[i] != 'l');
        ASSERT(format[i] != 'O');

        if (format[i] != ch){
            j++;
            ASSERT(j < N);
            ch = format[i];
        }

        ret->masks[j] |= bit;
        ret->shifts[j] = 31 - i;
    }

    ASSERT(j == N-1);

    return ret;
}

static const std::array<Instruction, 246> arm_instruction_table = {{
    // Barrier instructions
    { "DSB",                 MakeMatcher<0>("1111010101111111111100000100----", &Visitor::DSB) },
    { "DMB",                 MakeMatcher<0>("1111010101111111111100000101----", &Visitor::DMB) },
    { "ISB",                 MakeMatcher<0>("1111010101111111111100000110----", &Visitor::ISB) },

    // Branch instructions
    { "BLX (immediate)",     MakeMatcher<2>("1111101hvvvvvvvvvvvvvvvvvvvvvvvv", &Visitor::BLX_imm) },
    { "BLX (register)",      MakeMatcher<2>("cccc000100101111111111110011mmmm", &Visitor::BLX_reg) },
    { "B",                   MakeMatcher<2>("cccc1010vvvvvvvvvvvvvvvvvvvvvvvv", &Visitor::B) },
    { "BL",                  MakeMatcher<2>("cccc1011vvvvvvvvvvvvvvvvvvvvvvvv", &Visitor::BL) },
    { "BX",                  MakeMatcher<2>("cccc000100101111111111110001mmmm", &Visitor::BX) },
    { "BXJ",                 MakeMatcher<2>("cccc000100101111111111110010mmmm", &Visitor::BXJ) },

    // Coprocessor instructions
    { "CDP2",                MakeMatcher<0>("11111110-------------------1----", &Visitor::CDP) },
    { "CDP",                 MakeMatcher<0>("----1110-------------------0----", &Visitor::CDP) },
    { "LDC2",                MakeMatcher<0>("1111110----1--------------------", &Visitor::LDC) },
    { "LDC",                 MakeMatcher<0>("----110----1--------------------", &Visitor::LDC) },
    { "MCR2",                MakeMatcher<0>("----1110---0---------------1----", &Visitor::MCR) },
    { "MCR",                 MakeMatcher<0>("----1110---0---------------1----", &Visitor::MCR) },
    { "MCRR2",               MakeMatcher<0>("111111000100--------------------", &Visitor::MCRR) },
    { "MCRR",                MakeMatcher<0>("----11000100--------------------", &Visitor::MCRR) },
    { "MRC2",                MakeMatcher<0>("11111110---1---------------1----", &Visitor::MRC) },
    { "MRC",                 MakeMatcher<0>("----1110---1---------------1----", &Visitor::MRC) },
    { "MRRC2",               MakeMatcher<0>("111111000101--------------------", &Visitor::MRRC) },
    { "MRRC",                MakeMatcher<0>("----11000101--------------------", &Visitor::MRRC) },
    { "STC2",                MakeMatcher<0>("1111110----0--------------------", &Visitor::STC) },
    { "STC",                 MakeMatcher<0>("----110----0--------------------", &Visitor::STC) },

    // Data Processing instructions
    { "ADC (imm)",           MakeMatcher<6>("cccc0010101Snnnnddddrrrrvvvvvvvv", &Visitor::ADC_imm) },
    { "ADC (reg)",           MakeMatcher<7>("cccc0000101Snnnnddddvvvvvrr0mmmm", &Visitor::ADC_reg) },
    { "ADC (rsr)",           MakeMatcher<7>("cccc0000101Snnnnddddssss0rr1mmmm", &Visitor::ADC_rsr) },
    { "ADD (imm)",           MakeMatcher<6>("cccc0010100Snnnnddddrrrrvvvvvvvv", &Visitor::ADD_imm) },
    { "ADD (reg)",           MakeMatcher<7>("cccc0000100Snnnnddddvvvvvrr0mmmm", &Visitor::ADD_reg) },
    { "ADD (rsr)",           MakeMatcher<7>("cccc0000100Snnnnddddssss0rr1mmmm", &Visitor::ADD_rsr) },
    { "AND (imm)",           MakeMatcher<6>("cccc0010000Snnnnddddrrrrvvvvvvvv", &Visitor::AND_imm) },
    { "AND (reg)",           MakeMatcher<7>("cccc0000000Snnnnddddvvvvvrr0mmmm", &Visitor::AND_reg) },
    { "AND (rsr)",           MakeMatcher<7>("cccc0000000Snnnnddddssss0rr1mmmm", &Visitor::AND_rsr) },
    { "BIC (imm)",           MakeMatcher<6>("cccc0011110Snnnnddddrrrrvvvvvvvv", &Visitor::BIC_imm) },
    { "BIC (reg)",           MakeMatcher<7>("cccc0001110Snnnnddddvvvvvrr0mmmm", &Visitor::BIC_reg) },
    { "BIC (rsr)",           MakeMatcher<7>("cccc0001110Snnnnddddssss0rr1mmmm", &Visitor::BIC_rsr) },
    { "CMN (imm)",           MakeMatcher<4>("cccc00110111nnnn0000rrrrvvvvvvvv", &Visitor::CMN_imm) },
    { "CMN (reg)",           MakeMatcher<5>("cccc00010111nnnn0000vvvvvrr0mmmm", &Visitor::CMN_reg) },
    { "CMN (rsr)",           MakeMatcher<5>("cccc00010111nnnn0000ssss0rr1mmmm", &Visitor::CMN_rsr) },
    { "CMP (imm)",           MakeMatcher<4>("cccc00110101nnnn0000rrrrvvvvvvvv", &Visitor::CMP_imm) },
    { "CMP (reg)",           MakeMatcher<5>("cccc00010101nnnn0000vvvvvrr0mmmm", &Visitor::CMP_reg) },
    { "CMP (rsr)",           MakeMatcher<5>("cccc00010101nnnn0000ssss0rr1mmmm", &Visitor::CMP_rsr) },
    { "EOR (imm)",           MakeMatcher<6>("cccc0010001Snnnnddddrrrrvvvvvvvv", &Visitor::EOR_imm) },
    { "EOR (reg)",           MakeMatcher<7>("cccc0000001Snnnnddddvvvvvrr0mmmm", &Visitor::EOR_reg) },
    { "EOR (rsr)",           MakeMatcher<7>("cccc0000001Snnnnddddssss0rr1mmmm", &Visitor::EOR_rsr) },
    { "MOV (imm)",           MakeMatcher<5>("cccc0011101S0000ddddrrrrvvvvvvvv", &Visitor::MOV_imm) },
    { "MOV (reg)",           MakeMatcher<6>("cccc0001101S0000ddddvvvvvrr0mmmm", &Visitor::MOV_reg) },
    { "MOV (rsr)",           MakeMatcher<6>("cccc0001101S0000ddddssss0rr1mmmm", &Visitor::MOV_rsr) },
    { "MVN (imm)",           MakeMatcher<5>("cccc0011111S0000ddddrrrrvvvvvvvv", &Visitor::MVN_imm) },
    { "MVN (reg)",           MakeMatcher<6>("cccc0001111S0000ddddvvvvvrr0mmmm", &Visitor::MVN_reg) },
    { "MVN (rsr)",           MakeMatcher<6>("cccc0001111S0000ddddssss0rr1mmmm", &Visitor::MVN_rsr) },
    { "ORR (imm)",           MakeMatcher<6>("cccc0011100Snnnnddddrrrrvvvvvvvv", &Visitor::ORR_imm) },
    { "ORR (reg)",           MakeMatcher<7>("cccc0001100Snnnnddddvvvvvrr0mmmm", &Visitor::ORR_reg) },
    { "ORR (rsr)",           MakeMatcher<7>("cccc0001100Snnnnddddssss0rr1mmmm", &Visitor::ORR_rsr) },
    { "RSB (imm)",           MakeMatcher<6>("cccc0010011Snnnnddddrrrrvvvvvvvv", &Visitor::RSB_imm) },
    { "RSB (reg)",           MakeMatcher<7>("cccc0000011Snnnnddddvvvvvrr0mmmm", &Visitor::RSB_reg) },
    { "RSB (rsr)",           MakeMatcher<7>("cccc0000011Snnnnddddssss0rr1mmmm", &Visitor::RSB_rsr) },
    { "RSC (imm)",           MakeMatcher<6>("cccc0010111Snnnnddddrrrrvvvvvvvv", &Visitor::RSC_imm) },
    { "RSC (reg)",           MakeMatcher<7>("cccc0000111Snnnnddddvvvvvrr0mmmm", &Visitor::RSC_reg) },
    { "RSC (rsr)",           MakeMatcher<7>("cccc0000111Snnnnddddssss0rr1mmmm", &Visitor::RSC_rsr) },
    { "SBC (imm)",           MakeMatcher<6>("cccc0010110Snnnnddddrrrrvvvvvvvv", &Visitor::SBC_imm) },
    { "SBC (reg)",           MakeMatcher<7>("cccc0000110Snnnnddddvvvvvrr0mmmm", &Visitor::SBC_reg) },
    { "SBC (rsr)",           MakeMatcher<7>("cccc0000110Snnnnddddssss0rr1mmmm", &Visitor::SBC_rsr) },
    { "SUB (imm)",           MakeMatcher<6>("cccc0010010Snnnnddddrrrrvvvvvvvv", &Visitor::SUB_imm) },
    { "SUB (reg)",           MakeMatcher<7>("cccc0000010Snnnnddddvvvvvrr0mmmm", &Visitor::SUB_reg) },
    { "SUB (rsr)",           MakeMatcher<7>("cccc0000010Snnnnddddssss0rr1mmmm", &Visitor::SUB_rsr) },
    { "TEQ (imm)",           MakeMatcher<4>("cccc00110011nnnn0000rrrrvvvvvvvv", &Visitor::TEQ_imm) },
    { "TEQ (reg)",           MakeMatcher<5>("cccc00010011nnnn0000vvvvvrr0mmmm", &Visitor::TEQ_reg) },
    { "TEQ (rsr)",           MakeMatcher<5>("cccc00010011nnnn0000ssss0rr1mmmm", &Visitor::TEQ_rsr) },
    { "TST (imm)",           MakeMatcher<4>("cccc00110001nnnn0000rrrrvvvvvvvv", &Visitor::TST_imm) },
    { "TST (reg)",           MakeMatcher<5>("cccc00010001nnnn0000vvvvvrr0mmmm", &Visitor::TST_reg) },
    { "TST (rsr)",           MakeMatcher<5>("cccc00010001nnnn0000ssss0rr1mmmm", &Visitor::TST_rsr) },

    // Exception Generating instructions
    { "BKPT",                MakeMatcher<0>("----00010010------------0111----", &Visitor::BKPT) },
    { "HVC",                 MakeMatcher<0>("----00010100------------0111----", &Visitor::HVC) },
    { "SMC",                 MakeMatcher<0>("----000101100000000000000111----", &Visitor::SMC) },
    { "SVC",                 MakeMatcher<0>("----1111------------------------", &Visitor::SVC) },
    { "UDF",                 MakeMatcher<0>("111001111111------------1111----", &Visitor::UDF) },

    // Extension instructions
    { "SXTB",                MakeMatcher<0>("----011010101111------000111----", &Visitor::SXTB) },
    { "SXTB16",              MakeMatcher<0>("----011010001111------000111----", &Visitor::SXTB16) },
    { "SXTH",                MakeMatcher<0>("----011010111111------000111----", &Visitor::SXTH) },
    { "SXTAB",               MakeMatcher<0>("----01101010----------000111----", &Visitor::SXTAB) },
    { "SXTAB16",             MakeMatcher<0>("----01101000----------000111----", &Visitor::SXTAB16) },
    { "SXTAH",               MakeMatcher<0>("----01101011----------000111----", &Visitor::SXTAH) },
    { "UXTB",                MakeMatcher<0>("----011011101111------000111----", &Visitor::UXTB) },
    { "UXTB16",              MakeMatcher<0>("----011011001111------000111----", &Visitor::UXTB16) },
    { "UXTH",                MakeMatcher<0>("----011011111111------000111----", &Visitor::UXTH) },
    { "UXTAB",               MakeMatcher<0>("----01101110----------000111----", &Visitor::UXTAB) },
    { "UXTAB16",             MakeMatcher<0>("----01101100----------000111----", &Visitor::UXTAB16) },
    { "UXTAH",               MakeMatcher<0>("----01101111----------000111----", &Visitor::UXTAH) },

    // Hint instructions
    { "DBG",                 MakeMatcher<0>("----001100100000111100001111----", &Visitor::DBG) },
    { "PLD (imm)",           MakeMatcher<0>("11110101-101----1111------------", &Visitor::PLD) },
    { "PLD (lit)",           MakeMatcher<0>("11110101010111111111------------", &Visitor::PLD) },
    { "PLD (reg)",           MakeMatcher<0>("11110111-001----1111-------0----", &Visitor::PLD) },
    { "PLDW (imm)",          MakeMatcher<0>("11110101-001----1111------------", &Visitor::PLD) },
    { "PLDW (reg)",          MakeMatcher<0>("11110111-101----1111-------0----", &Visitor::PLD) },
    { "PLI (imm lit)",       MakeMatcher<0>("11110100-101----1111------------", &Visitor::PLI) },
    { "PLI (reg)",           MakeMatcher<0>("11110110-101----1111-------0----", &Visitor::PLI) },

    // Synchronization Primitive instructions
    { "CLREX",               MakeMatcher<0>("11110101011111111111000000011111", &Visitor::CLREX) },
    { "LDREX",               MakeMatcher<0>("----00011001--------111110011111", &Visitor::LDREX) },
    { "LDREXB",              MakeMatcher<0>("----00011101--------111110011111", &Visitor::LDREXB) },
    { "LDREXD",              MakeMatcher<0>("----00011011--------111110011111", &Visitor::LDREXD) },
    { "LDREXH",              MakeMatcher<0>("----00011111--------111110011111", &Visitor::LDREXH) },
    { "STREX",               MakeMatcher<0>("----00011000--------11111001----", &Visitor::STREX) },
    { "STREXB",              MakeMatcher<0>("----00011100--------11111001----", &Visitor::STREXB) },
    { "STREXD",              MakeMatcher<0>("----00011010--------11111001----", &Visitor::STREXD) },
    { "STREXH",              MakeMatcher<0>("----00011110--------11111001----", &Visitor::STREXH) },
    { "SWP",                 MakeMatcher<0>("----00010-00--------00001001----", &Visitor::SWP) },

    // Load/Store instructions
    { "LDR (imm)",           MakeMatcher<0>("----010--0-1--------------------", &Visitor::LDR_imm) },
    { "LDR (reg)",           MakeMatcher<0>("----011--0-1---------------0----", &Visitor::LDR_reg) },
    { "LDRB (imm)",          MakeMatcher<0>("----010--1-1--------------------", &Visitor::LDRB_imm) },
    { "LDRB (reg)",          MakeMatcher<0>("----011--1-1---------------0----", &Visitor::LDRB_reg) },
    { "LDRBT (A1)",          MakeMatcher<0>("----0100-111--------------------", &Visitor::LDRBT) },
    { "LDRBT (A2)",          MakeMatcher<0>("----0110-111---------------0----", &Visitor::LDRBT) },
    { "LDRD (imm)",          MakeMatcher<0>("----000--1-0------------1101----", &Visitor::LDRD_imm) },
    { "LDRD (reg)",          MakeMatcher<0>("----000--0-0--------00001101----", &Visitor::LDRD_reg) },
    { "LDRH (imm)",          MakeMatcher<0>("----000--1-1------------1011----", &Visitor::LDRH_imm) },
    { "LDRH (reg)",          MakeMatcher<0>("----000--0-1--------00001011----", &Visitor::LDRH_reg) },
    { "LDRHT (A1)",          MakeMatcher<0>("----0000-111------------1011----", &Visitor::LDRHT) },
    { "LDRHT (A2)",          MakeMatcher<0>("----0000-011--------00001011----", &Visitor::LDRHT) },
    { "LDRSB (imm)",         MakeMatcher<0>("----000--1-1------------1101----", &Visitor::LDRSB_imm) },
    { "LDRSB (reg)",         MakeMatcher<0>("----000--0-1--------00001101----", &Visitor::LDRSB_reg) },
    { "LDRSBT (A1)",         MakeMatcher<0>("----0000-111------------1101----", &Visitor::LDRSBT) },
    { "LDRSBT (A2)",         MakeMatcher<0>("----0000-011--------00001101----", &Visitor::LDRSBT) },
    { "LDRSH (imm)",         MakeMatcher<0>("----000--1-1------------1111----", &Visitor::LDRSH_imm) },
    { "LDRSH (reg)",         MakeMatcher<0>("----000--0-1--------00001111----", &Visitor::LDRSH_reg) },
    { "LDRSHT (A1)",         MakeMatcher<0>("----0000-111------------1111----", &Visitor::LDRSHT) },
    { "LDRSHT (A2)",         MakeMatcher<0>("----0000-011--------00001111----", &Visitor::LDRSHT) },
    { "LDRT (A1)",           MakeMatcher<0>("----0100-011--------------------", &Visitor::LDRT) },
    { "LDRT (A2)",           MakeMatcher<0>("----0110-011---------------0----", &Visitor::LDRT) },
    { "STR (imm)",           MakeMatcher<0>("----010--0-0--------------------", &Visitor::STR_imm) },
    { "STR (reg)",           MakeMatcher<0>("----011--0-0---------------0----", &Visitor::STR_reg) },
    { "STRB (imm)",          MakeMatcher<0>("----010--1-0--------------------", &Visitor::STRB_imm) },
    { "STRB (reg)",          MakeMatcher<0>("----011--1-0---------------0----", &Visitor::STRB_reg) },
    { "STRBT (A1)",          MakeMatcher<0>("----0100-110--------------------", &Visitor::STRBT) },
    { "STRBT (A2)",          MakeMatcher<0>("----0110-110---------------0----", &Visitor::STRBT) },
    { "STRD (imm)",          MakeMatcher<0>("----000--1-0------------1111----", &Visitor::STRD_imm) },
    { "STRD (reg)",          MakeMatcher<0>("----000--0-0--------00001111----", &Visitor::STRD_reg) },
    { "STRH (imm)",          MakeMatcher<0>("----000--1-0------------1011----", &Visitor::STRH_imm) },
    { "STRH (reg)",          MakeMatcher<0>("----000--0-0--------00001011----", &Visitor::STRH_reg) },
    { "STRHT (A1)",          MakeMatcher<0>("----0000-110------------1011----", &Visitor::STRHT) },
    { "STRHT (A2)",          MakeMatcher<0>("----0000-010--------00001011----", &Visitor::STRHT) },
    { "STRT (A1)",           MakeMatcher<0>("----0100-010--------------------", &Visitor::STRT) },
    { "STRT (A2)",           MakeMatcher<0>("----0110-010---------------0----", &Visitor::STRT) },

    // Load/Store Multiple instructions
    { "LDMIA/LDMFD",         MakeMatcher<0>("----100010-1--------------------", &Visitor::LDM) },
    { "LDMDA/LDMFA",         MakeMatcher<0>("----100000-1--------------------", &Visitor::LDM) },
    { "LDMDB/LDMEA",         MakeMatcher<0>("----100100-1--------------------", &Visitor::LDM) },
    { "LDMIB/LDMED",         MakeMatcher<0>("----100110-1--------------------", &Visitor::LDM) },
    { "LDM (exc ret)",       MakeMatcher<0>("----100--1-1----1---------------", &Visitor::LDM) },
    { "LDM (usr reg)",       MakeMatcher<0>("----100--1-1----0---------------", &Visitor::LDM) },
    { "POP",                 MakeMatcher<0>("----100010111101----------------", &Visitor::LDM) },
    { "POP",                 MakeMatcher<0>("----010010011101----000000000100", &Visitor::LDM) },
    { "PUSH",                MakeMatcher<0>("----100100101101----------------", &Visitor::STM) },
    { "PUSH",                MakeMatcher<0>("----010100101101----000000000100", &Visitor::STM) },
    { "STMIA/STMEA",         MakeMatcher<0>("----100010-0--------------------", &Visitor::STM) },
    { "STMDA/STMED",         MakeMatcher<0>("----100000-0--------------------", &Visitor::STM) },
    { "STMDB/STMFD",         MakeMatcher<0>("----100100-0--------------------", &Visitor::STM) },
    { "STMIB/STMFA",         MakeMatcher<0>("----100110-0--------------------", &Visitor::STM) },
    { "STMIB (usr reg)",     MakeMatcher<0>("----100--100--------------------", &Visitor::STM) },

    // Miscellaneous instructions
    { "CLZ",                 MakeMatcher<0>("----000101101111----11110001----", &Visitor::CLZ) },
    { "NOP",                 MakeMatcher<0>("----0011001000001111000000000000", &Visitor::NOP) },
    { "SEL",                 MakeMatcher<0>("----01101000--------11111011----", &Visitor::SEL) },

    // Unsigned Sum of Absolute Differences instructions
    { "USAD8",               MakeMatcher<0>("----01111000----1111----0001----", &Visitor::USAD8) },
    { "USADA8",              MakeMatcher<0>("----01111000------------0001----", &Visitor::USADA8) },

    // Packing instructions
    { "PKH",                 MakeMatcher<0>("----01101000--------------01----", &Visitor::PKH) },

    // Reversal instructions
    { "RBIT",                MakeMatcher<0>("----011011111111----11110011----", &Visitor::RBIT) },
    { "REV",                 MakeMatcher<0>("----011010111111----11110011----", &Visitor::REV) },
    { "REV16",               MakeMatcher<0>("----011010111111----11111011----", &Visitor::REV16) },
    { "REVSH",               MakeMatcher<0>("----011011111111----11111011----", &Visitor::REVSH) },

    // Saturation instructions
    { "SSAT",                MakeMatcher<0>("----0110101---------------01----", &Visitor::SSAT) },
    { "SSAT16",              MakeMatcher<0>("----01101010--------11110011----", &Visitor::SSAT16) },
    { "USAT",                MakeMatcher<0>("----0110111---------------01----", &Visitor::USAT) },
    { "USAT16",              MakeMatcher<0>("----01101110--------11110011----", &Visitor::USAT16) },

    // Multiply (Normal) instructions
    { "MLA",                 MakeMatcher<0>("----0000001-------------1001----", &Visitor::MLA) },
    { "MLS",                 MakeMatcher<0>("----00000110------------1001----", &Visitor::MLS) },
    { "MUL",                 MakeMatcher<0>("----0000000-----0000----1001----", &Visitor::MUL) },

    // Multiply (Long) instructions
    { "SMLAL",               MakeMatcher<0>("----0000111-------------1001----", &Visitor::SMLAL) },
    { "SMULL",               MakeMatcher<0>("----0000110-------------1001----", &Visitor::SMULL) },
    { "UMAAL",               MakeMatcher<0>("----00000100------------1001----", &Visitor::UMAAL) },
    { "UMLAL",               MakeMatcher<0>("----0000101-------------1001----", &Visitor::UMLAL) },
    { "UMULL",               MakeMatcher<0>("----0000100-------------1001----", &Visitor::UMULL) },

    // Multiply (Halfword) instructions
    { "SMLALXY",             MakeMatcher<0>("----00010100------------1--0----", &Visitor::SMLALxy) },
    { "SMLAXY",              MakeMatcher<0>("----00010000------------1--0----", &Visitor::SMLAxy) },
    { "SMULXY",              MakeMatcher<0>("----00010110----0000----1--0----", &Visitor::SMULxy) },

    // Multiply (Word by Halfword) instructions
    { "SMLAWY",              MakeMatcher<0>("----00010010------------1-00----", &Visitor::SMLAWy) },
    { "SMULWY",              MakeMatcher<0>("----00010010----0000----1-10----", &Visitor::SMULWy) },

    // Multiply (Most Significant Word) instructions
    { "SMMUL",               MakeMatcher<0>("----01110101----1111----00-1----", &Visitor::SMMUL) },
    { "SMMLA",               MakeMatcher<0>("----01110101------------00-1----", &Visitor::SMMLA) },
    { "SMMLS",               MakeMatcher<0>("----01110101------------11-1----", &Visitor::SMMLS) },

    // Multiply (Dual) instructions
    { "SMLAD",               MakeMatcher<0>("----01110000------------00-1----", &Visitor::SMLAD) },
    { "SMLALD",              MakeMatcher<0>("----01110100------------00-1----", &Visitor::SMLALD) },
    { "SMLSD",               MakeMatcher<0>("----01110000------------01-1----", &Visitor::SMLSD) },
    { "SMLSLD",              MakeMatcher<0>("----01110100------------01-1----", &Visitor::SMLSLD) },
    { "SMUAD",               MakeMatcher<0>("----01110000----1111----00-1----", &Visitor::SMUAD) },
    { "SMUSD",               MakeMatcher<0>("----01110000----1111----01-1----", &Visitor::SMUSD) },

    // Parallel Add/Subtract (Modulo) instructions
    { "SADD8",               MakeMatcher<0>("----01100001--------11111001----", &Visitor::SADD8) },
    { "SADD16",              MakeMatcher<0>("----01100001--------11110001----", &Visitor::SADD16) },
    { "SASX",                MakeMatcher<0>("----01100001--------11110011----", &Visitor::SASX) },
    { "SSAX",                MakeMatcher<0>("----01100001--------11110101----", &Visitor::SSAX) },
    { "SSUB8",               MakeMatcher<0>("----01100001--------11111111----", &Visitor::SSUB8) },
    { "SSUB16",              MakeMatcher<0>("----01100001--------11110111----", &Visitor::SSUB16) },
    { "UADD8",               MakeMatcher<0>("----01100101--------11111001----", &Visitor::UADD8) },
    { "UADD16",              MakeMatcher<0>("----01100101--------11110001----", &Visitor::UADD16) },
    { "UASX",                MakeMatcher<0>("----01100101--------11110011----", &Visitor::UASX) },
    { "USAX",                MakeMatcher<0>("----01100101--------11110101----", &Visitor::USAX) },
    { "USUB8",               MakeMatcher<0>("----01100101--------11111111----", &Visitor::USUB8) },
    { "USUB16",              MakeMatcher<0>("----01100101--------11110111----", &Visitor::USUB16) },

    // Parallel Add/Subtract (Saturating) instructions
    { "QADD8",               MakeMatcher<0>("----01100010--------11111001----", &Visitor::QADD8) },
    { "QADD16",              MakeMatcher<0>("----01100010--------11110001----", &Visitor::QADD16) },
    { "QASX",                MakeMatcher<0>("----01100010--------11110011----", &Visitor::QASX) },
    { "QSAX",                MakeMatcher<0>("----01100010--------11110101----", &Visitor::QSAX) },
    { "QSUB8",               MakeMatcher<0>("----01100010--------11111111----", &Visitor::QSUB8) },
    { "QSUB16",              MakeMatcher<0>("----01100010--------11110111----", &Visitor::QSUB16) },
    { "UQADD8",              MakeMatcher<0>("----01100110--------11111001----", &Visitor::UQADD8) },
    { "UQADD16",             MakeMatcher<0>("----01100110--------11110001----", &Visitor::UQADD16) },
    { "UQASX",               MakeMatcher<0>("----01100110--------11110011----", &Visitor::UQASX) },
    { "UQSAX",               MakeMatcher<0>("----01100110--------11110101----", &Visitor::UQSAX) },
    { "UQSUB8",              MakeMatcher<0>("----01100110--------11111111----", &Visitor::UQSUB8) },
    { "UQSUB16",             MakeMatcher<0>("----01100110--------11110111----", &Visitor::UQSUB16) },

    // Parallel Add/Subtract (Halving) instructions
    { "SHADD8",              MakeMatcher<0>("----01100011--------11111001----", &Visitor::SHADD8) },
    { "SHADD16",             MakeMatcher<0>("----01100011--------11110001----", &Visitor::SHADD16) },
    { "SHASX",               MakeMatcher<0>("----01100011--------11110011----", &Visitor::SHASX) },
    { "SHSAX",               MakeMatcher<0>("----01100011--------11110101----", &Visitor::SHSAX) },
    { "SHSUB8",              MakeMatcher<0>("----01100011--------11111111----", &Visitor::SHSUB8) },
    { "SHSUB16",             MakeMatcher<0>("----01100011--------11110111----", &Visitor::SHSUB16) },
    { "UHADD8",              MakeMatcher<0>("----01100111--------11111001----", &Visitor::UHADD8) },
    { "UHADD16",             MakeMatcher<0>("----01100111--------11110001----", &Visitor::UHADD16) },
    { "UHASX",               MakeMatcher<0>("----01100111--------11110011----", &Visitor::UHASX) },
    { "UHSAX",               MakeMatcher<0>("----01100111--------11110101----", &Visitor::UHSAX) },
    { "UHSUB8",              MakeMatcher<0>("----01100111--------11111111----", &Visitor::UHSUB8) },
    { "UHSUB16",             MakeMatcher<0>("----01100111--------11110111----", &Visitor::UHSUB16) },

    // Saturated Add/Subtract instructions
    { "QADD",                MakeMatcher<0>("----00010000--------00000101----", &Visitor::QADD) },
    { "QSUB",                MakeMatcher<0>("----00010010--------00000101----", &Visitor::QSUB) },
    { "QDADD",               MakeMatcher<0>("----00010100--------00000101----", &Visitor::QDADD) },
    { "QDSUB",               MakeMatcher<0>("----00010110--------00000101----", &Visitor::QDSUB) },

    // Status Register Access instructions
    { "CPS",                 MakeMatcher<0>("----00010000---00000000---0-----", &Visitor::CPS) },
    { "ERET",                MakeMatcher<0>("----0001011000000000000001101110", &Visitor::ERET) },
    { "SETEND",              MakeMatcher<0>("1111000100000001000000-000000000", &Visitor::SETEND) },
    { "MRS",                 MakeMatcher<0>("----000100001111----000000000000", &Visitor::MRS) },
    { "MRS (banked)",        MakeMatcher<0>("----00010-00--------001-00000000", &Visitor::MRS) },
    { "MRS (system)",        MakeMatcher<0>("----00010-001111----000000000000", &Visitor::MRS) },
    { "MSR (imm)",           MakeMatcher<0>("----00110010--001111------------", &Visitor::MSR) },
    { "MSR (reg)",           MakeMatcher<0>("----00010010--00111100000000----", &Visitor::MSR) },
    { "MSR (banked)",        MakeMatcher<0>("----00010-10----1111001-0000----", &Visitor::MSR) },
    { "MSR (imm special)",   MakeMatcher<0>("----00110-10----1111------------", &Visitor::MSR) },
    { "MSR (reg special)",   MakeMatcher<0>("----00010-10----111100000000----", &Visitor::MSR) },
    { "RFE",                 MakeMatcher<0>("----0001101-0000---------110----", &Visitor::RFE) },
    { "SRS",                 MakeMatcher<0>("0000011--0-00000000000000001----", &Visitor::SRS) },
}};

const Instruction& DecodeArm(u32 i) {
    return *std::find_if(arm_instruction_table.cbegin(), arm_instruction_table.cend(), [i](const auto& instruction) {
        return instruction.Match(i);
    });
}

};
