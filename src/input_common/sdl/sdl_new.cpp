// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <SDL.h>
#include "input_common/sdl/sdl_new.h"

namespace InputCommon {
namespace SDL {

void PollingThread(std::atomic_bool& running) {
    while (running) {
        SDL_Event dummy;
        while (SDL_PollEvent(&dummy))
            ;
    }
}

static int EventProcessor(void* userdata, SDL_Event* event) {
    if (userdata == nullptr) {
        return 0;
    }
    auto joystick_map = *reinterpret_cast<State::JoystickMap*>(userdata);
    s32 which = event->cdevice.which;
    auto& device = joystick_map[which];
    switch (event->type) {
    case SDL_JOYDEVICEADDED:
        LOG_CRITICAL(Input, "Controller added!");
        auto joystick = SDL_JoystickOpen(which);
        // joystick_map[which].SetConnected(true);
        break;
    case SDL_JOYDEVICEREMOVED:
        LOG_CRITICAL(Input, "Controller removed!");
        SDL_JoystickClose(SDL_JoystickFromInstanceID(which));
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

std::unique_ptr<State> Init() {
    // If the caller has already inited SDL events, then we don't need to start a thread
    bool poll_events = SDL_WasInit(SDL_INIT_EVENTS) == SDL_FALSE;
    u32 flags = SDL_INIT_GAMECONTROLLER | SDL_INIT_JOYSTICK;
    flags |= poll_events ? SDL_INIT_EVENTS : 0;
    if (SDL_Init(flags) < 0) {
        LOG_CRITICAL(Input, "SDL_Init(%d) failed with: %s", flags, SDL_GetError());
        return std::make_unique<State>(0, false);
    }

    // TODO: turn on the input factories
    // Input::RegisterFactory<Input::ButtonDevice>("sdl", std::make_shared<SDLButtonFactory>());
    // Input::RegisterFactory<Input::AnalogDevice>("sdl", std::make_shared<SDLAnalogFactory>());

    // add a event watcher to open and close joysticks.
    auto state = std::make_unique<State>(flags, poll_events);
    SDL_AddEventWatch(EventProcessor, static_cast<void*>(state->GetJoystickMap()));

    return state;
}

void Shutdown(u32 flags, State::JoystickMap* joystick_map) {
    SDL_DelEventWatch(EventProcessor, static_cast<void*>(joystick_map));
    SDL_QuitSubSystem(flags);
}

} // namespace SDL
} // namespace InputCommon