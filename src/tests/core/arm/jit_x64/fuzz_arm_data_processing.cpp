// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <random>

#include <catch.hpp>

#include "common/common_types.h"
#include "common/scope_exit.h"

#include "core/arm/disassembler/arm_disasm.h"
#include "core/arm/dyncom/arm_dyncom.h"
#include "core/arm/jit_x64/interface.h"
#include "core/core.h"
#include "core/memory_setup.h"

std::pair<u32, u32> FromBitString(const char* str) {
    REQUIRE(strlen(str) == 32);

    u32 bits = 0;
    u32 mask = 0;
    for (int i = 0; i < 32; i++) {
        const u32 bit = 1 << (31 - i);
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
    return { bits, mask };
}

TEST_CASE("Fuzz ARM data processing instructions", "[JitX64]") {

    // Init core
    Core::Init();
    SCOPE_EXIT({ Core::Shutdown(); });

    // Prepare random numbers
    std::random_device rd;
    std::mt19937 mt(rd());
    auto rand_int = [&mt](u32 min, u32 max) -> u32 {
        std::uniform_int<u32> rand(min, max);
        return rand(mt);
    };

    // Prepare memory
    u8* test_mem = new u8[4096 * 2];
    std::memset(test_mem, 0, 4096 * 2);
    Memory::MapMemoryRegion(0, 4096 * 2, test_mem);
    SCOPE_EXIT({ Memory::UnmapRegion(0, 4096 * 2); });

    // Prepare test subjects
    JitX64::ARM_Jit jit(PrivilegeMode::USER32MODE);
    ARM_DynCom interp(PrivilegeMode::USER32MODE);
    SCOPE_EXIT({
        jit.ClearCache();
        interp.ClearCache();
    });

    for (int run_number = 0; run_number < 10000; run_number++) {
        jit.ClearCache();
        interp.ClearCache();

        u32 initial_regs[15];
        for (int i = 0; i < 15; i++) {
            u32 val = rand_int(0, 0xFFFFFFFF);
            interp.SetReg(i, val);
            jit.SetReg(i, val);
            initial_regs[i] = val;
        }

        interp.SetCPSR(0x000001d0);
        jit.SetCPSR(0x000001d0);

        interp.SetPC(0);
        jit.SetPC(0);

        constexpr int NUM_INST = 5;

        for (int i = 0; i < NUM_INST; i++) {
            const std::array<std::pair<u32, u32>, 48> instructions = {{
                FromBitString("cccc0010101Snnnnddddrrrrvvvvvvvv"),
                FromBitString("cccc0000101Snnnnddddvvvvvrr0mmmm"),
                FromBitString("cccc0000101Snnnnddddssss0rr1mmmm"),
                FromBitString("cccc0010100Snnnnddddrrrrvvvvvvvv"),
                FromBitString("cccc0000100Snnnnddddvvvvvrr0mmmm"),
                FromBitString("cccc0000100Snnnnddddssss0rr1mmmm"),
                FromBitString("cccc0010000Snnnnddddrrrrvvvvvvvv"),
                FromBitString("cccc0000000Snnnnddddvvvvvrr0mmmm"),
                FromBitString("cccc0000000Snnnnddddssss0rr1mmmm"),
                FromBitString("cccc0011110Snnnnddddrrrrvvvvvvvv"),
                FromBitString("cccc0001110Snnnnddddvvvvvrr0mmmm"),
                FromBitString("cccc0001110Snnnnddddssss0rr1mmmm"),
                FromBitString("cccc00110111nnnn0000rrrrvvvvvvvv"),
                FromBitString("cccc00010111nnnn0000vvvvvrr0mmmm"),
                FromBitString("cccc00010111nnnn0000ssss0rr1mmmm"),
                FromBitString("cccc00110101nnnn0000rrrrvvvvvvvv"),
                FromBitString("cccc00010101nnnn0000vvvvvrr0mmmm"),
                FromBitString("cccc00010101nnnn0000ssss0rr1mmmm"),
                FromBitString("cccc0010001Snnnnddddrrrrvvvvvvvv"),
                FromBitString("cccc0000001Snnnnddddvvvvvrr0mmmm"),
                FromBitString("cccc0000001Snnnnddddssss0rr1mmmm"),
                FromBitString("cccc0011101S0000ddddrrrrvvvvvvvv"),
                FromBitString("cccc0001101S0000ddddvvvvvrr0mmmm"),
                FromBitString("cccc0001101S0000ddddssss0rr1mmmm"),
                FromBitString("cccc0011111S0000ddddrrrrvvvvvvvv"),
                FromBitString("cccc0001111S0000ddddvvvvvrr0mmmm"),
                FromBitString("cccc0001111S0000ddddssss0rr1mmmm"),
                FromBitString("cccc0011100Snnnnddddrrrrvvvvvvvv"),
                FromBitString("cccc0001100Snnnnddddvvvvvrr0mmmm"),
                FromBitString("cccc0001100Snnnnddddssss0rr1mmmm"),
                FromBitString("cccc0010011Snnnnddddrrrrvvvvvvvv"),
                FromBitString("cccc0000011Snnnnddddvvvvvrr0mmmm"),
                FromBitString("cccc0000011Snnnnddddssss0rr1mmmm"),
                FromBitString("cccc0010111Snnnnddddrrrrvvvvvvvv"),
                FromBitString("cccc0000111Snnnnddddvvvvvrr0mmmm"),
                FromBitString("cccc0000111Snnnnddddssss0rr1mmmm"),
                FromBitString("cccc0010110Snnnnddddrrrrvvvvvvvv"),
                FromBitString("cccc0000110Snnnnddddvvvvvrr0mmmm"),
                FromBitString("cccc0000110Snnnnddddssss0rr1mmmm"),
                FromBitString("cccc0010010Snnnnddddrrrrvvvvvvvv"),
                FromBitString("cccc0000010Snnnnddddvvvvvrr0mmmm"),
                FromBitString("cccc0000010Snnnnddddssss0rr1mmmm"),
                FromBitString("cccc00110011nnnn0000rrrrvvvvvvvv"),
                FromBitString("cccc00010011nnnn0000vvvvvrr0mmmm"),
                FromBitString("cccc00010011nnnn0000ssss0rr1mmmm"),
                FromBitString("cccc00110001nnnn0000rrrrvvvvvvvv"),
                FromBitString("cccc00010001nnnn0000vvvvvrr0mmmm"),
                FromBitString("cccc00010001nnnn0000ssss0rr1mmmm"),
            }};

            size_t inst_index = rand_int(0, instructions.size() - 1);
            u32 cond = rand_int(0x0, 0xE);
            u32 Rn = rand_int(0, 15);
            u32 Rd = rand_int(0, 14);
            u32 S = rand_int(0, 1);
            u32 shifter_operand = rand_int(0, 0xFFF);

            u32 assemble_randoms = (shifter_operand << 0) | (Rd << 12) | (Rn << 16) | (S << 20) | (cond << 28);

            u32 inst = instructions[inst_index].first | (assemble_randoms & (~instructions[inst_index].second));

            Memory::Write32(i * 4, inst);
        }

        Memory::Write32(NUM_INST * 4, 0b0011001000001111000000000000);

        interp.ExecuteInstructions(NUM_INST);
        jit.ExecuteInstructions(NUM_INST);

        bool pass = true;

        if (interp.GetCPSR() != jit.GetCPSR()) pass = false;
        for (int i = 0; i <= 15; i++) {
            if (interp.GetReg(i) != jit.GetReg(i)) pass = false;
        }

        if (!pass) {
            printf("Failed at execution number %i\n", run_number);

            printf("\nInstruction Listing: \n");
            for (int i = 0; i < NUM_INST; i++) {
                printf("%s\n", ARM_Disasm::Disassemble(i * 4, Memory::Read32(i * 4)).c_str());
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
            for (int inst = 0; inst < NUM_INST; inst++) {
                printf("%s\n", ARM_Disasm::Disassemble(inst * 4, Memory::Read32(inst * 4)).c_str());
                interp.Step();
                for (int i = 0; i <= 15; i++) {
                    printf("%4i: %08x\n", i, interp.GetReg(i));
                }
                printf("CPSR: %08x\n", interp.GetCPSR());
            }

            FAIL();
        }
    }
}