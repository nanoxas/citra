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

#ifdef _WIN32
#define SIGTRAP      5
#define SIGTERM     15
#define MSG_WAITALL  8
// windows uses SD_BOTH instead of SHUT_* and SD_BOTH == 2
#define SHUT_RDWR 2
#endif

namespace GDB {

struct BreakPoint {
    u32 address;
    u32 length;
    bool active;
};

void Init(u32 port);
void DeInit();
bool IsActive();
bool IsStepping();

void HandleException();
int  Signal(u32 signal);

// bool AddBreakPointHere();
void Break();

extern std::vector<BreakPoint> breakpoints;

}