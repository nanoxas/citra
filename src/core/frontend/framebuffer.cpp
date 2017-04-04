#include "common/logging/log.h"
#include "common/math_util.h"
#include "core/frontend/emu_window.h"
#include "core/frontend/framebuffer.h"
#include "core/settings.h"
#include "video_core/video_core.h"

/**
 * Check if the given x/y coordinates are within the touchpad specified by the framebuffer layout
 * @param layout FramebufferLayout object describing the framebuffer size and screen positions
 * @param framebuffer_x Framebuffer x-coordinate to check
 * @param framebuffer_y Framebuffer y-coordinate to check
 * @return True if the coordinates are within the touchpad, otherwise false
 */
static bool IsWithinTouchscreen(const Layout::FramebufferLayout& layout, unsigned framebuffer_x,
                                unsigned framebuffer_y) {
    return (
        framebuffer_y >= layout.bottom_screen.top && framebuffer_y < layout.bottom_screen.bottom &&
        framebuffer_x >= layout.bottom_screen.left && framebuffer_x < layout.bottom_screen.right);
}

std::tuple<unsigned, unsigned> ClipToTouchScreen(const Layout::FramebufferLayout& layout,
                                                 unsigned new_x, unsigned new_y) {
    new_x = std::max(new_x, layout.bottom_screen.left);
    new_x = std::min(new_x, layout.bottom_screen.right - 1);

    new_y = std::max(new_y, layout.bottom_screen.top);
    new_y = std::min(new_y, layout.bottom_screen.bottom - 1);

    return std::make_tuple(new_x, new_y);
}

void Framebuffer::TouchPressed(unsigned framebuffer_x, unsigned framebuffer_y) {
    if (!IsWithinTouchscreen(layout, framebuffer_x, framebuffer_y))
        return;

    touch_x = VideoCore::kScreenBottomWidth * (framebuffer_x - layout.bottom_screen.left) /
              (layout.bottom_screen.right - layout.bottom_screen.left);
    touch_y = VideoCore::kScreenBottomHeight * (framebuffer_y - layout.bottom_screen.top) /
              (layout.bottom_screen.bottom - layout.bottom_screen.top);
    touch_pressed = true;

    auto p = parent.lock();
    if (!p) {
        LOG_ERROR(Frontend, "EmuWindow Deleted before a Framebuffer!");
        return;
    }
    p->TouchPressed(touch_x, touch_y);
}

void Framebuffer::TouchReleased() {
    auto p = parent.lock();
    if (!p) {
        LOG_ERROR(Frontend, "EmuWindow Deleted before a Framebuffer!");
        return;
    }
    p->TouchReleased();
}

void Framebuffer::TouchMoved(unsigned framebuffer_x, unsigned framebuffer_y) {
    if (!touch_pressed)
        return;

    if (!IsWithinTouchscreen(layout, framebuffer_x, framebuffer_y))
        std::tie(framebuffer_x, framebuffer_y) =
            ClipToTouchScreen(layout, framebuffer_x, framebuffer_y);

    TouchPressed(framebuffer_x, framebuffer_y);
}

void Framebuffer::UpdateCurrentFramebufferLayout(Settings::LayoutOption option, unsigned width,
                                                 unsigned height) {
    Layout::FramebufferLayout layout;
    if (Settings::values.custom_layout == true) {
        layout = Layout::CustomFrameLayout(width, height);
    } else {
        switch (Settings::values.layout_option) {
        case Settings::LayoutOption::SingleScreen:
            layout = Layout::SingleFrameLayout(width, height, Settings::values.swap_screen);
            break;
        case Settings::LayoutOption::LargeScreen:
            layout = Layout::LargeFrameLayout(width, height, Settings::values.swap_screen);
            break;
        case Settings::LayoutOption::Custom:
            layout = Layout::CustomFrameLayout(width, height);
            break;
        case Settings::LayoutOption::Default:
        default:
            layout = Layout::DefaultFrameLayout(width, height, Settings::values.swap_screen);
            break;
        }
    }
    NotifyFramebufferLayoutChanged(layout);
}