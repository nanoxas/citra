// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <cstdio>
#include <cstring>

#include <catch.hpp>

#include "common/common_types.h"
#include "common/scope_exit.h"

#include "core/arm/dyncom/arm_dyncom.h"
#include "core/arm/jit_x64/interface.h"
#include "core/core.h"
#include "core/memory_setup.h"

#include "tests/core/arm/jit_x64/rand_int.h"

std::pair<u16, u16> FromBitString16(const char* str) {
    REQUIRE(strlen(str) == 16);

    u16 bits = 0;
    u16 mask = 0;
    for (int i = 0; i < 16; i++) {
        const u16 bit = 1 << (15 - i);
        switch (str[i]) {
        case '0':
            mask |= bit;
            break;
        case '1':
            bits |= bit;
            mask |= bit;
            break;
        default:
            // Do nothing
            break;
        }
    }
    return{ bits, mask };
}

void FuzzJitThumb(const int instruction_count, const int instructions_to_execute_count, const int run_count, const std::function<u16(int)> instruction_generator) {
    // Init core
    Core::Init();
    SCOPE_EXIT({ Core::Shutdown(); });

    // Prepare memory
    constexpr size_t MEMORY_SIZE = 4096 * 2;
    std::array<u8, MEMORY_SIZE> test_mem{};
    Memory::MapMemoryRegion(0, MEMORY_SIZE, test_mem.data());
    SCOPE_EXIT({ Memory::UnmapRegion(0, MEMORY_SIZE); });

    // Prepare test subjects
    JitX64::ARM_Jit jit(PrivilegeMode::USER32MODE);
    ARM_DynCom interp(PrivilegeMode::USER32MODE);
    SCOPE_EXIT({
        jit.FastClearCache();
        interp.ClearCache();
    });

    for (int run_number = 0; run_number < run_count; run_number++) {
        jit.FastClearCache();
        interp.ClearCache();

        u32 initial_regs[15];
        for (int i = 0; i < 15; i++) {
            u32 val = RandInt<u32>(0, 0xFFFFFFFF);
            interp.SetReg(i, val);
            jit.SetReg(i, val);
            initial_regs[i] = val;
        }

        interp.SetCPSR(0x000001d0);
        jit.SetCPSR(0x000001d0);

        interp.SetPC(0);
        jit.SetPC(0);

        Memory::Write32(0, 0xFAFFFFFF); // blx +#4 // Jump to the following code (switch to thumb)

        for (int i = 0; i < instruction_count; i++) {
            u16 inst = instruction_generator(i);

            Memory::Write16(4 + i * 2, inst);
        }

        Memory::Write16(4 + instruction_count * 2, 0xE7FE); // b +#0 // busy wait loop

        interp.ExecuteInstructions(instructions_to_execute_count);
        jit.ExecuteInstructions(instructions_to_execute_count);

        bool pass = true;

        if (interp.GetCPSR() != jit.GetCPSR()) pass = false;
        for (int i = 0; i <= 15; i++) {
            if (interp.GetReg(i) != jit.GetReg(i)) pass = false;
        }

        if (!pass) {
            printf("Failed at execution number %i\n", run_number);

            printf("\nInstruction Listing: \n");
            for (int i = 0; i < instruction_count; i++) {
                printf("%04x\n", Memory::Read16(4 + i * 2));
            }

            printf("\nFinal Register Listing: \n");
            for (int i = 0; i <= 15; i++) {
                printf("%4i: %08x %08x %s\n", i, interp.GetReg(i), jit.GetReg(i), interp.GetReg(i) != jit.GetReg(i) ? "*" : "");
            }
            printf("CPSR: %08x %08x %s\n", interp.GetCPSR(), jit.GetCPSR(), interp.GetCPSR() != jit.GetCPSR() ? "*" : "");

            printf("\nInterpreter walkthrough:\n");
            interp.ClearCache();
            interp.SetPC(0);
            interp.SetCPSR(0x000001d0);
            for (int i = 0; i < 15; i++) {
                interp.SetReg(i, initial_regs[i]);
                printf("%4i: %08x\n", i, interp.GetReg(i));
            }
            for (int inst = 0; inst < instruction_count; inst++) {
                interp.Step();
                for (int i = 0; i <= 15; i++) {
                    printf("%4i: %08x\n", i, interp.GetReg(i));
                }
                printf("CPSR: %08x\n", interp.GetCPSR());
            }

#ifdef _MSC_VER
            DebugBreak();
#endif
            FAIL();
        }

        printf("%i\r", run_number);
        if (run_number % 50 == 0) {
            fflush(stdout);
        }
    }
}

