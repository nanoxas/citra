// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <array>

#include <catch.hpp>

#include "common/common_types.h"

#include "tests/core/arm/jit_x64/rand_int.h"
#include "tests/core/arm/jit_x64/fuzz_arm_common.h"
#include <common/assert.h>

TEST_CASE("Fuzz ARM data processing instructions", "[JitX64]") {
    const std::array<std::pair<u32, u32>, 16> imm_instructions = {{
        FromBitString32("cccc0010101Snnnnddddrrrrvvvvvvvv"),
        FromBitString32("cccc0010100Snnnnddddrrrrvvvvvvvv"),
        FromBitString32("cccc0010000Snnnnddddrrrrvvvvvvvv"),
        FromBitString32("cccc0011110Snnnnddddrrrrvvvvvvvv"),
        FromBitString32("cccc00110111nnnn0000rrrrvvvvvvvv"),
        FromBitString32("cccc00110101nnnn0000rrrrvvvvvvvv"),
        FromBitString32("cccc0010001Snnnnddddrrrrvvvvvvvv"),
        FromBitString32("cccc0011101S0000ddddrrrrvvvvvvvv"),
        FromBitString32("cccc0011111S0000ddddrrrrvvvvvvvv"),
        FromBitString32("cccc0011100Snnnnddddrrrrvvvvvvvv"),
        FromBitString32("cccc0010011Snnnnddddrrrrvvvvvvvv"),
        FromBitString32("cccc0010111Snnnnddddrrrrvvvvvvvv"),
        FromBitString32("cccc0010110Snnnnddddrrrrvvvvvvvv"),
        FromBitString32("cccc0010010Snnnnddddrrrrvvvvvvvv"),
        FromBitString32("cccc00110011nnnn0000rrrrvvvvvvvv"),
        FromBitString32("cccc00110001nnnn0000rrrrvvvvvvvv"),
    }};

    const std::array<std::pair<u32, u32>, 16> reg_instructions = {{
        FromBitString32("cccc0000101Snnnnddddvvvvvrr0mmmm"),
        FromBitString32("cccc0000100Snnnnddddvvvvvrr0mmmm"),
        FromBitString32("cccc0000000Snnnnddddvvvvvrr0mmmm"),
        FromBitString32("cccc0001110Snnnnddddvvvvvrr0mmmm"),
        FromBitString32("cccc00010111nnnn0000vvvvvrr0mmmm"),
        FromBitString32("cccc00010101nnnn0000vvvvvrr0mmmm"),
        FromBitString32("cccc0000001Snnnnddddvvvvvrr0mmmm"),
        FromBitString32("cccc0001101S0000ddddvvvvvrr0mmmm"),
        FromBitString32("cccc0001111S0000ddddvvvvvrr0mmmm"),
        FromBitString32("cccc0001100Snnnnddddvvvvvrr0mmmm"),
        FromBitString32("cccc0000011Snnnnddddvvvvvrr0mmmm"),
        FromBitString32("cccc0000111Snnnnddddvvvvvrr0mmmm"),
        FromBitString32("cccc0000110Snnnnddddvvvvvrr0mmmm"),
        FromBitString32("cccc0000010Snnnnddddvvvvvrr0mmmm"),
        FromBitString32("cccc00010011nnnn0000vvvvvrr0mmmm"),
        FromBitString32("cccc00010001nnnn0000vvvvvrr0mmmm"),
    }};

    const std::array<std::pair<u32, u32>, 16> rsr_instructions = {{
        FromBitString32("cccc0000101Snnnnddddssss0rr1mmmm"),
        FromBitString32("cccc0000100Snnnnddddssss0rr1mmmm"),
        FromBitString32("cccc0000000Snnnnddddssss0rr1mmmm"),
        FromBitString32("cccc0001110Snnnnddddssss0rr1mmmm"),
        FromBitString32("cccc00010111nnnn0000ssss0rr1mmmm"),
        FromBitString32("cccc00010101nnnn0000ssss0rr1mmmm"),
        FromBitString32("cccc0000001Snnnnddddssss0rr1mmmm"),
        FromBitString32("cccc0001101S0000ddddssss0rr1mmmm"),
        FromBitString32("cccc0001111S0000ddddssss0rr1mmmm"),
        FromBitString32("cccc0001100Snnnnddddssss0rr1mmmm"),
        FromBitString32("cccc0000011Snnnnddddssss0rr1mmmm"),
        FromBitString32("cccc0000111Snnnnddddssss0rr1mmmm"),
        FromBitString32("cccc0000110Snnnnddddssss0rr1mmmm"),
        FromBitString32("cccc0000010Snnnnddddssss0rr1mmmm"),
        FromBitString32("cccc00010011nnnn0000ssss0rr1mmmm"),
        FromBitString32("cccc00010001nnnn0000ssss0rr1mmmm"),
    }};

    auto instruction_select = [&](bool Rd_can_be_r15) -> auto {
        return [&, Rd_can_be_r15]() -> u32 {
            size_t instruction_set = RandInt<size_t>(0, 2);

            std::pair<u32, u32> instruction;

            u32 cond = 0xE;
            // Have a one-in-twenty-five chance of actually having a cond.
            if (RandInt(1, 25) == 1) {
                cond = RandInt<u32>(0x0, 0xD);
            }

            u32 S = RandInt<u32>(0, 1);

            switch (instruction_set) {
            case 0: {
                instruction = imm_instructions[RandInt<size_t>(0, imm_instructions.size() - 1)];
                u32 Rd = RandInt<u32>(0, Rd_can_be_r15 ? 15 : 14);
                if (Rd == 15) S = false;
                u32 Rn = RandInt<u32>(0, 15);
                u32 shifter_operand = RandInt<u32>(0, 0xFFF);
                u32 assemble_randoms = (shifter_operand << 0) | (Rd << 12) | (Rn << 16) | (S << 20) | (cond << 28);
                return instruction.first | (assemble_randoms & ~instruction.second);
            }
            case 1: {
                instruction = reg_instructions[RandInt<size_t>(0, reg_instructions.size() - 1)];
                u32 Rd = RandInt<u32>(0, Rd_can_be_r15 ? 15 : 14);
                if (Rd == 15) S = false;
                u32 Rn = RandInt<u32>(0, 15);
                u32 shifter_operand = RandInt<u32>(0, 0xFFF);
                u32 assemble_randoms = (shifter_operand << 0) | (Rd << 12) | (Rn << 16) | (S << 20) | (cond << 28);
                return instruction.first | (assemble_randoms & ~instruction.second);
            }
            case 2: {
                instruction = rsr_instructions[RandInt<size_t>(0, rsr_instructions.size() - 1)];
                u32 Rd = RandInt<u32>(0, 14); // Rd can never be 15.
                u32 Rn = RandInt<u32>(0, 14);
                u32 Rs = RandInt<u32>(0, 14);
                int rotate = RandInt<int>(0, 3);
                u32 Rm = RandInt<u32>(0, 14);
                u32 assemble_randoms = (Rm << 0) | (rotate << 5) | (Rs << 8) | (Rd << 12) | (Rn << 16) | (S << 20) | (cond << 28);
                return instruction.first | (assemble_randoms & ~instruction.second);
            }
            }

            UNREACHABLE();
        };
    };

    SECTION("short blocks") {
        FuzzJit(5, 6, 5000, instruction_select(/*Rd_can_be_r15=*/false));
    }

    SECTION("long blocks") {
        FuzzJit(1024, 1025, 200, instruction_select(/*Rd_can_be_r15=*/false));
    }

    SECTION("R15") {
        // Temporarily disabled as interpreter fails tests.
        //FuzzJit(1, 1, 10000, instruction_select(/*Rd_can_be_r15=*/true));
    }
}