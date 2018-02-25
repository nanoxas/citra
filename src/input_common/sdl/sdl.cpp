// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <future>
#include <mutex>
#include <string>
#include <thread>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <SDL.h>
#include "common/logging/log.h"
#include "common/math_util.h"
#include "common/param_package.h"
#include "core/hle/service/hid/hid.h"
#include "input_common/main.h"
#include "input_common/sdl/sdl.h"

namespace InputCommon {

namespace SDL {

class SDLJoystick;
class SDLButtonFactory;
class SDLAnalogFactory;
// static std::unordered_map<, std::weak_ptr<SDLJoystick>> joystick_list;

// Maps controller GUIDs to the first seen controller for this. When a controller is added, it will
// be opened and added to this list. If a SDLJoystick does not have a valid instance id, it will try
// to read from this map until it gets a valid instance id
// static std::unordered_map<SDL_JoystickGUID, SDL_JoystickID> joystick_map;
// static std::mutex joystick_map_lock;
// static std::atomic_bool running = false;
// static std::future<void> event_polling_thread;
// static u32 flags;

constexpr s32 INVALID_DEVICE_ID = 0;
static std::weak_ptr<State::JoystickMap> joystick_map;
// static std::weak_ptr<SDL_Event> last_event;

// Find the first port that has a controller with the expected guid
static SDL_JoystickID GetJoystick(const std::string& guid) {
    if (auto map_ptr = joystick_map.lock()) {
        auto map = *map_ptr;
        auto id = SDL_JoystickGetGUIDFromString(guid.data());
        for (int i = 0; i < SDL_NumJoysticks(); ++i) {
            if (std::memcmp(&SDL_JoystickGetDeviceGUID(i), &id, sizeof(SDL_JoystickGUID)) == 0) {
                // if we find a match see if we've previously opened the controller
                if (map[guid] == INVALID_DEVICE_ID) {
                    // The device is newly connected, so open a controller for it
                    auto joystick = SDL_JoystickOpen(i);
                    map[guid] = SDL_JoystickInstanceID(joystick);
                }
                return map[guid];
            }
        }
    }
    return INVALID_DEVICE_ID;
}

static int EventProcessor(void* userdata, SDL_Event* event) {
    // if (userdata == nullptr) {
    //     return 0;
    // }
    // auto joystick_map = *reinterpret_cast<State::JoystickMap*>(userdata);
    // s32 which = event->cdevice.which;
    // auto& device = joystick_map[which];
    switch (event->type) {
    case SDL_JOYDEVICEADDED:
        LOG_CRITICAL(Input, "Controller added!");
        SDL_JoystickOpen(event->jdevice.which);
        Service::HID::ReloadInputDevices();
        // auto joystick = SDL_JoystickOpen(which);
        // joystick_map[which].SetConnected(true);
        break;
    case SDL_JOYDEVICEREMOVED:
        LOG_CRITICAL(Input, "Controller removed!");
        SDL_JoystickClose(SDL_JoystickFromInstanceID(event->jdevice.which));
        Service::HID::ReloadInputDevices();
        // SDL_JoystickClose(SDL_JoystickFromInstanceID(which));
        // joystick_map[which].SetConnected(false);
        break;
    // case SDL_JOYHATMOTION:
    //     LOG_CRITICAL(Input, "Hat moved! %d %d", event->jhat.hat, event->jhat.value);
    //     device.SetHat(event->jhat.hat, event->jhat.value);
    //     break;
    // case SDL_JOYBUTTONUP:
    // case SDL_JOYBUTTONDOWN:
    //     LOG_CRITICAL(Input, "Button moved! %d %d", event->jbutton.button,
    //                  event->jbutton.state == SDL_PRESSED);
    //     device.SetButton(event->jbutton.button, event->jbutton.state == SDL_PRESSED);
    //     break;
    // case SDL_JOYAXISMOTION:
    //     LOG_CRITICAL(Input, "Axis moved! %d %d", event->jaxis.axis, event->jaxis.value);
    //     device.SetAxis(event->jaxis.axis, event->jaxis.value);
    //     break;
    default:
        break;
    }
    // return value is ignored for watch filters
    return 0;
}

class SDLJoystick {
public:
    explicit SDLJoystick(SDL_JoystickID joystick_id) : joystick_id(joystick_id) {
        // if (auto joystick = SDL_JoystickOpen(joystick_index)) {
        //     joystick_id = SDL_JoystickInstanceID(joystick);
        // } else {
        //     LOG_ERROR(Input, "failed to open joystick %d", joystick_index);
        // joystick_id = static_cast<SDL_JoystickID>(-1);
        // }
    }

