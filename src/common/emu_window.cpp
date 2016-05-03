// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <cmath>

#include "common/assert.h"
#include "common/key_map.h"

#include "emu_window.h"
#include "video_core/video_core.h"

void EmuWindow::KeyPressed(KeyMap::HostDeviceKey key) {
    pad_state.hex |= KeyMap::GetPadKey(key).hex;
}

void EmuWindow::KeyReleased(KeyMap::HostDeviceKey key) {
    pad_state.hex &= ~KeyMap::GetPadKey(key).hex;
}

/**
 * Check if the given x/y coordinates are within the touchpad specified by the framebuffer layout
 * @param layout FramebufferLayout object describing the framebuffer size and screen positions
 * @param framebuffer_x Framebuffer x-coordinate to check
 * @param framebuffer_y Framebuffer y-coordinate to check
 * @return True if the coordinates are within the touchpad, otherwise false
 */
static bool IsWithinTouchscreen(const EmuWindow::FramebufferLayout& layout, unsigned framebuffer_x,
                                unsigned framebuffer_y) {
    return (framebuffer_y >= layout.bottom_screen.top    &&
            framebuffer_y <  layout.bottom_screen.bottom &&
            framebuffer_x >= layout.bottom_screen.left   &&
            framebuffer_x <  layout.bottom_screen.right);
}

std::tuple<unsigned,unsigned> EmuWindow::ClipToTouchScreen(unsigned new_x, unsigned new_y) {

    new_x = std::max(new_x, framebuffer_layout.bottom_screen.left);
    new_x = std::min(new_x, framebuffer_layout.bottom_screen.right-1);

    new_y = std::max(new_y, framebuffer_layout.bottom_screen.top);
    new_y = std::min(new_y, framebuffer_layout.bottom_screen.bottom-1);

    return std::make_tuple(new_x, new_y);
}

void EmuWindow::TouchPressed(unsigned framebuffer_x, unsigned framebuffer_y) {
    if (!IsWithinTouchscreen(framebuffer_layout, framebuffer_x, framebuffer_y))
        return;

    touch_x = VideoCore::kScreenBottomWidth * (framebuffer_x - framebuffer_layout.bottom_screen.left) /
        (framebuffer_layout.bottom_screen.right - framebuffer_layout.bottom_screen.left);
    touch_y = VideoCore::kScreenBottomHeight * (framebuffer_y - framebuffer_layout.bottom_screen.top) /
        (framebuffer_layout.bottom_screen.bottom - framebuffer_layout.bottom_screen.top);

    touch_pressed = true;
    pad_state.touch.Assign(1);
}

void EmuWindow::TouchReleased() {
    touch_pressed = false;
    touch_x = 0;
    touch_y = 0;
    pad_state.touch.Assign(0);
}

void EmuWindow::TouchMoved(unsigned framebuffer_x, unsigned framebuffer_y) {
    if (!touch_pressed)
        return;

    if (!IsWithinTouchscreen(framebuffer_layout, framebuffer_x, framebuffer_y))
        std::tie(framebuffer_x, framebuffer_y) = ClipToTouchScreen(framebuffer_x, framebuffer_y);

    TouchPressed(framebuffer_x, framebuffer_y);
}

EmuWindow::FramebufferLayout EmuWindow::FramebufferLayout::DefaultScreenLayout(unsigned width, unsigned height) {

    ASSERT(width > 0);
    ASSERT(height > 0);

    EmuWindow::FramebufferLayout res = { width, height, {}, {} };

    float window_aspect_ratio = static_cast<float>(height) / width;
    float emulation_aspect_ratio = static_cast<float>(VideoCore::kScreenTopHeight * 2) /
        VideoCore::kScreenTopWidth;

    if (window_aspect_ratio > emulation_aspect_ratio) {
        // Window is narrower than the emulation content => apply borders to the top and bottom
        int viewport_height = static_cast<int>(std::round(emulation_aspect_ratio * width));

        res.top_screen.left = 0;
        res.top_screen.right = res.top_screen.left + width;
        res.top_screen.top = (height - viewport_height) / 2;
        res.top_screen.bottom = res.top_screen.top + viewport_height / 2;

        int bottom_width = static_cast<int>((static_cast<float>(VideoCore::kScreenBottomWidth) /
            VideoCore::kScreenTopWidth) * (res.top_screen.right - res.top_screen.left));
        int bottom_border = ((res.top_screen.right - res.top_screen.left) - bottom_width) / 2;

        res.bottom_screen.left = bottom_border;
        res.bottom_screen.right = res.bottom_screen.left + bottom_width;
        res.bottom_screen.top = res.top_screen.bottom;
        res.bottom_screen.bottom = res.bottom_screen.top + viewport_height / 2;
    } else {
        // Otherwise, apply borders to the left and right sides of the window.
        int viewport_width = static_cast<int>(std::round(height / emulation_aspect_ratio));

        res.top_screen.left = (width - viewport_width) / 2;
        res.top_screen.right = res.top_screen.left + viewport_width;
        res.top_screen.top = 0;
        res.top_screen.bottom = res.top_screen.top + height / 2;

        int bottom_width = static_cast<int>((static_cast<float>(VideoCore::kScreenBottomWidth) /
            VideoCore::kScreenTopWidth) * (res.top_screen.right - res.top_screen.left));
        int bottom_border = ((res.top_screen.right - res.top_screen.left) - bottom_width) / 2;

        res.bottom_screen.left = res.top_screen.left + bottom_border;
        res.bottom_screen.right = res.bottom_screen.left + bottom_width;
        res.bottom_screen.top = res.top_screen.bottom;
        res.bottom_screen.bottom = res.bottom_screen.top + height / 2;
    }

    return res;
}