// Things not yet tested:
// FromBitString16("01001xxxxxxxxxxx"), // LDR Rd, [PC, #]
// FromBitString16("101101100101x000"), // SETEND
// FromBitString16("10111110xxxxxxxx"), // BKPT
// FromBitString16("0101oooxxxxxxxxx"), // LDR/STR
// FromBitString16("011xxxxxxxxxxxxx"), // loads/stores
// FromBitString16("1000xxxxxxxxxxxx"), // loads/stores
// FromBitString16("1001xxxxxxxxxxxx"), // loads/stores
// FromBitString16("1011x10xxxxxxxxx"), // push/pop
// FromBitString16("10110110011x0xxx"), // CPS
// FromBitString16("1100xxxxxxxxxxxx"), // STMIA/LDMIA
// FromBitString16("11011111xxxxxxxx"), // SWI
// FromBitString16("1101xxxxxxxxxxxx"), // B<cond>

TEST_CASE("Fuzz Thumb instructions set 1 (pure computation)", "[JitX64][Thumb]") {
    const std::array<std::pair<u16, u16>, 16> instructions = {{
        FromBitString16("00000xxxxxxxxxxx"), // LSL <Rd>, <Rm>, #<imm5>
        FromBitString16("00001xxxxxxxxxxx"), // LSR <Rd>, <Rm>, #<imm5>
        FromBitString16("00010xxxxxxxxxxx"), // ASR <Rd>, <Rm>, #<imm5>
        FromBitString16("000110oxxxxxxxxx"), // ADD/SUB_reg
        FromBitString16("000111oxxxxxxxxx"), // ADD/SUB_imm
        FromBitString16("001ooxxxxxxxxxxx"), // ADD/SUB/CMP/MOV_imm
        FromBitString16("010000ooooxxxxxx"), // Data Processing
        FromBitString16("010001000hxxxxxx"), // ADD (high registers)
        FromBitString16("010001010hxxxxxx"), // CMP (high registers)
        FromBitString16("01000101h0xxxxxx"), // CMP (high registers)
        FromBitString16("010001100hxxxxxx"), // MOV (high registers)
        FromBitString16("10110000oxxxxxxx"), // Adjust stack pointer
        FromBitString16("10110010ooxxxxxx"), // SXT/UXT
        FromBitString16("1011101000xxxxxx"), // REV
        FromBitString16("1011101001xxxxxx"), // REV16
        FromBitString16("1011101011xxxxxx"), // REVSH
    }};

    auto instruction_select = [&](int) -> u16 {
        size_t inst_index = RandInt<size_t>(0, instructions.size() - 1);

        u16 random = RandInt<u16>(0, 0xFFFF);

        return instructions[inst_index].first | (random &~ instructions[inst_index].second);
    };

    SECTION("short blocks") {
        FuzzJitThumb(5, 6, 10000, instruction_select);
    }

    SECTION("long blocks") {
        FuzzJitThumb(1024, 1025, 15, instruction_select);
    }
}

TEST_CASE("Fuzz Thumb instructions set 2 (affects PC)", "[JitX64][Thumb]") {
    const std::array<std::pair<u16, u16>, 5> instructions = {{
        FromBitString16("01000111xxxxx000"), // BLX/BX
        FromBitString16("1010oxxxxxxxxxxx"), // add to pc/sp
        FromBitString16("11100xxxxxxxxxxx"), // B
        FromBitString16("01000100h0xxxxxx"), // ADD (high registers)
        FromBitString16("01000110h0xxxxxx"), // MOV (high registers)
    }};

    auto instruction_select = [&](int) -> u16 {
        size_t inst_index = RandInt<size_t>(0, instructions.size() - 1);

        u16 random = RandInt<u16>(0, 0xFFFF);

        return instructions[inst_index].first | (random &~ instructions[inst_index].second);
    };

    FuzzJitThumb(1, 1, 10000, instruction_select);
}

TEST_CASE("Fuzz Thumb instructions set 3 (32-bit BL/BLX)", "[JitX64][Thumb]") {
    auto instruction_select = [&](int i) -> u16 {
        std::pair<u16, u16> inst_info;
        if (i == 0) {
            // BL / BLX prefix
            inst_info = FromBitString16("11110xxxxxxxxxxx");
        } else {
            // BL / BLX suffix
            inst_info = RandInt(0, 1) ? FromBitString16("11101xxxxxxxxxx0") : FromBitString16("11111xxxxxxxxxxx");
        }

        u16 random = RandInt<u16>(0, 0xFFFF);

        return inst_info.first | (random &~ inst_info.second);
    };

    FuzzJitThumb(2, 1, 1000, instruction_select);
}