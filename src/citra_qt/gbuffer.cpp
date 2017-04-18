#include <QApplication>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QOpenGLContext>

#include <QScreen>
#include <QWindow>

#include "citra_qt/bootmanager.h"
#include "citra_qt/gbuffer.h"
#include "common/logging/log.h"
#include "input_common/keyboard.h"
#include "input_common/main.h"
#include "video_core/debug_utils/debug_utils.h"
#include "video_core/video_core.h"

GBuffer::GBuffer(GRenderWindow* parent) : QOpenGLWidget(parent), Framebuffer(parent) {}

GBuffer::~GBuffer() {}

qreal GBuffer::windowPixelRatio() {
    // windowHandle() might not be accessible until the window is displayed to screen.
    return windowHandle() ? windowHandle()->screen()->devicePixelRatio() : 1.0f;
}

void GBuffer::paintEvent(QPaintEvent* e) {}

// void GBuffer::MoveContext() {}

QOpenGLContext* GBuffer::GetContext() {
    return context();
}

void GBuffer::MakeCurrent() {
    makeCurrent();
}

void GBuffer::DoneCurrent() {
    doneCurrent();
}

// On Qt 5.0+, this correctly gets the size of the framebuffer (pixels).
//
// Older versions get the window size (density independent pixels),
// and hence, do not support DPI scaling ("retina" displays).
// The result will be a viewport that is smaller than the extent of the window.
void GBuffer::OnFramebufferSizeChanged() {
    // Screen changes potentially incur a change in screen DPI, hence we should update the
    // framebuffer size
    qreal pixelRatio = windowPixelRatio();
    unsigned width = QPaintDevice::width() * pixelRatio;
    unsigned height = QPaintDevice::height() * pixelRatio;
    // ScaleFramelayout(width, height);
    // NotifyClientAreaSizeChanged(std::make_pair(width, height));
}

void GBuffer::keyPressEvent(QKeyEvent* event) {
    InputCommon::GetKeyboard()->PressKey(event->key());
}

void GBuffer::keyReleaseEvent(QKeyEvent* event) {
    InputCommon::GetKeyboard()->ReleaseKey(event->key());
}

void GBuffer::mousePressEvent(QMouseEvent* event) {
    auto pos = event->pos();
    if (event->button() == Qt::LeftButton) {
        qreal pixelRatio = windowPixelRatio();
        this->TouchPressed(static_cast<unsigned>(pos.x() * pixelRatio),
                           static_cast<unsigned>(pos.y() * pixelRatio));
    } else if (event->button() == Qt::RightButton) {
        motion_emu->BeginTilt(pos.x(), pos.y());
    }
}

void GBuffer::mouseMoveEvent(QMouseEvent* event) {
    auto pos = event->pos();
    qreal pixelRatio = windowPixelRatio();
    this->TouchMoved(std::max(static_cast<unsigned>(pos.x() * pixelRatio), 0u),
                     std::max(static_cast<unsigned>(pos.y() * pixelRatio), 0u));
    motion_emu->Tilt(pos.x(), pos.y());
}

void GBuffer::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton)
        this->TouchReleased();
    else if (event->button() == Qt::RightButton)
        motion_emu->EndTilt();
}

void GBuffer::focusOutEvent(QFocusEvent* event) {
    QWidget::focusOutEvent(event);
    InputCommon::GetKeyboard()->ReleaseAllKeys();
}

void GBuffer::OnClientAreaResized(unsigned width, unsigned height) {
    NotifyClientAreaSizeChanged(width, height);
}

void GBuffer::initializeGL() {
    // if (child) {
    //    delete child;
    //}

    // if (layout()) {
    //    delete layout();
    //}

    // TODO: One of these flags might be interesting: WA_OpaquePaintEvent, WA_NoBackground,
    // WA_DontShowOnScreen, WA_DeleteOnClose

    // Requests a forward-compatible context, which is required to get a 3.2+ context on OS X
    // fmt.setOption(QGL::NoDeprecatedFunctions);

    // OnMinimalClientAreaChangeRequest(GetActiveConfig().min_client_area_size);
    // QOpenGLContext* test1 = context();
    // if (test1 == nullptr) {
    //    LOG_INFO(Frontend, "As suspected context is uninited.");
    //}
    QOpenGLWidget::initializeGL();
    OnFramebufferSizeChanged();
    // QOpenGLContext* test2 = QOpenGLContext::currentContext();
    // if (test2 == nullptr) {
    //    LOG_INFO(Frontend, "As unsuspected context is uninited.");
    //}
    // NotifyClientAreaSizeChanged(std::make_pair(child->width(), child->height()));

    // BackupGeometry();
}
//
// void GBuffer::OnMinimalClientAreaChangeRequest(const std::pair<unsigned, unsigned>& minimal_size)
// {
//    setMinimumSize(minimal_size.first, minimal_size.second);
//}