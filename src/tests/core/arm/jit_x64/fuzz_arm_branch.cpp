// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <array>

#include <catch.hpp>

#include "common/common_types.h"

#include "tests/core/arm/jit_x64/rand_int.h"
#include "tests/core/arm/jit_x64/fuzz_arm_common.h"

TEST_CASE("Fuzz ARM branch instructions", "[JitX64]") {
    const std::array<std::pair<u32, u32>, 6> instructions = {{
        FromBitString32("1111101hvvvvvvvvvvvvvvvvvvvvvvvv"),
        FromBitString32("cccc000100101111111111110011mmmm"),
        FromBitString32("cccc1010vvvvvvvvvvvvvvvvvvvvvvvv"),
        FromBitString32("cccc1011vvvvvvvvvvvvvvvvvvvvvvvv"),
        FromBitString32("cccc000100101111111111110001mmmm"),
        FromBitString32("cccc000100101111111111110010mmmm"),
    }};

    auto instruction_select = [&]() -> u32 {
        size_t inst_index = RandInt<size_t>(0, instructions.size() - 1);

        u32 cond = RandInt<u32>(0, 0xE);
        u32 random = RandInt<u32>(0, 0xFFFFFF);
        u32 Rm = RandInt<u32>(0, 14);

        u32 assemble_randoms = (cond << 28) | (random << 4) | Rm;

        return instructions[inst_index].first | (assemble_randoms & (~instructions[inst_index].second));
    };

    SECTION("R15") {
        FuzzJit(1, 1, 10000, instruction_select);
    }
}