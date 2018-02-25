// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <atomic>
#include <future>
#include <memory>
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

class State {
public:
    State(u32 flags, bool poll_events) : flags(flags), joystick_map() {
        if (poll_events) {
            running = true;
            event_polling_thread = std::async(std::launch::async, [&] { PollingThread(); });
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

    // Maps SDL GUID -> SDL Joystick Instance ID (or -1 if that controller isn't connected)
    using JoystickMap = std::unordered_map<std::string, s32>;

    std::shared_ptr<JoystickMap> GetJoystickMap() const {
        return joystick_map;
    }

    // std::shared_ptr<SDL_Event> GetLastEvent() const {
    //     return last_event;
    // }

private:
    void PollingThread();

    u32 flags;
    std::atomic_bool running = false;
    std::future<void> event_polling_thread;
    std::shared_ptr<JoystickMap> joystick_map;
    // std::shared_ptr<SDL_Event> last_event;
};

/// Initializes and registers SDL device factories
std::unique_ptr<State> Init();

/// Unresisters SDL device factories and shut them down.
void Shutdown(std::unique_ptr<State> state);

/// Creates a ParamPackage from an SDL_Event that can directly be used to create a ButtonDevice
Common::ParamPackage SDLEventToButtonParamPackage(const SDL_Event& event);

namespace Polling {

/// Adds DevicePollers to the list that use the SDL backend for a specific device type
void AppendPollers(std::vector<std::unique_ptr<InputCommon::Polling::DevicePoller>>&,
                   InputCommon::Polling::DeviceType type);

} // namespace Polling
} // namespace SDL
} // namespace InputCommon
