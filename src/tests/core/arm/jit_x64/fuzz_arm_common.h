// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <functional>
#include <utility>

#include "common/common_types.h"

std::pair<u32, u32> FromBitString32(const char* str);

/**
 * For run_count times:
 * 1. Generates instruction_count instructions using instruction_generator
 * 2. Requests JitX64 and interpreter to execute them for instructions_to_execute_count instructions
 * 3. Verifies that the register state are equivalent
 *
 * Note: A b #+0 instruction is appended to the list of instructions.
 */
void FuzzJit(const int instruction_count, const int instructions_to_execute_count, const int run_count, const std::function<u32()> instruction_generator);
