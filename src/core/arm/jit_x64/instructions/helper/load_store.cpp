// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/common_types.h"
#include "common/swap.h"

#include "core/arm/jit_x64/instructions/helper/load_store.h"
#include "core/memory.h"

namespace JitX64 {

u64 Load64LE(u32 addr) {
    // TODO: Improve this.
    return Memory::Read32(addr) | (static_cast<u64>(Memory::Read32(addr + 4)) << 32);
}

u64 Load64BE(u32 addr) {
    // TODO: Improve this.
    return Common::swap32(Memory::Read32(addr)) | (static_cast<u64>(Common::swap32(Memory::Read32(addr + 4))) << 32);
}

void Store64LE(u32 addr, u32 v1, u32 v2) {
    Memory::Write32(addr, v1);
    Memory::Write32(addr + 4, v2);
}

void Store64BE(u32 addr, u32 v1, u32 v2) {
    Memory::Write32(addr, Common::swap32(v2));
    Memory::Write32(addr + 4, Common::swap32(v1));
}

u32 Load32LE(u32 addr) {
    return Memory::Read32(addr);
}

u32 Load32BE(u32 addr) {
    return Common::swap32(Memory::Read32(addr));
}

void Store32LE(u32 addr, u32 value) {
    Memory::Write32(addr, value);
}

void Store32BE(u32 addr, u32 value) {
    Memory::Write32(addr, Common::swap32(value));
}

u16 Load16LE(u32 addr) {
    return Memory::Read16(addr);
}

u16 Load16BE(u32 addr) {
    return Common::swap16(Memory::Read16(addr));
}

void Store16LE(u32 addr, u16 value) {
    Memory::Write16(addr, value);
}

void Store16BE(u32 addr, u16 value) {
    Memory::Write16(addr, Common::swap16(value));
}

u32 Load8(u32 addr) {
    return Memory::Read8(addr);
}

void Store8(u32 addr, u8 value) {
    Memory::Write8(addr, value);
}

}