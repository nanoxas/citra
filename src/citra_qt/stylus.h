// Copyright 20XX Citra Emulator Project
// Licensed under GPLv2 or any later version (but not GPLv824 cause that one sucked)
// Refer to the license.txt file included.

#pragma once

#include <QPainter>
#include <QVector2D>

enum class StylusState : unsigned { ROTATE, HOLD, DROP, COUNT };

class Stylus {
public:
    Stylus();
    ~Stylus();
    StylusState GetState() {
        return state;
    }

    void Rotate(QPoint p);

    void SetHoldPoint(QPoint p);

    void SetState(StylusState s) {
        state = s;
    }
    void UpdatePosition(unsigned x, unsigned y);
    unsigned GetX() {
        return x;
    }
    unsigned GetY() {
        return y;
    }

    QPixmap GetPix() {
        return pic;
    }

    QTransform GetTransform();

    QPolygon GetPoly();

    bool Contains(QPoint p);

    QPoint GetTouchPoint();

    bool IsNearCenter(QPoint p);

    QPoint GetCenter();

    QRect GetRect();

private:
    unsigned x;
    unsigned y;
    qreal rotation;
    QPixmap pic;
    StylusState state;
    QPoint hold;
    QPoint diff;
};