    ~SDLJoystick() {
        // if (auto joystick = SDL_JoystickFromInstanceID(joystick_id)) {
        //     SDL_JoystickClose(joystick);
        // }
    }

    bool GetButton(int button) const {
        // SDL_JoystickUpdate();
        // LOG_ERROR(Input, "Joystick? %d", joystick_id);
        // if (auto joystick = SDL_JoystickFromInstanceID(joystick_id)) {
        return SDL_JoystickGetButton(GetJoystick(), button) == 1;
        // }
        // LOG_CRITICAL(Input, "Joystick gone %d", joystick_id);
        // return {};
    }

    float GetAxis(int axis) const {
        return SDL_JoystickGetAxis(GetJoystick(), axis) / 32767.0f;
    }

    std::tuple<float, float> GetAnalog(int axis_x, int axis_y) const {
        float x = GetAxis(axis_x);
        float y = GetAxis(axis_y);
        y = -y; // 3DS uses an y-axis inverse from SDL

        // Make sure the coordinates are in the unit circle,
        // otherwise normalize it.
        float r = x * x + y * y;
        if (r > 1.0f) {
            r = std::sqrt(r);
            x /= r;
            y /= r;
        }

        return std::make_tuple(x, y);
    }

    bool GetHatDirection(int hat, Uint8 direction) const {
        return (SDL_JoystickGetHat(GetJoystick(), hat) & direction) != 0;
    }

private:
    /// Helper method that will check to see if a controller with this guid has been plugged in.
    /// If it has, then update the joystick_id and use that
    SDL_Joystick* GetJoystick() const {
        // Attempt to get a valid instance id from the map if this device was created before the
        // controller was plugged in

        // TODO get rid of this nonsense and just call HID::ReloadInputDevices(); when controllers
        // are added or removed
        // if (joystick_id == -1) {
        //     joystick_id = GetJoystickByGUID(joystick_guid);
        // }
        return SDL_JoystickFromInstanceID(joystick_id);
    }

    // SDL_JoystickGUID joystick_guid;
    SDL_JoystickID joystick_id;
};

class SDLButton final : public Input::ButtonDevice {
public:
    explicit SDLButton(SDL_JoystickID joystick_id, int button_)
        : joystick(joystick_id), button(button_) {}

    bool GetStatus() const override {
        return joystick.GetButton(button);
    }

private:
    SDLJoystick joystick;
    int button;
};

class SDLDirectionButton final : public Input::ButtonDevice {
public:
    explicit SDLDirectionButton(SDL_JoystickID joystick_id, int hat_, Uint8 direction_)
        : joystick(joystick_id), hat(hat_), direction(direction_) {}

    bool GetStatus() const override {
        return joystick.GetHatDirection(hat, direction);
    }

private:
    SDLJoystick joystick;
    int hat;
    Uint8 direction;
};

class SDLAxisButton final : public Input::ButtonDevice {
public:
    explicit SDLAxisButton(SDL_JoystickID joystick_id, int axis_, float threshold_,
                           bool trigger_if_greater_)
        : joystick(joystick_id), axis(axis_), threshold(threshold_),
          trigger_if_greater(trigger_if_greater_) {}

    bool GetStatus() const override {
        float axis_value = joystick.GetAxis(axis);
        if (trigger_if_greater)
            return axis_value > threshold;
        return axis_value < threshold;
    }

private:
    SDLJoystick joystick;
    int axis;
    float threshold;
    bool trigger_if_greater;
};

class SDLAnalog final : public Input::AnalogDevice {
public:
    SDLAnalog(SDL_JoystickID joystick_id, int axis_x_, int axis_y_)
        : joystick(joystick_id), axis_x(axis_x_), axis_y(axis_y_) {}

