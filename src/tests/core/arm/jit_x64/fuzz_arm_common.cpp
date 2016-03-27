// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <cstdio>
#include <cstring>

#include <catch.hpp>

#include "common/common_types.h"
#include "common/scope_exit.h"

#include "core/arm/disassembler/arm_disasm.h"
#include "core/arm/dyncom/arm_dyncom.h"
#include "core/arm/jit_x64/interface.h"
#include "core/core.h"
#include "core/memory_setup.h"

#include "tests/core/arm/jit_x64/fuzz_arm_common.h"
#include "tests/core/arm/jit_x64/rand_int.h"

std::pair<u32, u32> FromBitString32(const char* str) {
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
    return{ bits, mask };
}

void FuzzJit(const int instruction_count, const int instructions_to_execute_count, const int run_count, const std::function<u32()> instruction_generator) {
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

        for (int i = 0; i < instruction_count; i++) {
            u32 inst = instruction_generator();

            Memory::Write32(i * 4, inst);
        }

        Memory::Write32(instruction_count * 4, 0xEAFFFFFE); // b +#0 // busy wait loop

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
                printf("%s\n", ARM_Disasm::Disassemble(i * 4, Memory::Read32(i * 4)).c_str());
            }

            printf("\nFinal Register Listing: \n");
            printf("   R  interp   jit\n");
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
                printf("%s\n", ARM_Disasm::Disassemble(inst * 4, Memory::Read32(inst * 4)).c_str());
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