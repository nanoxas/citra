// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/common_types.h"

namespace JitX64 {

// TODO: Set up an appropriately mapped region of memory to use instead of compiling CALL instructions.

constexpr u32 RESERVATION_GRANULE_MASK = 0xFFFFFFF8;

u64 Load64LE(u32 addr);
u64 Load64BE(u32 addr);
void Store64LE(u32 addr, u32 v1, u32 v2);
void Store64BE(u32 addr, u32 v1, u32 v2);

u32 Load32LE(u32 addr);
u32 Load32BE(u32 addr);
void Store32LE(u32 addr, u32 value);
void Store32BE(u32 addr, u32 value);

u16 Load16LE(u32 addr);
u16 Load16BE(u32 addr);
void Store16LE(u32 addr, u16 value);
void Store16BE(u32 addr, u16 value);

u32 Load8(u32 addr);
void Store8(u32 addr, u8 value);

} // namespace JitX64
