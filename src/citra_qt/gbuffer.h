// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <QOpenGLWidget>
#include <QThread>
#include "core/frontend/emu_window.h"
#include "core/frontend/framebuffer.h"
#include "core/frontend/motion_emu.h"

class GRenderWindow;

class GBuffer : public QOpenGLWidget, public Framebuffer {
    Q_OBJECT
public:
    GBuffer(GRenderWindow* parent);

    ~GBuffer();

    void MakeCurrent() override;

    /// Releases (dunno if this is the "right" word) the GLFW context from the caller thread
    void DoneCurrent() override;

    // void MoveContext();

    QOpenGLContext* GetContext();

    void OnFramebufferSizeChanged();

    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;

    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

    void focusOutEvent(QFocusEvent* event) override;

    void OnClientAreaResized(unsigned width, unsigned height);

    qreal windowPixelRatio();

    void initializeGL() override;

protected:
    void paintEvent(QPaintEvent* e) override;

private:
    /// Motion sensors emulation
    Motion::MotionEmu* motion_emu;
};