// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <tuple>
#include "common/vector_math.h"

namespace Input {

/// An abstract class template for an input device (a button, an analog input, etc.).
template <typename StatusType>
class InputDevice {
public:
    virtual ~InputDevice() = default;
    virtual StatusType GetStatus() const {
        return {};
    }
};

/**
 * A button device is an input device that returns bool as status.
 * true for pressed; false for released.
 */
using ButtonDevice = InputDevice<bool>;

/**
 * An analog device is an input device that returns a tuple of x and y coordinates as status. The
 * coordinates are within the unit circle. x+ is defined as right direction, and y+ is defined as up
 * direction
 */
using AnalogDevice = InputDevice<std::tuple<float, float>>;

/**
 * A motion device is an input device that returns a tuple of accelerometer state vector and
 * gyroscope state vector.
 *
 * For both vectors:
 *   x+ is the same direction as LEFT on D-pad.
 *   y+ is normal to the touch screen, pointing outward.
 *   z+ is the same direction as UP on D-pad.
 *
 * For accelerometer state vector
 *   Units: g (gravitational acceleration)
 *
 * For gyroscope state vector:
 *   Orientation is determined by right-hand rule.
 *   Units: deg/sec
 */
using MotionDevice = InputDevice<std::tuple<Math::Vec3<float>, Math::Vec3<float>>>;

/**
 * A touch device is an input device that returns a tuple of two floats and a bool. The floats are
 * x and y coordinates in the range 0.0 - 1.0, and the bool indicates whether it is pressed.
 */
using TouchDevice = InputDevice<std::tuple<float, float, bool>>;

} // namespace Input
