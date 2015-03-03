// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/common_types.h"

#include "core/core.h"
#include "core/core_timing.h"

#include "core/settings.h"
#include "core/arm/arm_interface.h"
#include "core/arm/disassembler/arm_disasm.h"
#include "core/arm/dyncom/arm_dyncom.h"
#include "core/hle/hle.h"
#include "core/hle/kernel/thread.h"
#include "core/hw/hw.h"
// #ifdef USE_GDBSTUB
#include "core/arm/gdb_stub.h"
// #endif

namespace Core {

ARM_Interface*     g_app_core = nullptr;  ///< ARM11 application core
ARM_Interface*     g_sys_core = nullptr;  ///< ARM11 system (OS) core

/// Run the core CPU loop
void RunLoop(int tight_loop) {
    // If the current thread is an idle thread, then don't execute instructions,
    // instead advance to the next event and try to yield to the next thread
    if (Kernel::GetCurrentThread()->IsIdle()) {
        LOG_TRACE(Core_ARM11, "Idling");
        CoreTiming::Idle();
        CoreTiming::Advance();
        HLE::Reschedule(__func__);
    } else {
        g_app_core->Run(tight_loop);
    }

    HW::Update();
    if (HLE::g_reschedule) {
        Kernel::Reschedule();
    }
}

/// Step the CPU one instruction
void SingleStep() {
    RunLoop(1);
}

/// 
void Debug() {
    SingleStep();
    if (GDB::IsStepping()) {
        GDB::Signal(SIGTRAP);
        GDB::HandleException();
    }
}

/// Halt the core
void Halt(const char *msg) {
    // TODO(ShizZy): ImplementMe
    LOG_DEBUG(Core, "Halt called with message %s", msg);
    
}

/// Kill the core
void Stop() {
    // TODO(ShizZy): ImplementMe
}

/// Initialize the core
int Init() {
    g_sys_core = new ARM_DynCom(USER32MODE);
    g_app_core = new ARM_DynCom(USER32MODE);

    LOG_DEBUG(Core, "Initialized OK");

    // if (_CoreParameter.iGDBPort > 0) {
    if (Settings::values.gdb_port > 0) {
        GDB::Init(12345);
        GDB::Break();
    }
    // break at next instruction (the first instruction)
    // gdb_break();
    // }
    return 0;
}

void Shutdown() {
    delete g_app_core;
    delete g_sys_core;

    LOG_DEBUG(Core, "Shutdown OK");
}

} // namespace