    std::tuple<float, float> GetStatus() const override {
        return joystick.GetAnalog(axis_x, axis_y);
    }

private:
    SDLJoystick joystick;
    int axis_x;
    int axis_y;
};

// static bool CompareStringToGUID(const std::string& str, const SDL_JoystickGUID& guid) {
//     std::array<char, 33> buffer;
// }

// static SDL_JoystickID GetJoystick(const std::string& joystick_guid) {
//     auto it = joystick_list.find(joystick_guid);
//     if (it != joystick_list.end()) {
//         auto joystick = it->second;
//         if (SDL_JoystickGetAttached(joystick) == SDL_TRUE) {
//             return SDL_JoystickInstanceID(joystick);
//         }
//     }
//     // SDL_Joystick* joystick = joystick_list[joystick_guid];
//     // The old joystick is no longer connected (or never was), so check to see if there is a
//     device
//     // that matches the guid. if we find one

//     for (int i = 0; i < SDL_NumJoysticks(); ++i) {
//         if (CompareStringToGUID(joystick_guid, SDL_JoystickGetDeviceGUID(i)) {
//         }
//     }
//     // When a device is disconnected and reconnected, they maintain the same instance id
//     // so using this to look up a joystick in the InputDevices is sufficient
//     return SDL_JoystickInstanceID(joystick);
// }

/// A button device factory that creates button devices from SDL joystick
class SDLButtonFactory final : public Input::Factory<Input::ButtonDevice> {
public:
    /**
     * Creates a button device from a joystick button
     * @param params contains parameters for creating the device:
     *     - "joystick": the index of the joystick to bind
     *     - "button"(optional): the index of the button to bind
     *     - "hat"(optional): the index of the hat to bind as direction buttons
     *     - "axis"(optional): the index of the axis to bind
     *     - "direction"(only used for hat): the direction name of the hat to bind. Can be "up",
     *         "down", "left" or "right"
     *     - "threshold"(only used for axis): a float value in (-1.0, 1.0) which the button is
     *         triggered if the axis value crosses
     *     - "direction"(only used for axis): "+" means the button is triggered when the axis
     * value is greater than the threshold; "-" means the button is triggered when the axis
     * value is smaller than the threshold
     */
    std::unique_ptr<Input::ButtonDevice> Create(const Common::ParamPackage& params) override {
        const std::string joystick_index = params.Get("joystick", "");

        if (params.Has("hat")) {
            const int hat = params.Get("hat", 0);
            const std::string direction_name = params.Get("direction", "");
            Uint8 direction;
            if (direction_name == "up") {
                direction = SDL_HAT_UP;
            } else if (direction_name == "down") {
                direction = SDL_HAT_DOWN;
            } else if (direction_name == "left") {
                direction = SDL_HAT_LEFT;
            } else if (direction_name == "right") {
                direction = SDL_HAT_RIGHT;
            } else {
                direction = 0;
            }
            return std::make_unique<SDLDirectionButton>(GetJoystick(joystick_index), hat,
                                                        direction);
        }

        if (params.Has("axis")) {
            const int axis = params.Get("axis", 0);
            const float threshold = params.Get("threshold", 0.5f);
            const std::string direction_name = params.Get("direction", "");
            bool trigger_if_greater;
            if (direction_name == "+") {
                trigger_if_greater = true;
            } else if (direction_name == "-") {
                trigger_if_greater = false;
            } else {
                trigger_if_greater = true;
                LOG_ERROR(Input, "Unknown direction %s", direction_name.c_str());
            }
            return std::make_unique<SDLAxisButton>(GetJoystick(joystick_index), axis, threshold,
                                                   trigger_if_greater);
        }

        const int button = params.Get("button", 0);
        return std::make_unique<SDLButton>(GetJoystick(joystick_index), button);
    }
};

/// An analog device factory that creates analog devices from SDL joystick
class SDLAnalogFactory final : public Input::Factory<Input::AnalogDevice> {
public:
    /**
     * Creates analog device from joystick axes
     * @param params contains parameters for creating the device:
     *     - "joystick": the index of the joystick to bind
     *     - "axis_x": the index of the axis to be bind as x-axis
     *     - "axis_y": the index of the axis to be bind as y-axis
     */
    std::unique_ptr<Input::AnalogDevice> Create(const Common::ParamPackage& params) override {
        const std::string joystick_index = params.Get("joystick", "");
        const int axis_x = params.Get("axis_x", 0);
        const int axis_y = params.Get("axis_y", 1);
        return std::make_unique<SDLAnalog>(GetJoystick(joystick_index), axis_x, axis_y);
    }
};

// static int ReconnectJoystick(void* userdata, SDL_Event* event) {
//     LOG_WARNING(Input, "EVENET??? ");
//     switch (event->type) {
//     case SDL_JOYDEVICEADDED:
//     case SDL_JOYDEVICEREMOVED:
//         // case SDL_CONTROLLERDEVICEADDED:
//         {
//             LOG_CRITICAL(Input, "Controller added!");
//             Service::HID::ReloadInputDevices();
//             // SDL_JoystickOpen(SDL_JoystickFromInstanceID(event->cdevice.which));
//             break;
//         }
//         // case SDL_CONTROLLERDEVICEREMOVED:
//         //{
//         //    LOG_CRITICAL(Input, "Controller removed!");
//         //   SDL_JoystickClose(SDL_JoystickFromInstanceID(event->cdevice.which));
//         //   break;
//         // }
//     default:
//         break;
//     }
//     // return value is ignored for watch filters
//     return 0;
// }

void State::PollingThread() {
    while (running) {
        SDL_Event dummy;
        while (SDL_WaitEventTimeout(&dummy, 500))
            // LOG_INFO(Input, "Polling got an event")
            ;
    }
}

/**
 * This function converts a joystick ID used in SDL events to the device index. This is
 * necessary because Citra opens joysticks using their indices, not their IDs.
 */
static std::string JoystickIDToGUID(SDL_JoystickID id) {
    // int num_joysticks =;
    auto joystick = SDL_JoystickFromInstanceID(id);
    if (joystick == nullptr) {
        LOG_ERROR(Input, "Could not get an open joystick for event");
        return "";
    }
    std::array<char, 33> guid;
    SDL_JoystickGetGUIDString(SDL_JoystickGetGUID(joystick), guid.data(), guid.size());
    return std::string(guid.data(), guid.size());
    // for (int i = 0; i < SDL_NumJoysticks(); i++) {
    //     // auto joystick = GetJoystick(i);

    //     if (std::memcmp(&SDL_JoystickGetDeviceGUID(i), &id, sizeof(SDL_JoystickGUID)) == 0) {
    //         return i;
    //     }
    // }
    // return -1;
}

std::unique_ptr<State> Init() {
    // If the caller has already inited SDL events, then we don't need to start a thread
    bool poll_events = SDL_WasInit(SDL_INIT_EVENTS) == SDL_FALSE;
    u32 flags = SDL_INIT_JOYSTICK;
    flags |= poll_events ? SDL_INIT_EVENTS : 0;
    if (SDL_Init(flags) < 0) {
        LOG_CRITICAL(Input, "SDL_Init(%d) failed with: %s", flags, SDL_GetError());
        return std::make_unique<State>(0, false);
    }

    // TODO: turn on the input factories
    // Input::RegisterFactory<Input::ButtonDevice>("sdl", std::make_shared<SDLButtonFactory>());
    // Input::RegisterFactory<Input::AnalogDevice>("sdl", std::make_shared<SDLAnalogFactory>());

    // add a event watcher to reload devices when one gets dis/connected
    SDL_AddEventWatch(EventProcessor, nullptr);
    auto state = std::make_unique<State>(flags, poll_events);
    joystick_map = state->GetJoystickMap();
    // last_event = state->GetLastEvent();
    return state;
}

void Shutdown(std::unique_ptr<State> state) {
    SDL_DelEventWatch(EventProcessor, nullptr);
    SDL_QuitSubSystem(state->GetFlags());
}

// void Init() {
//     // If the caller has already inited SDL events, then we don't need to start a thread
//     bool poll_events = SDL_WasInit(SDL_INIT_EVENTS) == SDL_FALSE;
//     flags = SDL_INIT_GAMECONTROLLER | SDL_INIT_JOYSTICK;
//     flags |= poll_events ? SDL_INIT_EVENTS : 0;
//     if (SDL_Init(flags) < 0) {
//         LOG_CRITICAL(Input, "SDL_Init(%d) failed with: %s", flags, SDL_GetError());
//         return;
//     }

//     // TODO: turn on the input factories
//     Input::RegisterFactory<Input::ButtonDevice>("sdl", std::make_shared<SDLButtonFactory>());
//     Input::RegisterFactory<Input::AnalogDevice>("sdl", std::make_shared<SDLAnalogFactory>());

//     // add a event watcher to open and close joysticks.
//     auto state = std::make_unique<State>(flags, poll_events);
//     SDL_AddEventWatch(EventProcessor, static_cast<void*>(state->GetJoystickMap()));

//     if (poll_events) {
//         event_polling_thread =
//             std::async(std::launch::async, [& running = running] { PollingThread(running); });
//     }
// }

// void Shutdown() {

//     if (running) {
//         Input::UnregisterFactory<Input::ButtonDevice>("sdl");
//         Input::UnregisterFactory<Input::AnalogDevice>("sdl");
//         running = false;
//         SDL_DelEventWatch(EventProcessor, static_cast<void*>(joystick_map));
//         SDL_QuitSubSystem(flags);
//     }
// }

Common::ParamPackage SDLEventToButtonParamPackage(const SDL_Event& event) {
    Common::ParamPackage params({{"engine", "sdl"}});
    switch (event.type) {
    case SDL_JOYAXISMOTION:
        params.Set("joystick", JoystickIDToGUID(event.jaxis.which));
        params.Set("axis", event.jaxis.axis);
        if (event.jaxis.value > 0) {
            params.Set("direction", "+");
            params.Set("threshold", "0.5");
        } else {
            params.Set("direction", "-");
            params.Set("threshold", "-0.5");
        }
        break;
    case SDL_JOYBUTTONUP:
        params.Set("joystick", JoystickIDToGUID(event.jbutton.which));
        params.Set("button", event.jbutton.button);
        break;
    case SDL_JOYHATMOTION:
        params.Set("joystick", JoystickIDToGUID(event.jhat.which));
        params.Set("hat", event.jhat.hat);
        switch (event.jhat.value) {
        case SDL_HAT_UP:
            params.Set("direction", "up");
            break;
        case SDL_HAT_DOWN:
            params.Set("direction", "down");
            break;
        case SDL_HAT_LEFT:
            params.Set("direction", "left");
            break;
        case SDL_HAT_RIGHT:
            params.Set("direction", "right");
            break;
        default:
            return {};
        }
        break;
    }
    return params;
}

namespace Polling {

static std::mutex last_event_mutex;
// TODO: add an initial state for the controller so we don't accidentally get values we don't want
static SDL_Event last_event;

static int PollingEventWatcher(void* userdata, SDL_Event* event) {
    switch (event->type) {
    case SDL_JOYAXISMOTION: {
        LOG_ERROR(Input, "event: axis motion on %d value %d", event->jaxis.axis,
                  event->jaxis.value);
        std::lock_guard<std::mutex> lock(last_event_mutex);
        std::memcpy(&last_event, event, sizeof(last_event));
        break;
    }
    case SDL_JOYBUTTONUP: {
        LOG_ERROR(Input, "event: button up on %d", event->jbutton.button);
        std::lock_guard<std::mutex> lock(last_event_mutex);
        std::memcpy(&last_event, event, sizeof(last_event));
        break;
    }
    case SDL_JOYHATMOTION: {
        LOG_ERROR(Input, "event: hat motion on %d value %d", event->jhat.hat, event->jhat.value);
        std::lock_guard<std::mutex> lock(last_event_mutex);
        std::memcpy(&last_event, event, sizeof(last_event));
        break;
    }
    }
    return 0;
}
// TODO: Register an event filter to watch for events
class SDLPoller : public InputCommon::Polling::DevicePoller {
public:
    SDLPoller() {
        for (int i = 0; i < SDL_NumJoysticks(); ++i) {
            auto joystick = SDL_JoystickInstanceID(SDL_JoystickOpen(i));
            joysticks_opened.emplace_back(joystick);
        }
    }

