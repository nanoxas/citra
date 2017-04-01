
#include <QPainter>

#include "citra_qt/bootmanager.h"
#include "citra_qt/overlay.h"
#include "citra_qt/stylus.h"

Overlay::Overlay(GRenderWindow* parent) : QWidget(parent), parent(parent) {
    // setWindowFlags(Qt::Widget | Qt::FramelessWindowHint);
    // // setParent(0); // Create TopLevel-Widget
    // setAttribute(Qt::WA_NoSystemBackground, true);
    // setAttribute(Qt::WA_TranslucentBackground, true);
    // setWindowFlags(Qt::FramelessWindowHint | Qt::ToolTip | Qt::WindowStaysOnTopHint);
    setWindowFlags(Qt::FramelessWindowHint | Qt::ToolTip);
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAttribute(Qt::WA_NoSystemBackground);
    setAttribute(Qt::WA_TranslucentBackground);
    setFocusPolicy(Qt::NoFocus);
    setMouseTracking(false);
    move(0, 0);
}

void Overlay::keyPressEvent(QKeyEvent* event) {
    parent->keyPressEvent(event);
}

void Overlay::keyReleaseEvent(QKeyEvent* event) {
    parent->keyReleaseEvent(event);
}

void Overlay::mousePressEvent(QMouseEvent* event) {
    parent->mousePressEvent(event);
    QApplication::setActiveWindow(window());
}
void Overlay::mouseMoveEvent(QMouseEvent* event) {
    parent->mouseMoveEvent(event);
    // window()->setFocus();
    QApplication::setActiveWindow(window());
}
void Overlay::mouseDoubleClickEvent(QMouseEvent* event) {
    parent->mouseDoubleClickEvent(event);
    // window()->setFocus();
    QApplication::setActiveWindow(window());
}
void Overlay::mouseReleaseEvent(QMouseEvent* event) {
    parent->mouseReleaseEvent(event);
    // window()->setFocus();
    QApplication::setActiveWindow(window());
}

void Overlay::paintEvent(QPaintEvent* ev) {
    Stylus* stylus = parent->GetStylus();
    QPainter painter(this);
    // set the viewport to match the screen size
    // painter.setWindow(QApplication::desktop()->screenGeometry());
    painter.setRenderHint(QPainter::Antialiasing, true);
    // painter.setPen(palette().dark().color());
    // painter.setBrush(Qt::NoBrush);
    // painter.drawRect(QRect(0, 0, width() - 1, height() - 1));
    // QPoint p = stylus->GetTouchPoint();
    // painter.drawEllipse(QRect(p.x() - 5, p.y() - 5, 10, 10));
    QTransform t = stylus->GetTransform();
    painter.setTransform(t);
    painter.drawPixmap(0, 0, stylus->GetPix());
    // painter.drawRect(0, 0, stylus->GetRect().width(), stylus->GetRect().height());
}