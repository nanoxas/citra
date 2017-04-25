#include <QApplication>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QOpenGLContext>
#include <QScreen>
#include <QWindow>

#include "citra_qt/bootmanager.h"
#include "citra_qt/gbuffer.h"
#include "citra_qt/ui_settings.h"
#include "common/microprofile.h"
#include "common/scm_rev.h"
#include "common/string_util.h"
#include "core/core.h"
#include "core/frontend/motion_emu.h"
#include "core/settings.h"
#include "input_common/keyboard.h"
#include "input_common/main.h"
#include "video_core/debug_utils/debug_utils.h"
#include "video_core/video_core.h"

EmuThread::EmuThread(GRenderWindow* render_window)
    : exec_step(false), running(false), stop_run(false), render_window(render_window) {}

void EmuThread::run() {
    render_window->GetDefaultScreen()->MakeCurrent();

    MicroProfileOnThreadCreate("EmuThread");

    stop_run = false;

    // holds whether the cpu was running during the last iteration,
    // so that the DebugModeLeft signal can be emitted before the
    // next execution step
    bool was_active = false;
    while (!stop_run) {
        if (running) {
            if (!was_active)
                emit DebugModeLeft();

            Core::System::GetInstance().RunLoop();

            was_active = running || exec_step;
            if (!was_active && !stop_run)
                emit DebugModeEntered();
        } else if (exec_step) {
            if (!was_active)
                emit DebugModeLeft();

            exec_step = false;
            Core::System::GetInstance().SingleStep();
            emit DebugModeEntered();
            yieldCurrentThread();

            was_active = false;
        } else {
            std::unique_lock<std::mutex> lock(running_mutex);
            running_cv.wait(lock, [this] { return IsRunning() || exec_step || stop_run; });
        }
    }

    // Shutdown the core emulation
    Core::System::GetInstance().Shutdown();

#if MICROPROFILE_ENABLED
    MicroProfileOnThreadExit();
#endif

    // render_window->moveContext();
}

GRenderWindow::GRenderWindow(QWidget* parent, EmuThread* emu_thread)
    : QWidget(parent), emu_thread(emu_thread) {

    std::string window_title = Common::StringFromFormat("Citra %s| %s-%s", Common::g_build_name,
                                                        Common::g_scm_branch, Common::g_scm_desc);
    setWindowTitle(QString::fromStdString(window_title));

    UpdateScreensFromSettings();

    InputCommon::Init();
}

GRenderWindow::~GRenderWindow() {
    InputCommon::Shutdown();
}

void GRenderWindow::SwapBuffers() {
    render_context->moveToThread(qApp->thread());
    qApp->processEvents();
    frame_finished = false;
    // std::unique_lock<std::mutex> lock(frame_drawing_mutex);
    // frame_drawing_cv.wait(lock, [this] { return frame_finished == true; });
}

void GRenderWindow::FrameFinished() {
    MoveContext();
    frame_finished = true;
}

void GRenderWindow::PollEvents() {}

void GRenderWindow::BackupGeometry() {
    geometry = QWidget::saveGeometry();
}

void GRenderWindow::RestoreGeometry() {
    // We don't want to back up the geometry here (obviously)
    QWidget::restoreGeometry(geometry);
}

void GRenderWindow::restoreGeometry(const QByteArray& geometry) {
    // Make sure users of this class don't need to deal with backing up the geometry themselves
    QWidget::restoreGeometry(geometry);
    BackupGeometry();
}

void GRenderWindow::MoveContext() {
    GetDefaultScreen()->DoneCurrent();
    // If the thread started running, move the GL Context to the new thread. Otherwise, move it
    // back.
    auto thread = (QThread::currentThread() == qApp->thread() && emu_thread != nullptr)
                      ? emu_thread
                      : qApp->thread();
    GBuffer* screen = dynamic_cast<GBuffer*>(GetDefaultScreen());
    screen->GetContext()->moveToThread(thread);
}

void GRenderWindow::ShowFrames() {
    if (UISettings::values.single_window_mode) {
        QBoxLayout* layout = new QHBoxLayout(this);
        // if single window mode
        for (auto& screen : screens) {
            auto s = std::dynamic_pointer_cast<GBuffer>(screen);
            s->initializeGL();
            layout->addWidget(s.get());
        }
        setLayout(layout);
    } else {
        for (auto& screen : screens) {
            auto s = std::dynamic_pointer_cast<GBuffer>(screen);

            s->initializeGL();
            s->show();
        }
    }
    show();
}

QByteArray GRenderWindow::saveGeometry() {
    // If we are a top-level widget, store the current geometry
    // otherwise, store the last backup
    if (parent() == nullptr)
        return QWidget::saveGeometry();
    else
        return geometry;
}

void GRenderWindow::closeEvent(QCloseEvent* event) {
    motion_emu = nullptr;
    emit Closed();
    QWidget::closeEvent(event);
}

void GRenderWindow::OnEmulationStarting(EmuThread* emu_thread) {
    motion_emu = std::make_unique<Motion::MotionEmu>(*this);
    this->emu_thread = emu_thread;
    // create child windows
    // todo add motion_emu to all children here because it was null when i set it...
    // QBoxLayout* layout = new QHBoxLayout(this);
    // resize(VideoCore::kScreenTopWidth,
    //       VideoCore::kScreenTopHeight + VideoCore::kScreenBottomHeight);
    // layout->setMargin(0);
    // setLayout(layout);
    for (auto& screen : screens) {
    }
    // grab the context from the main window
    auto screen = std::dynamic_pointer_cast<GBuffer>(screens[0]);
    render_context = screen->GetContext();
}

void GRenderWindow::UpdateScreensFromSettings() {
    for (const auto& screen : Settings::values.screens) {
        if (!screen.is_active)
            continue;
        std::shared_ptr<GBuffer> buffer;
        if (UISettings::values.single_window_mode) {
            buffer = std::make_shared<GBuffer>(this);
        } else {
            buffer = std::make_shared<GBuffer>(nullptr);
        }
        buffer->resize(screen.size_height, screen.size_height);
        // QScreen* monitor = qApp->screens().value(screen.monitor);
        // if (monitor == nullptr) {
        //    buffer->windowHandle()->setScreen(qApp->primaryScreen());
        // } else {
        //     buffer->windowHandle()->setScreen(monitor);
        // }
        buffer->move(screen.position_x, screen.position_y);
        buffer->ChangeFramebufferLayout(screen.layout_option, screen.swap_screen);
        buffer->NotifyClientAreaSizeChanged(screen.size_width, screen.size_height);
        // buffer->setParent(nullptr);
        // buffer->show();
        screens.push_back(buffer);
    }
}

Framebuffer* GRenderWindow::GetDefaultScreen() {
    return screens[0].get();
}

void GRenderWindow::OnEmulationStopping() {
    motion_emu = nullptr;
    emu_thread = nullptr;
}

void GRenderWindow::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    GBuffer* buffer = dynamic_cast<GBuffer*>(this->GetDefaultScreen());
    connect(buffer, &QOpenGLWidget::frameSwapped, this, &GRenderWindow::FrameFinished);
    // windowHandle() is not initialized until the Window is shown, so we connect it here.
    connect(this->windowHandle(), SIGNAL(screenChanged(QScreen*)), this,
            SLOT(OnFramebufferSizeChanged()), Qt::UniqueConnection);
}