    ~SDLPoller() {
        for (auto joystick : joysticks_opened) {
            LOG_ERROR(Input, "closing: %d", joystick);
            SDL_JoystickClose(SDL_JoystickFromInstanceID(joystick));
        }
        // joysticks_opened.clear();
    }

    void Start() override {
        // SDL joysticks must be opened, otherwise they don't generate events
        // SDL_JoystickUpdate();
        // int num_joysticks = SDL_NumJoysticks();
        // open all joysticks to get events from all joysticks
        // // Empty event queue to get rid of old events. citra-qt doesn't use the queue
        // SDL_Event dummy;
        // while (SDL_PollEvent(&dummy)) {
        // }{

        SDL_AddEventWatch(PollingEventWatcher, nullptr);
    }

    void Stop() override {
        // {
        //     std::lock_guard<std::mutex> lock(last_event_mutex);
        //     LOG_ERROR(Input, "Deleting last event");
        //     //last_event = {};
        // }

        LOG_ERROR(Input, "deleting event");
        SDL_DelEventWatch(PollingEventWatcher, nullptr);
    }

private:
    // SDL_Event last_event;
    std::vector<SDL_JoystickID> joysticks_opened;
}; // namespace Polling

class SDLButtonPoller final : public SDLPoller {
public:
    SDLButtonPoller() = default;

