// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <cmath>
#include "common/assert.h"
#include "core/core.h"
#include "core/frontend/emu_window.h"
#include "core/settings.h"
#include "video_core/video_core.h"

void EmuWindow::TouchPressed(u16 x, u16 y) {
    std::lock_guard<std::mutex> lock(touch_mutex);
    touch_x = x;
    touch_y = y;
    touch_pressed = true;
}

void EmuWindow::TouchReleased() {
    std::lock_guard<std::mutex> lock(touch_mutex);
    touch_x = 0;
    touch_y = 0;
    touch_pressed = false;
}

void EmuWindow::AccelerometerChanged(float x, float y, float z) {
    constexpr float coef = 512;

    std::lock_guard<std::mutex> lock(accel_mutex);

    // TODO(wwylele): do a time stretch as it in GyroscopeChanged
    // The time stretch formula should be like
    // stretched_vector = (raw_vector - gravity) * stretch_ratio + gravity
    accel_x = static_cast<s16>(x * coef);
    accel_y = static_cast<s16>(y * coef);
    accel_z = static_cast<s16>(z * coef);
}

void EmuWindow::GyroscopeChanged(float x, float y, float z) {
    constexpr float FULL_FPS = 60;
    float coef = GetGyroscopeRawToDpsCoefficient();
    float stretch = Core::System::GetInstance().perf_stats.GetLastFrameTimeScale();
    std::lock_guard<std::mutex> lock(gyro_mutex);
    gyro_x = static_cast<s16>(x * coef * stretch);
    gyro_y = static_cast<s16>(y * coef * stretch);
    gyro_z = static_cast<s16>(z * coef * stretch);
}
