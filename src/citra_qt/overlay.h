// Copyright 20XX Citra Emulator Project
// Licensed under GPLv2 or any later version (but not GPLv824 cause that one sucked)
// Refer to the license.txt file included.

#pragma once

#include <QPainter>
#include <QtWidgets>

class Stylus;
class GRenderWindow;

class Overlay : public QWidget {
    Q_OBJECT
public:
    Overlay(GRenderWindow* parent);

protected:
    void paintEvent(QPaintEvent* ev) override;

    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;

    void keyPressEvent(QKeyEvent* event);
    void keyReleaseEvent(QKeyEvent* event);

private:
    GRenderWindow* parent;
    Stylus* stylus;
};