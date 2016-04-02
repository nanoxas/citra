// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <cinttypes>
#include <cstdio>
#include <cstring>

#include <catch.hpp>

#include "common/make_unique.h"
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

class TestMemory final : public Memory::MMIORegion {
public:
    static constexpr size_t CODE_MEMORY_SIZE = 4096 * 2;
    std::array<u16, CODE_MEMORY_SIZE> code_mem{};

    u8 Read8(VAddr addr) override { return addr; }
    u16 Read16(VAddr addr) override {
        if (addr < CODE_MEMORY_SIZE) {
            addr /= 2;
            return code_mem[addr];
        } else {
            return addr;
        }
    }
    u32 Read32(VAddr addr) override {
        if (addr < CODE_MEMORY_SIZE) {
            addr /= 2;
            return code_mem[addr] | (code_mem[addr+1] << 16);
        } else {
            return addr;
        }
    }
    u64 Read64(VAddr addr) override { return addr; }

    struct WriteRecord {
        WriteRecord(size_t size, VAddr addr, u64 data) : size(size), addr(addr), data(data) {}
        size_t size;
        VAddr addr;
        u64 data;
        bool operator==(const WriteRecord& o) const {
            return std::tie(size, addr, data) == std::tie(o.size, o.addr, o.data);
        }
    };

    std::vector<WriteRecord> recording;

    void Write8(VAddr addr, u8 data) override { recording.emplace_back(1, addr, data); }
    void Write16(VAddr addr, u16 data) override { recording.emplace_back(2, addr, data); }
    void Write32(VAddr addr, u32 data) override { recording.emplace_back(4, addr, data); }
    void Write64(VAddr addr, u64 data) override { recording.emplace_back(8, addr, data); }
};

