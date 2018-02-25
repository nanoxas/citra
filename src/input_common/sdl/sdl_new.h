// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <atomic>
#include <future>
#include <memory>
#include <mutex>
#include <unordered_map>
#include "core/frontend/input.h"

union SDL_Event;
namespace Common {
class ParamPackage;
}
namespace InputCommon {
namespace Polling {
class DevicePoller;
enum class DeviceType;
} // namespace Polling
} // namespace InputCommon

namespace InputCommon {
namespace SDL {

/**
 * Stores SDL Controller state as it gets added from the event loop
 */
class SDLDevice {
public:
    SDLDevice() = default;
    ~SDLDevice() = default;

    u8 GetHat(u8 which) {
        std::lock_guard<std::mutex> lock(lock);
        if (!connected) {
            return 0;
        }
        return hat[which];
    }

    void SetHat(u8 which, u8 val) {
        std::lock_guard<std::mutex> lock(lock);
        hat[which] = val;
    }

    u8 GetButton(u8 which) {
        std::lock_guard<std::mutex> lock(lock);
        if (!connected) {
            return 0;
        }
        return button[which];
    }

    void SetButton(u8 which, u8 val) {
        std::lock_guard<std::mutex> lock(lock);
        button[which] = val;
    }

    s16 GetAxis(u8 which) {
        std::lock_guard<std::mutex> lock(lock);
        if (!connected) {

            return 0;
        }
        return axis[which];
    }

    void SetAxis(u8 which, s16 val) {
        std::lock_guard<std::mutex> lock(lock);
        axis[which] = val;
    }

    bool IsConnected() {
        std::lock_guard<std::mutex> lock(lock);
        return connected;
    }

    void SetConnected(bool connect) {
        std::lock_guard<std::mutex> lock(lock);
        connected = connect;
    }

private:
    std::unordered_map<u8, u8> hat;
    std::unordered_map<u8, bool> button;
    std::unordered_map<u8, s16> axis;
    std::mutex lock;
    bool connected;
};

class State;

void PollingThread(std::atomic_bool& running);

/// Initializes and registers SDL device factories
/// poll_events - Pass in true to spawn a thread to pump events. This should be set to false if
/// the calling thread already has a SDL event loop
std::unique_ptr<State> Init();

/// Unresisters SDL device factories and shut them down.
/// @param flags Which SDL subsystems were started in Init that need to be shutdown
void Shutdown(u32 flags, State::JoystickMap* joystick_map);

/// Creates a ParamPackage from an SDL_Event that can directly be used to create a ButtonDevice
Common::ParamPackage SDLEventToButtonParamPackage(const SDL_Event& event);

class State {

public:
    State(u32 flags, bool poll_events) : flags(flags), running(true), joystick_map() {
        if (poll_events) {
            event_polling_thread =
                std::async(std::launch::async, [& running = running] { PollingThread(running); });
        }
    }

    ~State() {
        if (running) {
            Input::UnregisterFactory<Input::ButtonDevice>("sdl");
            Input::UnregisterFactory<Input::AnalogDevice>("sdl");
            running = false;
        }
    }

    u32 GetFlags() const {
        return flags;
    }

    // SDL Joystick Instance ID -> Internal device to update and read values from
    using JoystickMap = std::unordered_map<s32, SDLDevice>;

    JoystickMap* GetJoystickMap() {
        return &joystick_map;
    }

private:
    u32 flags;
    std::atomic_bool running = false;
    std::future<void> event_polling_thread;
    JoystickMap joystick_map;
};

namespace Polling {

/// Get all DevicePoller that use the SDL backend for a specific device type
std::vector<std::unique_ptr<InputCommon::Polling::DevicePoller>> GetPollers(
    InputCommon::Polling::DeviceType type);

} // namespace Polling
} // namespace SDL
} // namespace InputCommon