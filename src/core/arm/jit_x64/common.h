// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/common_types.h"

#include "core/arm/decoder/decoder.h"
#include "core/arm/skyeye_common/armstate.h"

namespace JitX64 {

using ArmReg = ArmDecoder::Register;
using ArmRegList = ArmDecoder::RegisterList;
using ArmImm4 = ArmDecoder::Imm4;
using ArmImm5 = ArmDecoder::Imm5;
using ArmImm8 = ArmDecoder::Imm8;
using ArmImm11 = ArmDecoder::Imm11;
using ArmImm12 = ArmDecoder::Imm12;
using ArmImm24 = ArmDecoder::Imm24;
using Cond = ArmDecoder::Cond;
using ShiftType = ArmDecoder::ShiftType;
using SignExtendRotation = ArmDecoder::SignExtendRotation;

struct JitState {
    JitState() : cpu_state(PrivilegeMode::USER32MODE) {}
    void Reset() {
        cpu_state.Reset();
    }

    ARMul_State cpu_state;

    u64 save_host_RSP;
    u64 return_RIP;

    s32 cycles_remaining;
};

}