void FuzzJitThumb(const int instruction_count, const int instructions_to_execute_count, const int run_count, const std::function<u16(int)> instruction_generator) {
    // Init core
    Core::Init();
    SCOPE_EXIT({ Core::Shutdown(); });

    // Prepare memory
    std::shared_ptr<TestMemory> test_mem = std::make_shared<TestMemory>();
    Memory::MapIoRegion(0x00000000, 0x80000000, test_mem);
    Memory::MapIoRegion(0x80000000, 0x80000000, test_mem);
    SCOPE_EXIT({
        Memory::UnmapRegion(0x00000000, 0x80000000);
    Memory::UnmapRegion(0x80000000, 0x80000000);
    });

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

        test_mem->code_mem[0] = 0xFFFF;
        test_mem->code_mem[1] = 0xFAFF; // blx +#4 // Jump to the following code (switch to thumb)

        for (int i = 0; i < instruction_count; i++) {
            u16 inst = instruction_generator(i);
            test_mem->code_mem[2 + i] = inst;
        }

        test_mem->code_mem[2 + instruction_count] = 0xE7FE; // b +#0 // busy wait loop

        test_mem->recording.clear();
        interp.ExecuteInstructions(instructions_to_execute_count);
        auto interp_mem_recording = test_mem->recording;

        test_mem->recording.clear();
        jit.ExecuteInstructions(instructions_to_execute_count);
        auto jit_mem_recording = test_mem->recording;

        bool pass = true;

        if (interp.GetCPSR() != jit.GetCPSR()) pass = false;
        for (int i = 0; i <= 15; i++) {
            if (interp.GetReg(i) != jit.GetReg(i)) pass = false;
        }
        if (interp_mem_recording != jit_mem_recording) pass = false;

        if (!pass) {
            printf("Failed at execution number %i\n", run_number);

            printf("\nInstruction Listing: \n");
            for (int i = 0; i < instruction_count; i++) {
                printf("%04x\n", test_mem->code_mem[2 + i]);
            }

            printf("\nFinal Register Listing: \n");
            for (int i = 0; i <= 15; i++) {
                printf("%4i: %08x %08x %s\n", i, interp.GetReg(i), jit.GetReg(i), interp.GetReg(i) != jit.GetReg(i) ? "*" : "");
            }
            printf("CPSR: %08x %08x %s\n", interp.GetCPSR(), jit.GetCPSR(), interp.GetCPSR() != jit.GetCPSR() ? "*" : "");

            if (interp_mem_recording != jit_mem_recording) {
                printf("memory write recording mismatch *\n");
                size_t i = 0;
                while (i < interp_mem_recording.size() || i < jit_mem_recording.size()) {
                    if (i < interp_mem_recording.size())
                        printf("interp: %zu %08x %08" PRIx64 "\n", interp_mem_recording[i].size, interp_mem_recording[i].addr, interp_mem_recording[i].data);
                    if (i < jit_mem_recording.size())
                        printf("jit   : %zu %08x %08" PRIx64 "\n", jit_mem_recording[i].size, jit_mem_recording[i].addr, jit_mem_recording[i].data);
                    i++;
                }
            }

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
//
// FromBitString16("10111110xxxxxxxx"), // BKPT
// FromBitString16("10110110011x0xxx"), // CPS
// FromBitString16("11011111xxxxxxxx"), // SWI
// FromBitString16("1011x101xxxxxxxx"), // PUSH/POP (R = 1)

TEST_CASE("Fuzz Thumb instructions set 1", "[JitX64][Thumb]") {
    const std::array<std::pair<u16, u16>, 24> instructions = {{
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
        FromBitString16("01001xxxxxxxxxxx"), // LDR Rd, [PC, #]
        FromBitString16("0101oooxxxxxxxxx"), // LDR/STR Rd, [Rn, Rm]
        FromBitString16("011xxxxxxxxxxxxx"), // LDR(B)/STR(B) Rd, [Rn, #]
        FromBitString16("1000xxxxxxxxxxxx"), // LDRH/STRH Rd, [Rn, #offset]
        FromBitString16("1001xxxxxxxxxxxx"), // LDR/STR Rd, [SP, #]
        FromBitString16("1011x100xxxxxxxx"), // PUSH/POP (R = 0)
        FromBitString16("1100xxxxxxxxxxxx"), // STMIA/LDMIA
        FromBitString16("101101100101x000"), // SETEND
    }};

    auto instruction_select = [&](int) -> u16 {
        size_t inst_index = RandInt<size_t>(0, instructions.size() - 1);

        if (inst_index == 22) {
            u16 L = RandInt<u16>(0, 1);
            u16 Rn = RandInt<u16>(0, 7);
            u16 reg_list = RandInt<u16>(1, 0xFF);
            if (!L && (reg_list & (1 << Rn))) {
                reg_list &= ~((1 << Rn) - 1);
                if (reg_list == 0) reg_list = 0x80;
            }
            u16 random = (L << 11) | (Rn << 8) | reg_list;
            return instructions[inst_index].first | (random &~instructions[inst_index].second);
        } else if (inst_index == 21) {
            u16 L = RandInt<u16>(0, 1);
            u16 reg_list = RandInt<u16>(1, 0xFF);
            u16 random = (L << 11) | reg_list;
            return instructions[inst_index].first | (random &~instructions[inst_index].second);
        } else {
            u16 random = RandInt<u16>(0, 0xFFFF);
            return instructions[inst_index].first | (random &~instructions[inst_index].second);
        }
    };

    SECTION("short blocks") {
        FuzzJitThumb(5, 6, 10000, instruction_select);
    }

    SECTION("long blocks") {
        FuzzJitThumb(1024, 1025, 15, instruction_select);
    }
}

TEST_CASE("Fuzz Thumb instructions set 2 (affects PC)", "[JitX64][Thumb]") {
    const std::array<std::pair<u16, u16>, 18> instructions = {{
        FromBitString16("01000111xxxxx000"), // BLX/BX
        FromBitString16("1010oxxxxxxxxxxx"), // add to pc/sp
        FromBitString16("11100xxxxxxxxxxx"), // B
        FromBitString16("01000100h0xxxxxx"), // ADD (high registers)
        FromBitString16("01000110h0xxxxxx"), // MOV (high registers)
        FromBitString16("11010001xxxxxxxx"), // B<cond>
        FromBitString16("11010010xxxxxxxx"), // B<cond>
        FromBitString16("11010011xxxxxxxx"), // B<cond>
        FromBitString16("11010100xxxxxxxx"), // B<cond>
        FromBitString16("11010101xxxxxxxx"), // B<cond>
        FromBitString16("11010110xxxxxxxx"), // B<cond>
        FromBitString16("11010111xxxxxxxx"), // B<cond>
        FromBitString16("11011000xxxxxxxx"), // B<cond>
        FromBitString16("11011001xxxxxxxx"), // B<cond>
        FromBitString16("11011010xxxxxxxx"), // B<cond>
        FromBitString16("11011011xxxxxxxx"), // B<cond>
        FromBitString16("11011100xxxxxxxx"), // B<cond>
        FromBitString16("11011110xxxxxxxx"), // B<cond>
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