#include <QApplication>
#include <QtWidgets>

#include "citra_qt/stylus.h"
#include "common/logging/log.h"

Stylus::Stylus() {
    pic = QPixmap(":stylus.png");
    x = QApplication::desktop()->screenGeometry().width() / 2;
    y = QApplication::desktop()->screenGeometry().height() / 2;
    rotation = 0;
}

Stylus::~Stylus() {}

void Stylus::SetHoldPoint(QPoint p) {
    // hold = QPoint(p.x() - x, p.y() - y);
    diff = QPoint(p.x() - x, p.y() - y);
    hold = p;
}

void Stylus::Rotate(QPoint n) {
    QPoint center = GetCenter();
    // get the angle from the center to the initial click point
    qreal init_x = hold.x() - center.x();
    qreal init_y = hold.y() - center.y();
    qreal initial_angle = std::atan2(init_y, init_x);
    qreal x = n.x() - center.x();
    qreal y = n.y() - center.y();

    qreal mv_angle = std::atan2(y, x);

    // get the changed angle
    rotation = (mv_angle - initial_angle) * 180 / M_PI;

    if (std::fabs(rotation) > 360.0) {
        rotation = fmod(rotation, 360);
    }
}

bool Stylus::IsNearCenter(QPoint n) {
    // QPoint c = GetCenter();
    // return (n - c).manhattanLength() < 75;
    // jokes on you i'm now checking to see if its the top half or the bottom half
    QTransform t = GetTransform();
    QRect top = QRect(0, 0, pic.width(), pic.height() / 2);
    return t.mapRect(top).contains(n);
}

QPoint Stylus::GetCenter() {
    return QRect(x, y, pic.width(), pic.height()).center();
}

void Stylus::UpdatePosition(unsigned x, unsigned y) {
    this->x = x - diff.x();
    this->y = y - diff.y();
}

QTransform Stylus::GetTransform() {
    QTransform t;
    t.translate(x, y);
    t.rotate(rotation);
    return t;
}

QRect Stylus::GetRect() {
    return QRect(0, 0, pic.width(), pic.height());
}

QPolygon Stylus::GetPoly() {
    QTransform t = GetTransform();
    QRect r = GetRect();
    return t.mapToPolygon(r);
}

bool Stylus::Contains(QPoint pos) {
    QPolygon p = GetPoly();
    return p.containsPoint(pos, Qt::OddEvenFill);
}

QPoint Stylus::GetTouchPoint() {
    QTransform t = GetTransform();
    return t.map(QPoint(pic.width() / 2, pic.height() - 10));
}