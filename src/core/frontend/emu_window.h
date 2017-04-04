// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <mutex>
#include <tuple>
#include <utility>
#include "common/common_types.h"
#include "common/math_util.h"
#include "core/frontend/framebuffer.h"

/**
 * Abstraction class used to provide an interface between emulation code and the frontend
 * (e.g. SDL, QGLWidget, GLFW, etc...).
 *
 * Design notes on the interaction between EmuWindow and the emulation core:
 * - Generally, decisions on anything visible to the user should be left up to the GUI.
 *   For example, the emulation core should not try to dictate some window title or size.
 *   This stuff is not the core's business and only causes problems with regards to thread-safety
 *   anyway.
 * - Under certain circumstances, it may be desirable for the core to politely request the GUI
 *   to set e.g. a minimum window size. However, the GUI should always be free to ignore any
 *   such hints.
 * - EmuWindow may expose some of its state as read-only to the emulation core, however care
 *   should be taken to make sure the provided information is self-consistent. This requires
 *   some sort of synchronization (most of this is still a TODO).
 * - DO NOT TREAT THIS CLASS AS A GUI TOOLKIT ABSTRACTION LAYER. That's not what it is. Please
 *   re-read the upper points again and think about it if you don't see this.
 */
class EmuWindow {
public:
    /**
     * Signal accelerometer state has changed.
     * @param x X-axis accelerometer value
     * @param y Y-axis accelerometer value
     * @param z Z-axis accelerometer value
     * @note all values are in unit of g (gravitational acceleration).
     *    e.g. x = 1.0 means 9.8m/s^2 in x direction.
     * @see GetAccelerometerState for axis explanation.
     */
    void AccelerometerChanged(float x, float y, float z);

    /**
     * Signal gyroscope state has changed.
     * @param x X-axis accelerometer value
     * @param y Y-axis accelerometer value
     * @param z Z-axis accelerometer value
     * @note all values are in deg/sec.
     * @see GetGyroscopeState for axis explanation.
     */
    void GyroscopeChanged(float x, float y, float z);

    /**
     * Called by a Framebuffer to set the touch state for the core.
     * @param x Native 3ds x coordinate
     * @param y Native 3ds y coordinate
     */
    void TouchPressed(u16 x, u16 y) {
        std::lock_guard<std::mutex> lock(touch_mutex);
        touch_x = x;
        touch_y = y;
        touch_pressed = true;
    }

    /**
     * Called by a Framebuffer to clear any touch state
     */
    void TouchReleased() {
        std::lock_guard<std::mutex> lock(touch_mutex);
        touch_x = 0;
        touch_y = 0;
        touch_pressed = false;
    }

    /**
     * Gets the current touch screen state (touch X/Y coordinates and whether or not it is pressed).
     * @note This should be called by the core emu thread to get a state set by the window thread.
     * @return std::tuple of (x, y, pressed) where `x` and `y` are the touch coordinates and
     *         `pressed` is true if the touch screen is currently being pressed
     */
    std::tuple<u16, u16, bool> GetTouchState() const {
        std::lock_guard<std::mutex> lock(touch_mutex);
        return std::make_tuple(touch_x, touch_y, touch_pressed);
    }

    /**
     * Gets the current accelerometer state (acceleration along each three axis).
     * Axis explained:
     *   +x is the same direction as LEFT on D-pad.
     *   +y is normal to the touch screen, pointing outward.
     *   +z is the same direction as UP on D-pad.
     * Units:
     *   1 unit of return value = 1/512 g (measured by hw test),
     *   where g is the gravitational acceleration (9.8 m/sec2).
     * @note This should be called by the core emu thread to get a state set by the window thread.
     * @return std::tuple of (x, y, z)
     */
    std::tuple<s16, s16, s16> GetAccelerometerState() const {
        std::lock_guard<std::mutex> lock(accel_mutex);
        return std::make_tuple(accel_x, accel_y, accel_z);
    }

    /**
     * Gets the current gyroscope state (angular rates about each three axis).
     * Axis explained:
     *   +x is the same direction as LEFT on D-pad.
     *   +y is normal to the touch screen, pointing outward.
     *   +z is the same direction as UP on D-pad.
     * Orientation is determined by right-hand rule.
     * Units:
     *   1 unit of return value = (1/coef) deg/sec,
     *   where coef is the return value of GetGyroscopeRawToDpsCoefficient().
     * @note This should be called by the core emu thread to get a state set by the window thread.
     * @return std::tuple of (x, y, z)
     */
    std::tuple<s16, s16, s16> GetGyroscopeState() const {
        std::lock_guard<std::mutex> lock(gyro_mutex);
        return std::make_tuple(gyro_x, gyro_y, gyro_z);
    }

    /**
     * Gets the coefficient for units conversion of gyroscope state.
     * The conversion formula is r = coefficient * v,
     * where v is angular rate in deg/sec,
     * and r is the gyroscope state.
     * @return float-type coefficient
     */
    constexpr f32 GetGyroscopeRawToDpsCoefficient() const {
        return 14.375f; // taken from hw test, and gyroscope's document
    }

protected:
    EmuWindow(std::vector<Framebuffer*> screens) {
        screens = screens;
        touch_x = 0;
        touch_y = 0;
        touch_pressed = false;
        accel_x = 0;
        accel_y = -512;
        accel_z = 0;
        gyro_x = 0;
        gyro_y = 0;
        gyro_z = 0;
    }
    virtual ~EmuWindow() {}

private:
    std::vector<std::shared_ptr<Framebuffer>> screens;
    std::mutex touch_mutex;
    u16 touch_x;        ///< Touchpad X-position in native 3DS pixel coordinates (0-320)
    u16 touch_y;        ///< Touchpad Y-position in native 3DS pixel coordinates (0-240)
    bool touch_pressed; ///< True if touchpad area is currently pressed, otherwise false

    std::mutex accel_mutex;
    s16 accel_x; ///< Accelerometer X-axis value in native 3DS units
    s16 accel_y; ///< Accelerometer Y-axis value in native 3DS units
    s16 accel_z; ///< Accelerometer Z-axis value in native 3DS units

    std::mutex gyro_mutex;
    s16 gyro_x; ///< Gyroscope X-axis value in native 3DS units
    s16 gyro_y; ///< Gyroscope Y-axis value in native 3DS units
    s16 gyro_z; ///< Gyroscope Z-axis value in native 3DS units
};
