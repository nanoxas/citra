// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <array>

#include <catch.hpp>

#include "common/common_types.h"

#include "tests/core/arm/jit_x64/rand_int.h"
#include "tests/core/arm/jit_x64/fuzz_arm_common.h"

TEST_CASE("Fuzz ARM load/store instructions (byte, half-word, word)", "[JitX64]") {
    const std::array<std::pair<u32, u32>, 17> instructions = {{
        FromBitString32("cccc010pu0w1nnnnddddvvvvvvvvvvvv"), // LDR_imm
        FromBitString32("cccc011pu0w1nnnnddddvvvvvrr0mmmm"), // LDR_reg
        FromBitString32("cccc010pu1w1nnnnddddvvvvvvvvvvvv"), // LDRB_imm
        FromBitString32("cccc011pu1w1nnnnddddvvvvvrr0mmmm"), // LDRB_reg
        FromBitString32("cccc010pu0w0nnnnddddvvvvvvvvvvvv"), // STR_imm
        FromBitString32("cccc011pu0w0nnnnddddvvvvvrr0mmmm"), // STR_reg
        FromBitString32("cccc010pu1w0nnnnddddvvvvvvvvvvvv"), // STRB_imm
        FromBitString32("cccc011pu1w0nnnnddddvvvvvrr0mmmm"), // STRB_reg
        FromBitString32("cccc000pu1w1nnnnddddvvvv1011vvvv"), // LDRH_imm
        FromBitString32("cccc000pu0w1nnnndddd00001011mmmm"), // LDRH_reg
        FromBitString32("cccc000pu1w1nnnnddddvvvv1101vvvv"), // LDRSB_imm
        FromBitString32("cccc000pu0w1nnnndddd00001101mmmm"), // LDRSB_reg
        FromBitString32("cccc000pu1w1nnnnddddvvvv1111vvvv"), // LDRSH_imm
        FromBitString32("cccc000pu0w1nnnndddd00001111mmmm"), // LDRSH_reg
        FromBitString32("cccc000pu1w0nnnnddddvvvv1011vvvv"), // STRH_imm
        FromBitString32("cccc000pu0w0nnnndddd00001011mmmm"), // STRH_reg
        FromBitString32("1111000100000001000000e000000000"), // SETEND
    }};

    auto instruction_select = [&]() -> u32 {
        size_t inst_index = RandInt<size_t>(0, instructions.size() - 1);

        u32 cond = 0xE;
        // Have a one-in-twenty-five chance of actually having a cond.
        if (RandInt(1, 25) == 1) {
            cond = RandInt<u32>(0x0, 0xD);
        }

        u32 Rn = RandInt<u32>(0, 14);
        u32 Rd = RandInt<u32>(0, 14);
        u32 W = 0;
        u32 P = RandInt<u32>(0, 1);
        if (P) W = RandInt<u32>(0, 1);
        u32 U = RandInt<u32>(0, 1);
        u32 rand = RandInt<u32>(0, 0xFF);
        u32 Rm = RandInt<u32>(0, 14);

        u32 assemble_randoms = (Rm << 0) | (rand << 4) | (Rd << 12) | (Rn << 16) | (W << 21) | (U << 23) | (P << 24) | (cond << 28);

        return instructions[inst_index].first | (assemble_randoms & (~instructions[inst_index].second));
    };

    SECTION("short blocks") {
        FuzzJit(5, 6, 5000, instruction_select);
    }
}

TEST_CASE("Fuzz ARM load/store instructions (double-word)", "[JitX64]") {
    const std::array<std::pair<u32, u32>, 4> instructions = {{
        FromBitString32("cccc000pu1w0nnnnddddvvvv1101vvvv"), // LDRD_imm
        FromBitString32("cccc000pu0w0nnnndddd00001101mmmm"), // LDRD_reg
        FromBitString32("cccc000pu1w0nnnnddddvvvv1111vvvv"), // STRD_imm
        FromBitString32("cccc000pu0w0nnnndddd00001111mmmm"), // STRD_reg
    }};

    auto instruction_select = [&]() -> u32 {
        size_t inst_index = RandInt<size_t>(0, instructions.size() - 1);

        u32 cond = 0xE;
        // Have a one-in-twenty-five chance of actually having a cond.
        if (RandInt(1, 25) == 1) {
            cond = RandInt<u32>(0x0, 0xD);
        }

        u32 Rn = RandInt<u32>(0, 6) * 2;
        u32 Rd = RandInt<u32>(0, 6) * 2;
        u32 W = 0;
        u32 P = RandInt<u32>(0, 1);
        if (P) W = RandInt<u32>(0, 1);
        u32 U = RandInt<u32>(0, 1);
        u32 rand = RandInt<u32>(0, 0xF);
        u32 Rm = RandInt<u32>(0, 14);

        u32 assemble_randoms = (Rm << 0) | (rand << 4) | (Rd << 12) | (Rn << 16) | (W << 21) | (U << 23) | (P << 24) | (cond << 28);

        return instructions[inst_index].first | (assemble_randoms & (~instructions[inst_index].second));
    };

    SECTION("short blocks") {
        FuzzJit(1, 2, 5000, instruction_select);
    }
}

TEST_CASE("Fuzz ARM load/store multiple instructions", "[JitX64]") {
    const std::array<std::pair<u32, u32>, 2> instructions = {{
        FromBitString32("cccc100pu0w1nnnnxxxxxxxxxxxxxxxx"), // LDM
        FromBitString32("cccc100pu0w0nnnnxxxxxxxxxxxxxxxx"), // STM
    }};

    auto instruction_select = [&]() -> u32 {
        size_t inst_index = RandInt<size_t>(0, instructions.size() - 1);

        u32 cond = 0xE;
        // Have a one-in-twenty-five chance of actually having a cond.
        if (RandInt(1, 25) == 1) {
            cond = RandInt<u32>(0x0, 0xD);
        }

        u32 reg_list = RandInt<u32>(1, 0xFFFF);
        u32 Rn = RandInt<u32>(0, 14);
        u32 flags = RandInt<u32>(0, 0xF);

        if (inst_index == 1 && (flags & 2)) {
            if (reg_list & (1 << Rn))
                reg_list &= ~((1 << Rn) - 1);
        } else if (inst_index == 1 && (flags & 2)) {
            reg_list &= ~(1 << Rn);
        }

        u32 assemble_randoms = (reg_list << 0) | (Rn << 16) | (flags << 24) | (cond << 28);

        return instructions[inst_index].first | (assemble_randoms & (~instructions[inst_index].second));
    };

    SECTION("short blocks") {
        FuzzJit(1, 1, 5000, instruction_select);
    }
}