    ~SDLButtonPoller() = default;

    Common::ParamPackage GetNextInput() override {
        // SDL_Event event;
        // while (SDL_PollEvent(&event)) {
        //     switch (event.type) {
        //     case SDL_JOYAXISMOTION:
        //         if (std::abs(event.jaxis.value / 32767.0) < 0.5) {
        //             break;
        //         }
        //     case SDL_JOYBUTTONUP:
        //     case SDL_JOYHATMOTION:
        //         return SDLEventToButtonParamPackage(event);
        //     }
        // }
        std::lock_guard<std::mutex> lock(last_event_mutex);
        if (last_event.type != 0) {
            LOG_ERROR(Input, "found event of type: %d", last_event.type);
            return SDLEventToButtonParamPackage(last_event);
        } else {
            return {};
        }
    }
};

class SDLAnalogPoller final : public SDLPoller {
public:
    SDLAnalogPoller() = default;

    ~SDLAnalogPoller() = default;

    // void Start() override {
    // SDLPoller::Start();

    // // Reset stored axes
    // analog_xaxis = -1;
    // analog_yaxis = -1;
    // analog_axes_joystick = -1;
    //}

    Common::ParamPackage GetNextInput() override {
        // SDL_Event event;
        // while (SDL_PollEvent(&event)) {
        //     if (event.type != SDL_JOYAXISMOTION || std::abs(event.jaxis.value / 32767.0) <
        //     0.5) {
        //         continue;
        //     }
        //     // An analog device needs two axes, so we need to store the axis for later and
        //     wait
        //     // for a second SDL event. The axes also must be from the same joystick.
        //     int axis = event.jaxis.axis;
        //     if (analog_xaxis == -1) {
        //         analog_xaxis = axis;
        //         analog_axes_joystick = event.jaxis.which;
        //     } else if (analog_yaxis == -1 && analog_xaxis != axis &&
        //                analog_axes_joystick == event.jaxis.which) {
        //         analog_yaxis = axis;
        //     }
        // }
        // Common::ParamPackage params;
        // if (analog_xaxis != -1 && analog_yaxis != -1) {
        //     params.Set("engine", "sdl");
        //     params.Set("joystick", JoystickIDToDeviceIndex(analog_axes_joystick));
        //     params.Set("axis_x", analog_xaxis);
        //     params.Set("axis_y", analog_yaxis);
        //     analog_xaxis = -1;
        //     analog_yaxis = -1;
        //     analog_axes_joystick = -1;
        //     return params;
        // }
        // return params;

        std::lock_guard<std::mutex> lock(last_event_mutex);
        return SDLEventToButtonParamPackage(last_event);
    }

private:
    // int analog_xaxis = -1;
    // int analog_yaxis = -1;
    // SDL_JoystickID analog_axes_joystick = -1;
};

void AppendPollers(std::vector<std::unique_ptr<InputCommon::Polling::DevicePoller>>& pollers,
                   InputCommon::Polling::DeviceType type) {
    // std::vector<std::unique_ptr<InputCommon::Polling::DevicePoller>> pollers;
    switch (type) {
    case InputCommon::Polling::DeviceType::Analog:
        pollers.push_back(std::make_unique<SDLAnalogPoller>());
        break;
    case InputCommon::Polling::DeviceType::Button:
        pollers.push_back(std::make_unique<SDLButtonPoller>());
        break;
    }
    // return pollers;
}
} // namespace Polling
} // namespace SDL
} // namespace InputCommon
