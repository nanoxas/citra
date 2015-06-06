// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

// Originally written by Sven Peter <sven@fail0verflow.com> for anergistic.

#pragma once

#include <signal.h>

#include "common/common.h"
#include "common/thread.h"

#include "core/core.h"
#include "core/mem_map.h"
#include "core/arm/arm_interface.h"

// help find problems with the stub
// (gdb) set debug remote 1


#ifdef _WIN32
#define SIGTRAP      5
#define SIGTERM     15
#endif

namespace GDB {

struct BreakPoint {
// BreakPoints are stored in a map<addr, BP> so having it in the struct seems redundant
//    u32 address;
    u32 length;
};

void Init(u16 port);
void DeInit();
bool IsActive();
bool IsStepping();

void HandleException();
int  Signal(u32 signal);

void Break();

}