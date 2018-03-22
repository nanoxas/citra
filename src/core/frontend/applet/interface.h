// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <atomic>

namespace Frontend {

class AppletInterface {
public:
    virtual ~AppletInterface() = default;

    /**
     * On applet start, the applet specific configuration will be passed in along with the
     * framebuffer.
     */
    template <typename Config>
    virtual void Start(const Config&, /* framebuffer */) = 0;

    /**
     * Called on a fixed schedule to have the applet update any state such as the framebuffer.
     */
    virtual void Update() = 0;

    /**
     * Checked every update to see if the applet is still running. When the applet is done, the core
     * will call ReceiveData
     */
    virtual bool IsRunning() {
        return running;
    }

private:
    /**
     * Called after the applet has stopped running. Receives the data from the applet to pass to the
     * calling program
     */
    template <typename Data>
    virtual Data ReceiveData() = 0;

    // framebuffer;
    std::atomic_bool running = false;
};

} // namespace Frontend