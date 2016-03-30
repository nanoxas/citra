// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <array>

#include <catch.hpp>

#include "common/common_types.h"

#include "tests/core/arm/jit_x64/rand_int.h"
#include "tests/core/arm/jit_x64/fuzz_arm_common.h"

TEST_CASE("Fuzz ARM data processing instructions", "[JitX64]") {
    const std::array<std::pair<u32, u32>, 48> instructions = {{
        FromBitString32("cccc0010101Snnnnddddrrrrvvvvvvvv"),
        FromBitString32("cccc0000101Snnnnddddvvvvvrr0mmmm"),
        FromBitString32("cccc0000101Snnnnddddssss0rr1mmmm"),
        FromBitString32("cccc0010100Snnnnddddrrrrvvvvvvvv"),
        FromBitString32("cccc0000100Snnnnddddvvvvvrr0mmmm"),
        FromBitString32("cccc0000100Snnnnddddssss0rr1mmmm"),
        FromBitString32("cccc0010000Snnnnddddrrrrvvvvvvvv"),
        FromBitString32("cccc0000000Snnnnddddvvvvvrr0mmmm"),
        FromBitString32("cccc0000000Snnnnddddssss0rr1mmmm"),
        FromBitString32("cccc0011110Snnnnddddrrrrvvvvvvvv"),
        FromBitString32("cccc0001110Snnnnddddvvvvvrr0mmmm"),
        FromBitString32("cccc0001110Snnnnddddssss0rr1mmmm"),
        FromBitString32("cccc00110111nnnn0000rrrrvvvvvvvv"),
        FromBitString32("cccc00010111nnnn0000vvvvvrr0mmmm"),
        FromBitString32("cccc00010111nnnn0000ssss0rr1mmmm"),
        FromBitString32("cccc00110101nnnn0000rrrrvvvvvvvv"),
        FromBitString32("cccc00010101nnnn0000vvvvvrr0mmmm"),
        FromBitString32("cccc00010101nnnn0000ssss0rr1mmmm"),
        FromBitString32("cccc0010001Snnnnddddrrrrvvvvvvvv"),
        FromBitString32("cccc0000001Snnnnddddvvvvvrr0mmmm"),
        FromBitString32("cccc0000001Snnnnddddssss0rr1mmmm"),
        FromBitString32("cccc0011101S0000ddddrrrrvvvvvvvv"),
        FromBitString32("cccc0001101S0000ddddvvvvvrr0mmmm"),
        FromBitString32("cccc0001101S0000ddddssss0rr1mmmm"),
        FromBitString32("cccc0011111S0000ddddrrrrvvvvvvvv"),
        FromBitString32("cccc0001111S0000ddddvvvvvrr0mmmm"),
        FromBitString32("cccc0001111S0000ddddssss0rr1mmmm"),
        FromBitString32("cccc0011100Snnnnddddrrrrvvvvvvvv"),
        FromBitString32("cccc0001100Snnnnddddvvvvvrr0mmmm"),
        FromBitString32("cccc0001100Snnnnddddssss0rr1mmmm"),
        FromBitString32("cccc0010011Snnnnddddrrrrvvvvvvvv"),
        FromBitString32("cccc0000011Snnnnddddvvvvvrr0mmmm"),
        FromBitString32("cccc0000011Snnnnddddssss0rr1mmmm"),
        FromBitString32("cccc0010111Snnnnddddrrrrvvvvvvvv"),
        FromBitString32("cccc0000111Snnnnddddvvvvvrr0mmmm"),
        FromBitString32("cccc0000111Snnnnddddssss0rr1mmmm"),
        FromBitString32("cccc0010110Snnnnddddrrrrvvvvvvvv"),
        FromBitString32("cccc0000110Snnnnddddvvvvvrr0mmmm"),
        FromBitString32("cccc0000110Snnnnddddssss0rr1mmmm"),
        FromBitString32("cccc0010010Snnnnddddrrrrvvvvvvvv"),
        FromBitString32("cccc0000010Snnnnddddvvvvvrr0mmmm"),
        FromBitString32("cccc0000010Snnnnddddssss0rr1mmmm"),
        FromBitString32("cccc00110011nnnn0000rrrrvvvvvvvv"),
        FromBitString32("cccc00010011nnnn0000vvvvvrr0mmmm"),
        FromBitString32("cccc00010011nnnn0000ssss0rr1mmmm"),
        FromBitString32("cccc00110001nnnn0000rrrrvvvvvvvv"),
        FromBitString32("cccc00010001nnnn0000vvvvvrr0mmmm"),
        FromBitString32("cccc00010001nnnn0000ssss0rr1mmmm"),
    }};

    auto instruction_select_without_R15 = [&]() -> u32 {
        size_t inst_index = RandInt<size_t>(0, instructions.size() - 1);

        u32 cond = 0xE;
        // Have a one-in-twenty-five chance of actually having a cond.
        if (RandInt(1, 25) == 1) {
            cond = RandInt<u32>(0x0, 0xD);
        }

        u32 Rn = RandInt<u32>(0, 15);
        u32 Rd = RandInt<u32>(0, 14);
        u32 S = RandInt<u32>(0, 1);
        u32 shifter_operand = RandInt<u32>(0, 0xFFF);

        u32 assemble_randoms = (shifter_operand << 0) | (Rd << 12) | (Rn << 16) | (S << 20) | (cond << 28);

        return instructions[inst_index].first | (assemble_randoms & (~instructions[inst_index].second));
    };

    SECTION("short blocks") {
        FuzzJit(5, 6, 5000, instruction_select_without_R15);
    }

    SECTION("long blocks") {
        FuzzJit(1024, 1025, 50, instruction_select_without_R15);
    }

    auto instruction_select_only_R15 = [&]() -> u32 {
        size_t inst_index = RandInt<size_t>(0, instructions.size() - 1);

        u32 cond = 0xE;
        // Have a one-in-twenty-five chance of actually having a cond.
        if (RandInt(1, 25) == 1) {
            cond = RandInt<u32>(0x0, 0xD);
        }

        u32 Rn = RandInt<u32>(0, 15);
        u32 Rd = 15;
        u32 S = 0;
        u32 shifter_operand = RandInt<u32>(0, 0xFFF);

        u32 assemble_randoms = (shifter_operand << 0) | (Rd << 12) | (Rn << 16) | (S << 20) | (cond << 28);

        return instructions[inst_index].first | (assemble_randoms & (~instructions[inst_index].second));
    };

    SECTION("R15") {
        FuzzJit(1, 1, 10000, instruction_select_only_R15);
    }
}