EmuWindow::FramebufferLayout EmuWindow::FramebufferLayout::TopOnlyLayout(unsigned width, unsigned height) {

    ASSERT(width > 0);
    ASSERT(height > 0);

    EmuWindow::FramebufferLayout res = { width, height, {}, {} };

    float window_aspect_ratio = static_cast<float>(height) / width;
    float emulation_aspect_ratio = static_cast<float>(VideoCore::kScreenTopHeight) /
        VideoCore::kScreenTopWidth;

    if (window_aspect_ratio > emulation_aspect_ratio) {
        // Window is narrower than the emulation content => apply borders to the top and bottom
        int viewport_height = static_cast<int>(std::round(emulation_aspect_ratio * width));

        res.top_screen.left = 0;
        res.top_screen.right = res.top_screen.left + width;
        res.top_screen.top = (height - viewport_height) / 2;
        res.top_screen.bottom = res.top_screen.top + viewport_height;

        res.bottom_screen.left = 0;
        res.bottom_screen.right = 0;
        res.bottom_screen.top = 0;
        res.bottom_screen.bottom = 0;
    }
    else {
        // Otherwise, apply borders to the left and right sides of the window.
        int viewport_width = static_cast<int>(std::round(height / emulation_aspect_ratio));

        res.top_screen.left = (width - viewport_width) / 2;
        res.top_screen.right = res.top_screen.left + viewport_width;
        res.top_screen.top = 0;
        res.top_screen.bottom = res.top_screen.top + height;

        res.bottom_screen.left = 0;
        res.bottom_screen.right = 0;
        res.bottom_screen.top = 0;
        res.bottom_screen.bottom = 0;
    }

    return res;
}

EmuWindow::FramebufferLayout EmuWindow::FramebufferLayout::BotOnlyLayout(unsigned width, unsigned height) {

    ASSERT(width > 0);
    ASSERT(height > 0);

    EmuWindow::FramebufferLayout res = { width, height, {}, {} };

    float window_aspect_ratio = static_cast<float>(height) / width;
    float emulation_aspect_ratio = static_cast<float>(VideoCore::kScreenBottomHeight) /
        VideoCore::kScreenBottomWidth;

    if (window_aspect_ratio > emulation_aspect_ratio) {
        // Window is narrower than the emulation content => apply borders to the top and bottom
        int viewport_height = static_cast<int>(std::round(emulation_aspect_ratio * width));

        res.bottom_screen.left = 0;
        res.bottom_screen.right = res.top_screen.left + width;
        res.bottom_screen.top = (height - viewport_height) / 2;
        res.bottom_screen.bottom = res.top_screen.top + viewport_height;

        res.top_screen.left = 0;
        res.top_screen.right = 0;
        res.top_screen.top = 0;
        res.top_screen.bottom = 0;
    }
    else {
        // Otherwise, apply borders to the left and right sides of the window.
        int viewport_width = static_cast<int>(std::round(height / emulation_aspect_ratio));

        res.bottom_screen.left = (width - viewport_width) / 2;
        res.bottom_screen.right = res.top_screen.left + viewport_width;
        res.bottom_screen.top = 0;
        res.bottom_screen.bottom = res.top_screen.top + height;

        res.top_screen.left = 0;
        res.top_screen.right = 0;
        res.top_screen.top = 0;
        res.top_screen.bottom = 0;
    }

    return res;
}

