// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <algorithm>
#include <cstddef>

#include "common/assert.h"
#include "common/common_types.h"

#include "core/arm/decoder/decoder.h"

namespace ArmDecoder {

namespace Impl {
    template<typename T, size_t N, typename... Args>
    struct Call {
        static void call(Visitor* v, T fn, u32* list, Args... args) {
            Call<T, N - 1, Args..., u32>::call(v, fn, ++list, args..., *list);
        }
    };

    template<typename T, typename... Args>
    struct Call<T, 0, Args...> {
        static void call(Visitor* v, T fn, u32* list, Args... args) {
            (v->*fn)(args...);
        }
    };

    template<size_t N, typename T>
    struct MatcherImpl : Matcher {
        std::array<u32, N> masks;
        std::array<size_t, N> shifts;
        T fn;
        virtual void visit(Visitor *v, u32 inst) override {
            std::array<u32, N> values;
            for (int i = 0; i<N; i++) {
                values[i] = (inst & masks[i]) >> shifts[i];
            }
            Call<T, N>::call(v, fn, values.data());
        }
    };
}

template<size_t N, typename T>
static std::unique_ptr<Matcher> MakeMatcher(const char* const format, T fn) {
    ASSERT(strlen(format) == 32);

    auto ret = new Impl::MatcherImpl<N, T>();
    ret->fn = fn;
    ret->masks.fill(0);
    ret->shifts.fill(0);

    char ch = 0;
    int j = -1;

    for (int i = 0; i < 32; i++) {
        if (format[i] == '0') {
            ret->bit_mask |= 1 << (31 - i);
            ch = 0;
            continue;
        } else if (format[i] == '1') {
            ret->bit_mask |= 1 << (31 - i);
            ret->expected |= 1 << (31 - i);
            ch = 0;
            continue;
        } else if (format[i] == 'x') {
            ch = 0;
            continue;
        }

        // Ban some characters
        ASSERT(format[i] != 'I');
        ASSERT(format[i] != 'l');
        ASSERT(format[i] != 'O');
        ASSERT(format[i] != 'i');

        if (format[i] != ch){
            j++;
            ASSERT(j < N);
            ch = format[i];
        }

        ret->masks[j] |= 1 << (31 - i);
        ret->shifts[j] = 31 - i;
    }

    ASSERT(j == N-1);

    return std::unique_ptr<Matcher>(ret);
}

static const std::array<Instruction, 246> arm_instruction_table = {{
    // Barrier instructions
    { "DSB",                 MakeMatcher<0>("1111010101111111111100000100xxxx", &Visitor::DSB) },
    { "DMB",                 MakeMatcher<0>("1111010101111111111100000101xxxx", &Visitor::DMB) },
    { "ISB",                 MakeMatcher<0>("1111010101111111111100000110xxxx", &Visitor::ISB) },

    // Branch instructions
    { "BLX (immediate)",     MakeMatcher<2>("1111101hvvvvvvvvvvvvvvvvvvvvvvvv", &Visitor::BLX_imm) },
    { "BLX (register)",      MakeMatcher<2>("cccc000100101111111111110011mmmm", &Visitor::BLX_reg) },
    { "B",                   MakeMatcher<2>("cccc1010vvvvvvvvvvvvvvvvvvvvvvvv", &Visitor::B) },
    { "BL",                  MakeMatcher<2>("cccc1011vvvvvvvvvvvvvvvvvvvvvvvv", &Visitor::BL) },
    { "BX",                  MakeMatcher<2>("cccc000100101111111111110001mmmm", &Visitor::BX) },
    { "BXJ",                 MakeMatcher<2>("cccc000100101111111111110010mmmm", &Visitor::BXJ) },

    // Coprocessor instructions
    { "CDP2",                MakeMatcher<0>("11111110xxxxxxxxxxxxxxxxxxx1xxxx", &Visitor::CDP) },
    { "CDP",                 MakeMatcher<0>("xxxx1110xxxxxxxxxxxxxxxxxxx0xxxx", &Visitor::CDP) },
    { "LDC2",                MakeMatcher<0>("1111110xxxx1xxxxxxxxxxxxxxxxxxxx", &Visitor::LDC) },
    { "LDC",                 MakeMatcher<0>("xxxx110xxxx1xxxxxxxxxxxxxxxxxxxx", &Visitor::LDC) },
    { "MCR2",                MakeMatcher<0>("xxxx1110xxx0xxxxxxxxxxxxxxx1xxxx", &Visitor::MCR) },
    { "MCR",                 MakeMatcher<0>("xxxx1110xxx0xxxxxxxxxxxxxxx1xxxx", &Visitor::MCR) },
    { "MCRR2",               MakeMatcher<0>("111111000100xxxxxxxxxxxxxxxxxxxx", &Visitor::MCRR) },
    { "MCRR",                MakeMatcher<0>("xxxx11000100xxxxxxxxxxxxxxxxxxxx", &Visitor::MCRR) },
    { "MRC2",                MakeMatcher<0>("11111110xxx1xxxxxxxxxxxxxxx1xxxx", &Visitor::MRC) },
    { "MRC",                 MakeMatcher<0>("xxxx1110xxx1xxxxxxxxxxxxxxx1xxxx", &Visitor::MRC) },
    { "MRRC2",               MakeMatcher<0>("111111000101xxxxxxxxxxxxxxxxxxxx", &Visitor::MRRC) },
    { "MRRC",                MakeMatcher<0>("xxxx11000101xxxxxxxxxxxxxxxxxxxx", &Visitor::MRRC) },
    { "STC2",                MakeMatcher<0>("1111110xxxx0xxxxxxxxxxxxxxxxxxxx", &Visitor::STC) },
    { "STC",                 MakeMatcher<0>("xxxx110xxxx0xxxxxxxxxxxxxxxxxxxx", &Visitor::STC) },

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
    { "BKPT",                MakeMatcher<0>("xxxx00010010xxxxxxxxxxxx0111xxxx", &Visitor::BKPT) },
    { "HVC",                 MakeMatcher<0>("xxxx00010100xxxxxxxxxxxx0111xxxx", &Visitor::HVC) },
    { "SMC",                 MakeMatcher<0>("xxxx000101100000000000000111xxxx", &Visitor::SMC) },
    { "SVC",                 MakeMatcher<0>("xxxx1111xxxxxxxxxxxxxxxxxxxxxxxx", &Visitor::SVC) },
    { "UDF",                 MakeMatcher<0>("111001111111xxxxxxxxxxxx1111xxxx", &Visitor::UDF) },

    // Extension instructions
    { "SXTB",                MakeMatcher<0>("xxxx011010101111xxxxxx000111xxxx", &Visitor::SXTB) },
    { "SXTB16",              MakeMatcher<0>("xxxx011010001111xxxxxx000111xxxx", &Visitor::SXTB16) },
    { "SXTH",                MakeMatcher<0>("xxxx011010111111xxxxxx000111xxxx", &Visitor::SXTH) },
    { "SXTAB",               MakeMatcher<0>("xxxx01101010xxxxxxxxxx000111xxxx", &Visitor::SXTAB) },
    { "SXTAB16",             MakeMatcher<0>("xxxx01101000xxxxxxxxxx000111xxxx", &Visitor::SXTAB16) },
    { "SXTAH",               MakeMatcher<0>("xxxx01101011xxxxxxxxxx000111xxxx", &Visitor::SXTAH) },
    { "UXTB",                MakeMatcher<0>("xxxx011011101111xxxxxx000111xxxx", &Visitor::UXTB) },
    { "UXTB16",              MakeMatcher<0>("xxxx011011001111xxxxxx000111xxxx", &Visitor::UXTB16) },
    { "UXTH",                MakeMatcher<0>("xxxx011011111111xxxxxx000111xxxx", &Visitor::UXTH) },
    { "UXTAB",               MakeMatcher<0>("xxxx01101110xxxxxxxxxx000111xxxx", &Visitor::UXTAB) },
    { "UXTAB16",             MakeMatcher<0>("xxxx01101100xxxxxxxxxx000111xxxx", &Visitor::UXTAB16) },
    { "UXTAH",               MakeMatcher<0>("xxxx01101111xxxxxxxxxx000111xxxx", &Visitor::UXTAH) },

    // Hint instructions
    { "DBG",                 MakeMatcher<0>("xxxx001100100000111100001111xxxx", &Visitor::DBG) },
    { "PLD (imm)",           MakeMatcher<0>("11110101x101xxxx1111xxxxxxxxxxxx", &Visitor::PLD) },
    { "PLD (lit)",           MakeMatcher<0>("11110101010111111111xxxxxxxxxxxx", &Visitor::PLD) },
    { "PLD (reg)",           MakeMatcher<0>("11110111x001xxxx1111xxxxxxx0xxxx", &Visitor::PLD) },
    { "PLDW (imm)",          MakeMatcher<0>("11110101x001xxxx1111xxxxxxxxxxxx", &Visitor::PLD) },
    { "PLDW (reg)",          MakeMatcher<0>("11110111x101xxxx1111xxxxxxx0xxxx", &Visitor::PLD) },
    { "PLI (imm lit)",       MakeMatcher<0>("11110100x101xxxx1111xxxxxxxxxxxx", &Visitor::PLI) },
    { "PLI (reg)",           MakeMatcher<0>("11110110x101xxxx1111xxxxxxx0xxxx", &Visitor::PLI) },

    // Synchronization Primitive instructions
    { "CLREX",               MakeMatcher<0>("11110101011111111111000000011111", &Visitor::CLREX) },
    { "LDREX",               MakeMatcher<0>("xxxx00011001xxxxxxxx111110011111", &Visitor::LDREX) },
    { "LDREXB",              MakeMatcher<0>("xxxx00011101xxxxxxxx111110011111", &Visitor::LDREXB) },
    { "LDREXD",              MakeMatcher<0>("xxxx00011011xxxxxxxx111110011111", &Visitor::LDREXD) },
    { "LDREXH",              MakeMatcher<0>("xxxx00011111xxxxxxxx111110011111", &Visitor::LDREXH) },
    { "STREX",               MakeMatcher<0>("xxxx00011000xxxxxxxx11111001xxxx", &Visitor::STREX) },
    { "STREXB",              MakeMatcher<0>("xxxx00011100xxxxxxxx11111001xxxx", &Visitor::STREXB) },
    { "STREXD",              MakeMatcher<0>("xxxx00011010xxxxxxxx11111001xxxx", &Visitor::STREXD) },
    { "STREXH",              MakeMatcher<0>("xxxx00011110xxxxxxxx11111001xxxx", &Visitor::STREXH) },
    { "SWP",                 MakeMatcher<0>("xxxx00010x00xxxxxxxx00001001xxxx", &Visitor::SWP) },

    // Load/Store instructions
    { "LDR (imm)",           MakeMatcher<0>("xxxx010xx0x1xxxxxxxxxxxxxxxxxxxx", &Visitor::LDR_imm) },
    { "LDR (reg)",           MakeMatcher<0>("xxxx011xx0x1xxxxxxxxxxxxxxx0xxxx", &Visitor::LDR_reg) },
    { "LDRB (imm)",          MakeMatcher<0>("xxxx010xx1x1xxxxxxxxxxxxxxxxxxxx", &Visitor::LDRB_imm) },
    { "LDRB (reg)",          MakeMatcher<0>("xxxx011xx1x1xxxxxxxxxxxxxxx0xxxx", &Visitor::LDRB_reg) },
    { "LDRBT (A1)",          MakeMatcher<0>("xxxx0100x111xxxxxxxxxxxxxxxxxxxx", &Visitor::LDRBT) },
    { "LDRBT (A2)",          MakeMatcher<0>("xxxx0110x111xxxxxxxxxxxxxxx0xxxx", &Visitor::LDRBT) },
    { "LDRD (imm)",          MakeMatcher<0>("xxxx000xx1x0xxxxxxxxxxxx1101xxxx", &Visitor::LDRD_imm) },
    { "LDRD (reg)",          MakeMatcher<0>("xxxx000xx0x0xxxxxxxx00001101xxxx", &Visitor::LDRD_reg) },
    { "LDRH (imm)",          MakeMatcher<0>("xxxx000xx1x1xxxxxxxxxxxx1011xxxx", &Visitor::LDRH_imm) },
    { "LDRH (reg)",          MakeMatcher<0>("xxxx000xx0x1xxxxxxxx00001011xxxx", &Visitor::LDRH_reg) },
    { "LDRHT (A1)",          MakeMatcher<0>("xxxx0000x111xxxxxxxxxxxx1011xxxx", &Visitor::LDRHT) },
    { "LDRHT (A2)",          MakeMatcher<0>("xxxx0000x011xxxxxxxx00001011xxxx", &Visitor::LDRHT) },
    { "LDRSB (imm)",         MakeMatcher<0>("xxxx000xx1x1xxxxxxxxxxxx1101xxxx", &Visitor::LDRSB_imm) },
    { "LDRSB (reg)",         MakeMatcher<0>("xxxx000xx0x1xxxxxxxx00001101xxxx", &Visitor::LDRSB_reg) },
    { "LDRSBT (A1)",         MakeMatcher<0>("xxxx0000x111xxxxxxxxxxxx1101xxxx", &Visitor::LDRSBT) },
    { "LDRSBT (A2)",         MakeMatcher<0>("xxxx0000x011xxxxxxxx00001101xxxx", &Visitor::LDRSBT) },
    { "LDRSH (imm)",         MakeMatcher<0>("xxxx000xx1x1xxxxxxxxxxxx1111xxxx", &Visitor::LDRSH_imm) },
    { "LDRSH (reg)",         MakeMatcher<0>("xxxx000xx0x1xxxxxxxx00001111xxxx", &Visitor::LDRSH_reg) },
    { "LDRSHT (A1)",         MakeMatcher<0>("xxxx0000x111xxxxxxxxxxxx1111xxxx", &Visitor::LDRSHT) },
    { "LDRSHT (A2)",         MakeMatcher<0>("xxxx0000x011xxxxxxxx00001111xxxx", &Visitor::LDRSHT) },
    { "LDRT (A1)",           MakeMatcher<0>("xxxx0100x011xxxxxxxxxxxxxxxxxxxx", &Visitor::LDRT) },
    { "LDRT (A2)",           MakeMatcher<0>("xxxx0110x011xxxxxxxxxxxxxxx0xxxx", &Visitor::LDRT) },
    { "STR (imm)",           MakeMatcher<0>("xxxx010xx0x0xxxxxxxxxxxxxxxxxxxx", &Visitor::STR_imm) },
    { "STR (reg)",           MakeMatcher<0>("xxxx011xx0x0xxxxxxxxxxxxxxx0xxxx", &Visitor::STR_reg) },
    { "STRB (imm)",          MakeMatcher<0>("xxxx010xx1x0xxxxxxxxxxxxxxxxxxxx", &Visitor::STRB_imm) },
    { "STRB (reg)",          MakeMatcher<0>("xxxx011xx1x0xxxxxxxxxxxxxxx0xxxx", &Visitor::STRB_reg) },
    { "STRBT (A1)",          MakeMatcher<0>("xxxx0100x110xxxxxxxxxxxxxxxxxxxx", &Visitor::STRBT) },
    { "STRBT (A2)",          MakeMatcher<0>("xxxx0110x110xxxxxxxxxxxxxxx0xxxx", &Visitor::STRBT) },
    { "STRD (imm)",          MakeMatcher<0>("xxxx000xx1x0xxxxxxxxxxxx1111xxxx", &Visitor::STRD_imm) },
    { "STRD (reg)",          MakeMatcher<0>("xxxx000xx0x0xxxxxxxx00001111xxxx", &Visitor::STRD_reg) },
    { "STRH (imm)",          MakeMatcher<0>("xxxx000xx1x0xxxxxxxxxxxx1011xxxx", &Visitor::STRH_imm) },
    { "STRH (reg)",          MakeMatcher<0>("xxxx000xx0x0xxxxxxxx00001011xxxx", &Visitor::STRH_reg) },
    { "STRHT (A1)",          MakeMatcher<0>("xxxx0000x110xxxxxxxxxxxx1011xxxx", &Visitor::STRHT) },
    { "STRHT (A2)",          MakeMatcher<0>("xxxx0000x010xxxxxxxx00001011xxxx", &Visitor::STRHT) },
    { "STRT (A1)",           MakeMatcher<0>("xxxx0100x010xxxxxxxxxxxxxxxxxxxx", &Visitor::STRT) },
    { "STRT (A2)",           MakeMatcher<0>("xxxx0110x010xxxxxxxxxxxxxxx0xxxx", &Visitor::STRT) },

    // Load/Store Multiple instructions
    { "LDMIA/LDMFD",         MakeMatcher<0>("xxxx100010x1xxxxxxxxxxxxxxxxxxxx", &Visitor::LDM) },
    { "LDMDA/LDMFA",         MakeMatcher<0>("xxxx100000x1xxxxxxxxxxxxxxxxxxxx", &Visitor::LDM) },
    { "LDMDB/LDMEA",         MakeMatcher<0>("xxxx100100x1xxxxxxxxxxxxxxxxxxxx", &Visitor::LDM) },
    { "LDMIB/LDMED",         MakeMatcher<0>("xxxx100110x1xxxxxxxxxxxxxxxxxxxx", &Visitor::LDM) },
    { "LDM (exc ret)",       MakeMatcher<0>("xxxx100xx1x1xxxx1xxxxxxxxxxxxxxx", &Visitor::LDM) },
    { "LDM (usr reg)",       MakeMatcher<0>("xxxx100xx1x1xxxx0xxxxxxxxxxxxxxx", &Visitor::LDM) },
    { "POP",                 MakeMatcher<0>("xxxx100010111101xxxxxxxxxxxxxxxx", &Visitor::LDM) },
    { "POP",                 MakeMatcher<0>("xxxx010010011101xxxx000000000100", &Visitor::LDM) },
    { "PUSH",                MakeMatcher<0>("xxxx100100101101xxxxxxxxxxxxxxxx", &Visitor::STM) },
    { "PUSH",                MakeMatcher<0>("xxxx010100101101xxxx000000000100", &Visitor::STM) },
    { "STMIA/STMEA",         MakeMatcher<0>("xxxx100010x0xxxxxxxxxxxxxxxxxxxx", &Visitor::STM) },
    { "STMDA/STMED",         MakeMatcher<0>("xxxx100000x0xxxxxxxxxxxxxxxxxxxx", &Visitor::STM) },
    { "STMDB/STMFD",         MakeMatcher<0>("xxxx100100x0xxxxxxxxxxxxxxxxxxxx", &Visitor::STM) },
    { "STMIB/STMFA",         MakeMatcher<0>("xxxx100110x0xxxxxxxxxxxxxxxxxxxx", &Visitor::STM) },
    { "STMIB (usr reg)",     MakeMatcher<0>("xxxx100xx100xxxxxxxxxxxxxxxxxxxx", &Visitor::STM) },

    // Miscellaneous instructions
    { "CLZ",                 MakeMatcher<0>("xxxx000101101111xxxx11110001xxxx", &Visitor::CLZ) },
    { "NOP",                 MakeMatcher<0>("xxxx0011001000001111000000000000", &Visitor::NOP) },
    { "SEL",                 MakeMatcher<0>("xxxx01101000xxxxxxxx11111011xxxx", &Visitor::SEL) },

    // Unsigned Sum of Absolute Differences instructions
    { "USAD8",               MakeMatcher<0>("xxxx01111000xxxx1111xxxx0001xxxx", &Visitor::USAD8) },
    { "USADA8",              MakeMatcher<0>("xxxx01111000xxxxxxxxxxxx0001xxxx", &Visitor::USADA8) },

    // Packing instructions
    { "PKH",                 MakeMatcher<0>("xxxx01101000xxxxxxxxxxxxxx01xxxx", &Visitor::PKH) },

    // Reversal instructions
    { "RBIT",                MakeMatcher<0>("xxxx011011111111xxxx11110011xxxx", &Visitor::RBIT) },
    { "REV",                 MakeMatcher<0>("xxxx011010111111xxxx11110011xxxx", &Visitor::REV) },
    { "REV16",               MakeMatcher<0>("xxxx011010111111xxxx11111011xxxx", &Visitor::REV16) },
    { "REVSH",               MakeMatcher<0>("xxxx011011111111xxxx11111011xxxx", &Visitor::REVSH) },

    // Saturation instructions
    { "SSAT",                MakeMatcher<0>("xxxx0110101xxxxxxxxxxxxxxx01xxxx", &Visitor::SSAT) },
    { "SSAT16",              MakeMatcher<0>("xxxx01101010xxxxxxxx11110011xxxx", &Visitor::SSAT16) },
    { "USAT",                MakeMatcher<0>("xxxx0110111xxxxxxxxxxxxxxx01xxxx", &Visitor::USAT) },
    { "USAT16",              MakeMatcher<0>("xxxx01101110xxxxxxxx11110011xxxx", &Visitor::USAT16) },

    // Multiply (Normal) instructions
    { "MLA",                 MakeMatcher<0>("xxxx0000001xxxxxxxxxxxxx1001xxxx", &Visitor::MLA) },
    { "MLS",                 MakeMatcher<0>("xxxx00000110xxxxxxxxxxxx1001xxxx", &Visitor::MLS) },
    { "MUL",                 MakeMatcher<0>("xxxx0000000xxxxx0000xxxx1001xxxx", &Visitor::MUL) },

    // Multiply (Long) instructions
    { "SMLAL",               MakeMatcher<0>("xxxx0000111xxxxxxxxxxxxx1001xxxx", &Visitor::SMLAL) },
    { "SMULL",               MakeMatcher<0>("xxxx0000110xxxxxxxxxxxxx1001xxxx", &Visitor::SMULL) },
    { "UMAAL",               MakeMatcher<0>("xxxx00000100xxxxxxxxxxxx1001xxxx", &Visitor::UMAAL) },
    { "UMLAL",               MakeMatcher<0>("xxxx0000101xxxxxxxxxxxxx1001xxxx", &Visitor::UMLAL) },
    { "UMULL",               MakeMatcher<0>("xxxx0000100xxxxxxxxxxxxx1001xxxx", &Visitor::UMULL) },

    // Multiply (Halfword) instructions
    { "SMLALXY",             MakeMatcher<0>("xxxx00010100xxxxxxxxxxxx1xx0xxxx", &Visitor::SMLALxy) },
    { "SMLAXY",              MakeMatcher<0>("xxxx00010000xxxxxxxxxxxx1xx0xxxx", &Visitor::SMLAxy) },
    { "SMULXY",              MakeMatcher<0>("xxxx00010110xxxx0000xxxx1xx0xxxx", &Visitor::SMULxy) },

    // Multiply (Word by Halfword) instructions
    { "SMLAWY",              MakeMatcher<0>("xxxx00010010xxxxxxxxxxxx1x00xxxx", &Visitor::SMLAWy) },
    { "SMULWY",              MakeMatcher<0>("xxxx00010010xxxx0000xxxx1x10xxxx", &Visitor::SMULWy) },

    // Multiply (Most Significant Word) instructions
    { "SMMUL",               MakeMatcher<0>("xxxx01110101xxxx1111xxxx00x1xxxx", &Visitor::SMMUL) },
    { "SMMLA",               MakeMatcher<0>("xxxx01110101xxxxxxxxxxxx00x1xxxx", &Visitor::SMMLA) },
    { "SMMLS",               MakeMatcher<0>("xxxx01110101xxxxxxxxxxxx11x1xxxx", &Visitor::SMMLS) },

    // Multiply (Dual) instructions
    { "SMLAD",               MakeMatcher<0>("xxxx01110000xxxxxxxxxxxx00x1xxxx", &Visitor::SMLAD) },
    { "SMLALD",              MakeMatcher<0>("xxxx01110100xxxxxxxxxxxx00x1xxxx", &Visitor::SMLALD) },
    { "SMLSD",               MakeMatcher<0>("xxxx01110000xxxxxxxxxxxx01x1xxxx", &Visitor::SMLSD) },
    { "SMLSLD",              MakeMatcher<0>("xxxx01110100xxxxxxxxxxxx01x1xxxx", &Visitor::SMLSLD) },
    { "SMUAD",               MakeMatcher<0>("xxxx01110000xxxx1111xxxx00x1xxxx", &Visitor::SMUAD) },
    { "SMUSD",               MakeMatcher<0>("xxxx01110000xxxx1111xxxx01x1xxxx", &Visitor::SMUSD) },

    // Parallel Add/Subtract (Modulo) instructions
    { "SADD8",               MakeMatcher<0>("xxxx01100001xxxxxxxx11111001xxxx", &Visitor::SADD8) },
    { "SADD16",              MakeMatcher<0>("xxxx01100001xxxxxxxx11110001xxxx", &Visitor::SADD16) },
    { "SASX",                MakeMatcher<0>("xxxx01100001xxxxxxxx11110011xxxx", &Visitor::SASX) },
    { "SSAX",                MakeMatcher<0>("xxxx01100001xxxxxxxx11110101xxxx", &Visitor::SSAX) },
    { "SSUB8",               MakeMatcher<0>("xxxx01100001xxxxxxxx11111111xxxx", &Visitor::SSUB8) },
    { "SSUB16",              MakeMatcher<0>("xxxx01100001xxxxxxxx11110111xxxx", &Visitor::SSUB16) },
    { "UADD8",               MakeMatcher<0>("xxxx01100101xxxxxxxx11111001xxxx", &Visitor::UADD8) },
    { "UADD16",              MakeMatcher<0>("xxxx01100101xxxxxxxx11110001xxxx", &Visitor::UADD16) },
    { "UASX",                MakeMatcher<0>("xxxx01100101xxxxxxxx11110011xxxx", &Visitor::UASX) },
    { "USAX",                MakeMatcher<0>("xxxx01100101xxxxxxxx11110101xxxx", &Visitor::USAX) },
    { "USUB8",               MakeMatcher<0>("xxxx01100101xxxxxxxx11111111xxxx", &Visitor::USUB8) },
    { "USUB16",              MakeMatcher<0>("xxxx01100101xxxxxxxx11110111xxxx", &Visitor::USUB16) },

    // Parallel Add/Subtract (Saturating) instructions
    { "QADD8",               MakeMatcher<0>("xxxx01100010xxxxxxxx11111001xxxx", &Visitor::QADD8) },
    { "QADD16",              MakeMatcher<0>("xxxx01100010xxxxxxxx11110001xxxx", &Visitor::QADD16) },
    { "QASX",                MakeMatcher<0>("xxxx01100010xxxxxxxx11110011xxxx", &Visitor::QASX) },
    { "QSAX",                MakeMatcher<0>("xxxx01100010xxxxxxxx11110101xxxx", &Visitor::QSAX) },
    { "QSUB8",               MakeMatcher<0>("xxxx01100010xxxxxxxx11111111xxxx", &Visitor::QSUB8) },
    { "QSUB16",              MakeMatcher<0>("xxxx01100010xxxxxxxx11110111xxxx", &Visitor::QSUB16) },
    { "UQADD8",              MakeMatcher<0>("xxxx01100110xxxxxxxx11111001xxxx", &Visitor::UQADD8) },
    { "UQADD16",             MakeMatcher<0>("xxxx01100110xxxxxxxx11110001xxxx", &Visitor::UQADD16) },
    { "UQASX",               MakeMatcher<0>("xxxx01100110xxxxxxxx11110011xxxx", &Visitor::UQASX) },
    { "UQSAX",               MakeMatcher<0>("xxxx01100110xxxxxxxx11110101xxxx", &Visitor::UQSAX) },
    { "UQSUB8",              MakeMatcher<0>("xxxx01100110xxxxxxxx11111111xxxx", &Visitor::UQSUB8) },
    { "UQSUB16",             MakeMatcher<0>("xxxx01100110xxxxxxxx11110111xxxx", &Visitor::UQSUB16) },

    // Parallel Add/Subtract (Halving) instructions
    { "SHADD8",              MakeMatcher<0>("xxxx01100011xxxxxxxx11111001xxxx", &Visitor::SHADD8) },
    { "SHADD16",             MakeMatcher<0>("xxxx01100011xxxxxxxx11110001xxxx", &Visitor::SHADD16) },
    { "SHASX",               MakeMatcher<0>("xxxx01100011xxxxxxxx11110011xxxx", &Visitor::SHASX) },
    { "SHSAX",               MakeMatcher<0>("xxxx01100011xxxxxxxx11110101xxxx", &Visitor::SHSAX) },
    { "SHSUB8",              MakeMatcher<0>("xxxx01100011xxxxxxxx11111111xxxx", &Visitor::SHSUB8) },
    { "SHSUB16",             MakeMatcher<0>("xxxx01100011xxxxxxxx11110111xxxx", &Visitor::SHSUB16) },
    { "UHADD8",              MakeMatcher<0>("xxxx01100111xxxxxxxx11111001xxxx", &Visitor::UHADD8) },
    { "UHADD16",             MakeMatcher<0>("xxxx01100111xxxxxxxx11110001xxxx", &Visitor::UHADD16) },
    { "UHASX",               MakeMatcher<0>("xxxx01100111xxxxxxxx11110011xxxx", &Visitor::UHASX) },
    { "UHSAX",               MakeMatcher<0>("xxxx01100111xxxxxxxx11110101xxxx", &Visitor::UHSAX) },
    { "UHSUB8",              MakeMatcher<0>("xxxx01100111xxxxxxxx11111111xxxx", &Visitor::UHSUB8) },
    { "UHSUB16",             MakeMatcher<0>("xxxx01100111xxxxxxxx11110111xxxx", &Visitor::UHSUB16) },

    // Saturated Add/Subtract instructions
    { "QADD",                MakeMatcher<0>("xxxx00010000xxxxxxxx00000101xxxx", &Visitor::QADD) },
    { "QSUB",                MakeMatcher<0>("xxxx00010010xxxxxxxx00000101xxxx", &Visitor::QSUB) },
    { "QDADD",               MakeMatcher<0>("xxxx00010100xxxxxxxx00000101xxxx", &Visitor::QDADD) },
    { "QDSUB",               MakeMatcher<0>("xxxx00010110xxxxxxxx00000101xxxx", &Visitor::QDSUB) },

    // Status Register Access instructions
    { "CPS",                 MakeMatcher<0>("xxxx00010000xxx00000000xxx0xxxxx", &Visitor::CPS) },
    { "ERET",                MakeMatcher<0>("xxxx0001011000000000000001101110", &Visitor::ERET) },
    { "SETEND",              MakeMatcher<0>("1111000100000001000000x000000000", &Visitor::SETEND) },
    { "MRS",                 MakeMatcher<0>("xxxx000100001111xxxx000000000000", &Visitor::MRS) },
    { "MRS (banked)",        MakeMatcher<0>("xxxx00010x00xxxxxxxx001x00000000", &Visitor::MRS) },
    { "MRS (system)",        MakeMatcher<0>("xxxx00010x001111xxxx000000000000", &Visitor::MRS) },
    { "MSR (imm)",           MakeMatcher<0>("xxxx00110010xx001111xxxxxxxxxxxx", &Visitor::MSR) },
    { "MSR (reg)",           MakeMatcher<0>("xxxx00010010xx00111100000000xxxx", &Visitor::MSR) },
    { "MSR (banked)",        MakeMatcher<0>("xxxx00010x10xxxx1111001x0000xxxx", &Visitor::MSR) },
    { "MSR (imm special)",   MakeMatcher<0>("xxxx00110x10xxxx1111xxxxxxxxxxxx", &Visitor::MSR) },
    { "MSR (reg special)",   MakeMatcher<0>("xxxx00010x10xxxx111100000000xxxx", &Visitor::MSR) },
    { "RFE",                 MakeMatcher<0>("xxxx0001101x0000xxxxxxxxx110xxxx", &Visitor::RFE) },
    { "SRS",                 MakeMatcher<0>("0000011xx0x00000000000000001xxxx", &Visitor::SRS) },
}};

const Instruction& DecodeArm(u32 i) {
    return *std::find_if(arm_instruction_table.cbegin(), arm_instruction_table.cend(), [i](const auto& instruction) {
        return instruction.Match(i);
    });
}

};
