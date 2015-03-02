// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

// Originally written by Sven Peter <sven@fail0verflow.com> for anergistic.

#pragma once

#include <signal.h>

#include "common/common.h"
#include "common/thread.h"
#include "common/break_points.h"

#include "core/core.h"
#include "core/mem_map.h"
#include "core/arm/arm_interface.h"

#ifdef _WIN32
#define SIGTRAP      5
#define SIGTERM     15
#define MSG_WAITALL  8
#endif

// typedef enum {
//     GDB_BP_TYPE_NONE = 0,
//     GDB_BP_TYPE_X,
//     GDB_BP_TYPE_R,
//     GDB_BP_TYPE_W,
//     GDB_BP_TYPE_A
// } gdb_bp_type;


namespace GDB {
struct BreakPoint {
    u32 address;
    u32 length;
    bool active;
};
void Init(u32 port);
void DeInit();
bool IsActive();
void Break();

void HandleException();
int  Signal(u32 signal);

// int  gdb_bp_x(u32 addr);
// int  gdb_bp_r(u32 addr);
// int  gdb_bp_w(u32 addr);
// int  gdb_bp_a(u32 addr);

// bool AddBreakPoint(u32 type, u32 addr, u32 len);

void AddBreakPoint(TBreakPoint& bp);
}