EmuWindow::FramebufferLayout EmuWindow::FramebufferLayout::BotFirstLayout(unsigned width, unsigned height) {

    ASSERT(width > 0);
    ASSERT(height > 0);

    EmuWindow::FramebufferLayout res = { width, height, {}, {} };

    float window_aspect_ratio = static_cast<float>(height) / width;
    float emulation_aspect_ratio = static_cast<float>(VideoCore::kScreenTopHeight * 2) /
        VideoCore::kScreenTopWidth;

    if (window_aspect_ratio > emulation_aspect_ratio) {
        // Window is narrower than the emulation content => apply borders to the top and bottom
        int viewport_height = static_cast<int>(std::round(emulation_aspect_ratio * width));
        int bottom_width = static_cast<int>((static_cast<float>(VideoCore::kScreenBottomWidth) /
            VideoCore::kScreenTopWidth) * (res.top_screen.right - res.top_screen.left));
        int bottom_border = ((res.top_screen.right - res.top_screen.left) - bottom_width) / 2;

        res.bottom_screen.left = bottom_border;
        res.bottom_screen.right = res.bottom_screen.left + bottom_width;
        res.bottom_screen.top = (height - viewport_height) / 2;
        res.bottom_screen.bottom = res.bottom_screen.top + viewport_height / 2;

        res.top_screen.left = 0;
        res.top_screen.right = res.top_screen.left + width;
        res.top_screen.top = res.top_screen.bottom;
        res.top_screen.bottom = res.top_screen.top + viewport_height / 2;
    }
    else {
        // Otherwise, apply borders to the left and right sides of the window.
        int viewport_width = static_cast<int>(std::round(height / emulation_aspect_ratio));
        int bottom_width = static_cast<int>((static_cast<float>(VideoCore::kScreenBottomWidth) /
            VideoCore::kScreenTopWidth) * (res.top_screen.right - res.top_screen.left));
        int bottom_border = ((res.top_screen.right - res.top_screen.left) - bottom_width) / 2;

        res.bottom_screen.left = res.top_screen.left + bottom_border;
        res.bottom_screen.right = res.bottom_screen.left + bottom_width;
        res.bottom_screen.top = 0;
        res.bottom_screen.bottom = res.bottom_screen.top + height / 2;

        res.top_screen.left = (width - viewport_width) / 2;
        res.top_screen.right = res.top_screen.left + viewport_width;
        res.top_screen.top = res.top_screen.bottom;
        res.top_screen.bottom = res.top_screen.top + height / 2;
    }

    return res;
}

Screen::Screen(EmuWindow* single) {
    is_split_screen = false;
    single_window = single;
}

Screen::Screen(EmuWindow* top, EmuWindow* bot) {
    is_split_screen = true;
    split_window_top = top;
    split_window_bot = bot;
}

EmuWindow* Screen::GetTopScreen() {
    return (is_split_screen) ? split_window_top : single_window;
}

EmuWindow* Screen::GetBotScreen() {
    return (is_split_screen) ? split_window_bot : single_window;
}

void Screen::SwapBuffers() {
    if (is_split_screen) {
        split_window_top->SwapBuffers();
        split_window_bot->SwapBuffers();
    } else {
        single_window->SwapBuffers();
    }
}

void Screen::PollEvents() {
    if (is_split_screen) {
        split_window_top->SwapBuffers();
        split_window_bot->SwapBuffers();
    } else {
        single_window->SwapBuffers();
    }
}

void Screen::MakeCurrent() {
    if (is_split_screen) {
        split_window_top->MakeCurrent();
        split_window_bot->MakeCurrent();
    } else {
        single_window->MakeCurrent();
    }
}

void Screen::DoneCurrent() {
    if (is_split_screen) {
        split_window_top->DoneCurrent();
        split_window_bot->DoneCurrent();
    } else {
        single_window->DoneCurrent();
    }
}

void Screen::ReloadSetKeymaps() {
    if (is_split_screen) {
        split_window_top->ReloadSetKeymaps();
        split_window_bot->ReloadSetKeymaps();
    } else {
        single_window->ReloadSetKeymaps();
    }
}

void Screen::KeyPressed(KeyMap::HostDeviceKey key){
    return (is_split_screen) ? split_window_bot->KeyPressed(key) : single_window->KeyPressed(key);
}
void Screen::KeyReleased(KeyMap::HostDeviceKey key){
    return (is_split_screen) ? split_window_bot->KeyReleased(key) : single_window->KeyReleased(key);
}
void Screen::TouchPressed(unsigned framebuffer_x, unsigned framebuffer_y){
    return (is_split_screen) ? split_window_bot->TouchPressed(framebuffer_x, framebuffer_y) : single_window->TouchPressed(framebuffer_x, framebuffer_y);
}
void Screen::TouchReleased(){
    return (is_split_screen) ? split_window_bot->TouchReleased() : single_window->TouchReleased();
}
void Screen::TouchMoved(unsigned framebuffer_x, unsigned framebuffer_y){
    return (is_split_screen) ? split_window_bot->TouchMoved(framebuffer_x, framebuffer_y) : single_window->TouchMoved(framebuffer_x, framebuffer_y);
}
Service::HID::PadState Screen::GetPadState(){
    return (is_split_screen) ? split_window_bot->GetPadState() : single_window->GetPadState();
}
std::tuple<u16, u16, bool> Screen::GetTouchState(){
    return (is_split_screen) ? split_window_bot->GetTouchState() : single_window->GetTouchState();
}
std::tuple<s16, s16, s16> Screen::GetAccelerometerState(){
    return (is_split_screen) ? split_window_bot->GetAccelerometerState() : single_window->GetAccelerometerState();
}
std::tuple<s16, s16, s16> Screen::GetGyroscopeState(){
    return (is_split_screen) ? split_window_bot->GetGyroscopeState() : single_window->GetGyroscopeState();
}
f32 Screen::GetGyroscopeRawToDpsCoefficient(){
    return (is_split_screen) ? split_window_bot->GetGyroscopeRawToDpsCoefficient() : single_window->GetGyroscopeRawToDpsCoefficient();
}