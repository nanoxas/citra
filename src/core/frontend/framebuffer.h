// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/frontend/framebuffer_layout.h"

class EmuWindow;
namespace Settings {
enum class LayoutOption;
}

/**
 * Abstraction for any compatible buffer to draw a screen. Touch input should be handled as part of
 * this class and passed to the EmuWindow where it will be read by the rest of the core.
 */
class Framebuffer {
public:
    /// Makes the graphics context current for the caller thread
    virtual void MakeCurrent() = 0;

    /// Releases (dunno if this is the "right" word) the GLFW context from the caller thread
    virtual void DoneCurrent() = 0;

    /**
     * Signal that a touch pressed event has occurred (e.g. mouse click pressed)
     * @param framebuffer_x Framebuffer x-coordinate that was pressed
     * @param framebuffer_y Framebuffer y-coordinate that was pressed
     */
    void TouchPressed(unsigned framebuffer_x, unsigned framebuffer_y);

    /// Signal that a touch released event has occurred (e.g. mouse click released)
    void TouchReleased();

    /**
     * Signal that a touch movement event has occurred (e.g. mouse was moved over the emu window)
     * @param framebuffer_x Framebuffer x-coordinate
     * @param framebuffer_y Framebuffer y-coordinate
     */
    void TouchMoved(unsigned framebuffer_x, unsigned framebuffer_y);

    /**
     * Gets the framebuffer layout (width, height, and screen regions)
     * @note This method is thread-safe
     */
    const Layout::FramebufferLayout& GetFramebufferLayout() const {
        return layout;
    }

    /**
     * Changes the framebuffer layout while keeping the previous width and height
     */
    void ChangeFramebufferLayout(Settings::LayoutOption option, bool swap_screen);

    EmuWindow* parent_window; ///< Reference used to update the emulation state for touch input

    /**
     * Update internal client area size with the given parameter.
     * @note Framebuffer implementations will usually use this in window resize event handlers.
     */
    void NotifyClientAreaSizeChanged(unsigned width, unsigned height);

protected:
    Framebuffer(EmuWindow* p) : parent_window(p) {}

    virtual ~Framebuffer() {}

private:
    void ResizeFramebufferLayout();

    unsigned client_area_width;  ///< Current client width, should be set by window impl.
    unsigned client_area_height; ///< Current client height, should be set by window impl.

    u16 touch_x;
    u16 touch_y;
    bool touch_pressed;
    Settings::LayoutOption layout_option;
    bool swap_screen;
    Layout::FramebufferLayout layout;
};