// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <cstddef>
#include <cstring>

#include <boost/optional.hpp>

#include "common/assert.h"
#include "common/common_types.h"
#include "common/make_unique.h"

#include "core/arm/decoder/decoder.h"

namespace ArmDecoder {

namespace Impl {
    struct MatcherImpl : Matcher {
        MatcherImpl(u32 mask, u32 expect, std::function<void(Visitor* v, u32 instruction)> fn) : fn(fn) {
            bit_mask = mask;
            expected = expect;
        }
        std::function<void(Visitor* v, u32 instruction)> fn;
        virtual void visit(Visitor *v, u32 inst) override {
            fn(v, inst);
        }
    };
}

std::unique_ptr<Matcher> MakeMatcher(const char* str, std::function<void(Visitor* v, u32 instruction)> fn) {
    ASSERT(strlen(str) == 16);

    u32 mask = 0;
    u32 expect = 0;

    for (int i = 0; i < 16; i++) {
        mask <<= 1;
        expect <<= 1;

        switch (str[i]) {
        case '0':
            mask |= 1;
            expect |= 0;
            break;
        case '1':
            mask |= 1;
            expect |= 1;
            break;
        default:
            mask |= 0;
            expect |= 0;
            break;
        }
    }

    return Common::make_unique<Impl::MatcherImpl>(mask, expect, fn);
}

template<size_t a, size_t b, typename T>
static constexpr T bits(T s){
    return ((s << ((sizeof(s) * 8 - 1) - b)) >> (sizeof(s) * 8 - b + a - 1));
}

static const std::array<Instruction, 27> thumb_instruction_table = { {
    { "LSL/LSR/ASR",             MakeMatcher("000ooxxxxxxxxxxx", [](Visitor* v, u32 instruction) {
        u32 opcode = bits<11, 12>(instruction);
        u32 imm5 = bits<6, 10>(instruction);
        Register Rm = bits<3, 5>(instruction);
        Register Rd = bits<0, 2>(instruction);
        switch (opcode) {
        case 0: // LSL <Rd>, <Rm>, #<imm5>
            v->MOV_reg(0xE, /*S=*/true, Rd, imm5, 0b00, Rm);
            break;
        case 1: // LSR <Rd>, <Rm>, #<imm5>
            v->MOV_reg(0xE, /*S=*/true, Rd, imm5, 0b01, Rm);
            break;
        case 2: // ASR <Rd>, <Rm>, #<imm5>
            v->MOV_reg(0xE, /*S=*/true, Rd, imm5, 0b10, Rm);
            break;
        default:
            UNREACHABLE();
        }
    })},
    { "ADD/SUB_reg",              MakeMatcher("000110oxxxxxxxxx", [](Visitor* v, u32 instruction) {
        u32 opcode = bits<9, 9>(instruction);
        Register Rm = bits<6, 8>(instruction);
        Register Rn = bits<3, 5>(instruction);
        Register Rd = bits<0, 2>(instruction);
        switch (opcode) {
        case 0: // ADD <Rd>, <Rn>, <Rm>
            v->ADD_reg(0xE, /*S=*/true, Rn, Rd, 0, 0, Rm);
            break;
        case 1: // SUB <Rd>, <Rn>, <Rm>
            v->SUB_reg(0xE, /*S=*/true, Rn, Rd, 0, 0, Rm);
            break;
        default:
            UNREACHABLE();
        }
    })},
    { "ADD/SUB_imm",             MakeMatcher("000111oxxxxxxxxx", [](Visitor* v, u32 instruction) {
        u32 opcode = bits<9, 9>(instruction);
        u32 imm3 = bits<6, 8>(instruction);
        Register Rn = bits<3, 5>(instruction);
        Register Rd = bits<0, 2>(instruction);
        switch (opcode) {
        case 0: // ADD <Rd>, <Rn>, #<imm3>
            v->ADD_imm(0xE, /*S=*/true, Rn, Rd, 0, imm3);
            break;
        case 1: // SUB <Rd>, <Rn>, #<imm3>
            v->SUB_imm(0xE, /*S=*/true, Rn, Rd, 0, imm3);
            break;
        default:
            UNREACHABLE();
        }
    })},
    { "add/sub/cmp/mov_imm",     MakeMatcher("001ooxxxxxxxxxxx", [](Visitor* v, u32 instruction) {
        u32 opcode = bits<11, 12>(instruction);
        Register Rd = bits<8, 10>(instruction);
        u32 imm8 = bits<0, 7>(instruction);
        switch (opcode) {
        case 0: // MOV Rd, #imm8
            v->MOV_imm(0xE, /*S=*/true, Rd, 0, imm8);
            break;
        case 1: // CMP Rn, #imm8
            v->CMP_imm(0xE, Rd, 0, imm8);
            break;
        case 2: // ADD Rd, #imm8
            v->ADD_imm(0xE, /*S=*/true, Rd, Rd, 0, imm8);
            break;
        case 3: // SUB Rd, #imm8
            v->SUB_imm(0xE, /*S=*/true, Rd, Rd, 0, imm8);
            break;
        default:
            UNREACHABLE();
        }
    })},
    { "data processing reg",     MakeMatcher("010000ooooxxxxxx", [](Visitor* v, u32 instruction) {
        u32 opcode = bits<6, 9>(instruction);
        Register Rm_Rs = bits<3, 5>(instruction);
        Register Rd_Rn = bits<0, 2>(instruction);
        switch (opcode) {
        case 0: // AND Rd, Rm
            v->AND_reg(0xE, /*S=*/true, Rd_Rn, Rd_Rn, 0, 0, Rm_Rs);
            break;
        case 1: // EOR Rd, Rm
            v->EOR_reg(0xE, /*S=*/true, Rd_Rn, Rd_Rn, 0, 0, Rm_Rs);
            break;
        case 2: // LSL Rd, Rs
            v->MOV_rsr(0xE, /*S=*/true, Rd_Rn, Rm_Rs, 0b0001, Rd_Rn);
            break;
        case 3: // LSR Rd, Rs
            v->MOV_rsr(0xE, /*S=*/true, Rd_Rn, Rm_Rs, 0b0011, Rd_Rn);
            break;
        case 4: // ASR Rd, Rs
            v->MOV_rsr(0xE, /*S=*/true, Rd_Rn, Rm_Rs, 0b0101, Rd_Rn);
            break;
        case 5: // ADC Rd, Rm
            v->ADC_reg(0xE, /*S=*/true, Rd_Rn, Rd_Rn, 0, 0, Rm_Rs);
            break;
        case 6: // SBC Rd, Rm
            v->SBC_reg(0xE, /*S=*/true, Rd_Rn, Rd_Rn, 0, 0, Rm_Rs);
            break;
        case 7: // ROR Rd, Rs
            v->MOV_rsr(0xE, /*S=*/true, Rd_Rn, Rm_Rs, 0b0111, Rd_Rn);
            break;
        case 8: // TST Rm, Rn
            v->TST_reg(0xE, Rd_Rn, 0, 0, Rm_Rs);
            break;
        case 9: // NEG Rd, Rm
            v->RSB_imm(0xE, /*S=*/true, Rm_Rs, Rd_Rn, 0, 0);
            break;
        case 10: // CMP Rm, Rn
            v->CMP_reg(0xE, Rd_Rn, 0, 0, Rm_Rs);
            break;
        case 11: // CMN Rm, Rn
            v->CMN_reg(0xE, Rd_Rn, 0, 0, Rm_Rs);
            break;
        case 12: // ORR Rd, Rm
            v->ORR_reg(0xE, /*S=*/true, Rd_Rn, Rd_Rn, 0, 0, Rm_Rs);
            break;
        case 13: // MUL Rd, Rm
            v->MUL();
            break;
        case 14: // BIC Rm, Rd
            v->BIC_reg(0xE, /*S=*/true, Rd_Rn, Rd_Rn, 0, 0, Rm_Rs);
            break;
        case 15: // MVN Rd, Rm
            v->MVN_reg(0xE, /*S=*/true, Rd_Rn, 0, 0, Rm_Rs);
            break;
        default:
            UNREACHABLE();
        }
    })},
    { "special data processing", MakeMatcher("010001ooxxxxxxxx", [](Visitor* v, u32 instruction) {
        u32 opcode = bits<8, 9>(instruction);
        Register Rm = bits<3, 6>(instruction);
        Register Rd = bits<0, 2>(instruction) | (bits<7, 7>(instruction) << 3);
        switch (opcode) {
        case 0: // ADD Rd, Rm
            v->ADD_reg(0xE, /*S=*/false, Rd, Rd, 0, 0, Rm);
            break;
        case 1: // CMP Rm, Rn
            v->CMP_reg(0xE, Rd, 0, 0, Rm);
            break;
        case 2: // MOV Rd, Rm
            v->MOV_reg(0xE, /*S=*/false, Rd, 0, 0, Rm);
            break;
        default:
            UNREACHABLE();
        }
    })},
    { "BLX/BX",                  MakeMatcher("01000111xxxxx000", [](Visitor* v, u32 instruction) {
        bool L = bits<7, 7>(instruction);
        Register Rm = bits<3, 6>(instruction);
        if (!L) { // BX Rm
            v->BX(0xE, Rm);
        } else { // BLX Rm
            v->BLX_reg(0xE, Rm);
        }
    })},
    { "load from literal pool",  MakeMatcher("01001xxxxxxxxxxx", [](Visitor* v, u32 instruction) {
        // LDR Rd, [PC, #]
        Register Rd = bits<8, 10>(instruction);
        u32 imm8 = bits<0, 7>(instruction);
        v->LDR_imm();
    })},
    { "load/store reg offset",   MakeMatcher("0101oooxxxxxxxxx", [](Visitor* v, u32 instruction) {
        u32 opcode = bits<9, 11>(instruction);
        Register Rm = bits<6, 8>(instruction);
        Register Rn = bits<3, 5>(instruction);
        Register Rd = bits<0, 2>(instruction);
        switch (opcode) {
        case 0: // STR Rd, [Rn, Rm]
            v->STR_reg();
            break;
        case 1: // STRH Rd, [Rn, Rm]
            v->STRH_reg();
            break;
        case 2: // STRB Rd, [Rn, Rm]
            v->STRB_reg();
            break;
        case 3: // LDRSB Rd, [Rn, Rm]
            v->LDRSB_reg();
            break;
        case 4: // LDR Rd, [Rn, Rm]
            v->LDR_reg();
            break;
        case 5: // LDRH Rd, [Rn, Rm]
            v->LDRH_reg();
            break;
        case 6: // LDRB Rd, [Rn, Rm]
            v->LDRB_reg();
            break;
        case 7: // LDRSH Rd, [Rn, Rm]
            v->LDRSH_reg();
            break;
        default:
            UNREACHABLE();
        }
    })},
    { "STR(B)/LDR(B)_imm",       MakeMatcher("011xxxxxxxxxxxxx", [](Visitor* v, u32 instruction) {
        u32 opc = bits<11, 12>(instruction);
        Register offset = bits<6, 10>(instruction);
        Register Rn = bits<3, 5>(instruction);
        Register Rd = bits<0, 2>(instruction);
        switch (opc) {
        case 0: // STR Rd, [Rn, #offset]
            v->STR_imm();
            break;
        case 1: // LDR Rd, [Rn, #offset]
            v->LDR_imm();
            break;
        case 2: // STRB Rd, [Rn, #offset]
            v->STRB_imm();
            break;
        case 3: // LDRB Rd, [Rn, #offset]
            v->LDRB_imm();
            break;
        default:
            UNREACHABLE();
        }
    })},
    { "STRH/LDRH_imm",           MakeMatcher("1000xxxxxxxxxxxx", [](Visitor* v, u32 instruction) {
        bool L = bits<11, 11>(instruction);
        Register offset = bits<6, 10>(instruction);
        Register Rn = bits<3, 5>(instruction);
        Register Rd = bits<0, 2>(instruction);
        if (!L) { // STRH Rd, [Rn, #offset]
            v->STRH_imm();
        } else { // LDRH Rd, [Rn, #offset]
            v->LDRH_imm();
        }
    })},
    { "load/store stack",        MakeMatcher("1001xxxxxxxxxxxx", [](Visitor* v, u32 instruction) {
        bool L = bits<11, 11>(instruction);
        Register Rd = bits<8, 10>(instruction);
        u32 offset = bits<0, 7>(instruction);
        if (!L) { // STR Rd, [SP, #offset]
            v->STR_imm();
        } else { // LDR Rd, [SP, #offset]
            v->LDR_imm();
        }
    })},
    { "add to sp/pc",            MakeMatcher("1010oxxxxxxxxxxx", [](Visitor* v, u32 instruction) {
        // ADD Rd, PC/SP, #imm8
        Register Rn = bits<11, 11>(instruction) ? 13 : 15;
        Register Rd = bits<8, 10>(instruction);
        u32 imm8 = bits<0, 7>(instruction);
        v->ADD_imm(0xE, /*S=*/false, Rn, Rd, 0xF, imm8);
    })},
    { "adjust stack ptr",        MakeMatcher("10110000oxxxxxxx", [](Visitor* v, u32 instruction) {
        // SUB SP, SP, #<imm7*4>
        u32 opc = bits<7, 7>(instruction);
        u32 imm7 = bits<0, 6>(instruction);
        switch (opc) {
        case 0:
            v->ADD_imm(0xE, /*S=*/false, 13, 13, 0xF, imm7);
            break;
        case 1:
            v->SUB_imm(0xE, /*S=*/false, 13, 13, 0xF, imm7);
            break;
        default:
            UNREACHABLE();
        }
    })},
    { "sign/zero extend",        MakeMatcher("10110010ooxxxxxx", [](Visitor* v, u32 instruction) {
        u32 opc = bits<6, 7>(instruction);
        Register Rm = bits<3, 5>(instruction);
        Register Rd = bits<0, 2>(instruction);
        switch (opc) {
        case 0: // SXTH Rd, Rm
            v->SXTH();
            break;
        case 1: // SXTB Rd, Rm
            v->SXTB();
            break;
        case 2: // UXTH Rd, Rm
            v->UXTH();
            break;
        case 3: // UXTB Rd, Rm
            v->UXTB();
            break;
        default:
            UNREACHABLE();
        }
    })},
    { "PUSH/POP_reglist",        MakeMatcher("1011x10xxxxxxxxx", [](Visitor* v, u32 instruction) {
        bool L = bits<11, 11>(instruction);
        bool R = bits<8, 8>(instruction);
        u32 reglist = bits<0, 7>(instruction);
        if (!L) { // PUSH {reglist, <R>=LR}
            reglist |= R << 14;
            v->STM();
        } else { // POP {reglist, <R>=PC}
            reglist |= R << 15;
            v->LDM();
        }
    })},
    { "SETEND",                  MakeMatcher("101101100101x000", [](Visitor* v, u32 instruction) {
        bool E = bits<3, 3>(instruction);
        v->SETEND(E);
    })},
    { "change processor state",  MakeMatcher("10110110011x0xxx", [](Visitor* v, u32 instruction) {
        bool imod = bits<4, 4>(instruction);
        bool A = bits<2, 2>(instruction);
        bool I = bits<1, 1>(instruction);
        bool F = bits<0, 0>(instruction);
        v->CPS();
    })},
    { "reverse bytes",           MakeMatcher("10111010ooxxxxxx", [](Visitor* v, u32 instruction) {
        u32 opc = bits<6, 7>(instruction);
        Register Rn = bits<3, 5>(instruction);
        Register Rd = bits<0, 2>(instruction);
        switch (opc) {
        case 0: // REV Rd, Rn
            v->REV();
            break;
        case 1: // REV16 Rd, Rn
            v->REV16();
            break;
        case 2: // undefined
            v->UDF();
            break;
        case 3: // REVSH Rd, Rn
            v->REVSH();
            break;
        default:
            UNREACHABLE();
        }
    })},
    { "BKPT",                    MakeMatcher("10111110xxxxxxxx", [](Visitor* v, u32 instruction) {
        // BKPT #imm8
        Imm8 imm8 = bits<0, 7>(instruction);
        v->BKPT();
    })},
    { "STMIA/LDMIA",             MakeMatcher("1100xxxxxxxxxxxx", [](Visitor* v, u32 instruction) {
        bool L = bits<11, 11>(instruction);
        Register Rn = bits<8, 10>(instruction);
        u32 reglist = bits<0, 7>(instruction);
        if (!L) { // STMIA Rn!, { reglist }
            v->STM();
        } else { // LDMIA Rn!, { reglist }
            v->LDM();
        }
    })},
    { "B<cond>",                 MakeMatcher("1101xxxxxxxxxxxx", [](Visitor* v, u32 instruction) {
        // B<cond> <PC + #offset*2>
        Cond cond = bits<8, 11>(instruction);
        s32 offset = bits<0, 7>(instruction);
        v->thumb_B(cond, offset);
    })},
    { "SWI",                     MakeMatcher("11011111xxxxxxxx", [](Visitor* v, u32 instruction) {
        // SWI #imm8
        Imm8 imm8 = bits<0, 7>(instruction);
        v->SVC(/*imm8*/);
    })},
    { "B",                       MakeMatcher("11100xxxxxxxxxxx", [](Visitor* v, u32 instruction) {
        // B <PC + #offset*2>
        Imm11 imm11 = bits<0, 10>(instruction);
        v->thumb_B(imm11);
    })},
    { "BLX (suffix)",            MakeMatcher("11101xxxxxxxxxx0", [](Visitor* v, u32 instruction) {
        Imm11 imm11 = bits<0, 10>(instruction);
        v->thumb_BLX_suffix(/*X=*/true, imm11);
    })},
    { "BL/BLX (prefix)",         MakeMatcher("11110xxxxxxxxxxx", [](Visitor* v, u32 instruction) {
        Imm11 imm11 = bits<0, 10>(instruction);
        v->thumb_BLX_prefix(imm11);
    })},
    { "BL (suffix)",             MakeMatcher("11111xxxxxxxxxxx", [](Visitor* v, u32 instruction) {
        Imm11 imm11 = bits<0, 10>(instruction);
        v->thumb_BLX_suffix(/*X=*/false, imm11);
    })}
}};

boost::optional<const Instruction&> DecodeThumb(u16 i) {
    // NOTE: The reverse search direction is important. Searching forwards would result in incorrect behavior.
    //       This is because the entries in thumb_instruction_table have more specific matches coming after less specific ones.
    //       Example:
    //           000ooxxxxxxxxxxx comes before 000110oxxxxxxxxx
    //       with a forward search direction notice how the first one will always be matched and the latter never will be.
    auto iterator = std::find_if(thumb_instruction_table.crbegin(), thumb_instruction_table.crend(), [i](const Instruction& instruction) {
        return instruction.Match(i);
    });

    return (iterator != thumb_instruction_table.crend()) ? boost::make_optional<const Instruction&>(*iterator) : boost::none;
}

};
