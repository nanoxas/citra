// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <QThread>
#include <QWidget>
#include "common/thread.h"
#include "core/frontend/emu_window.h"

class QKeyEvent;
class QScreen;

class GMainWindow;
class GRenderWindow;

namespace Motion {
class MotionEmu;
}

class EmuThread : public QThread {
    Q_OBJECT

public:
    explicit EmuThread(GRenderWindow* render_window);

    /**
     * Start emulation (on new thread)
     * @warning Only call when not running!
     */
    void run() override;

    /**
     * Steps the emulation thread by a single CPU instruction (if the CPU is not already running)
     * @note This function is thread-safe
     */
    void ExecStep() {
        exec_step = true;
        running_cv.notify_all();
    }

    /**
     * Sets whether the emulation thread is running or not
     * @param running Boolean value, set the emulation thread to running if true
     * @note This function is thread-safe
     */
    void SetRunning(bool running) {
        std::unique_lock<std::mutex> lock(running_mutex);
        this->running = running;
        lock.unlock();
        running_cv.notify_all();
    }

    /**
     * Check if the emulation thread is running or not
     * @return True if the emulation thread is running, otherwise false
     * @note This function is thread-safe
     */
    bool IsRunning() {
        return running;
    }

    /**
     * Requests for the emulation thread to stop running
     */
    void RequestStop() {
        stop_run = true;
        SetRunning(false);
    };

private:
    bool exec_step;
    bool running;
    std::atomic<bool> stop_run;
    std::mutex running_mutex;
    std::condition_variable running_cv;

    GRenderWindow* render_window;

signals:
    /**
     * Emitted when the CPU has halted execution
     *
     * @warning When connecting to this signal from other threads, make sure to specify either
     * Qt::QueuedConnection (invoke slot within the destination object's message thread) or even
     * Qt::BlockingQueuedConnection (additionally block source thread until slot returns)
     */
    void DebugModeEntered();

    /**
     * Emitted right before the CPU continues execution
     *
     * @warning When connecting to this signal from other threads, make sure to specify either
     * Qt::QueuedConnection (invoke slot within the destination object's message thread) or even
     * Qt::BlockingQueuedConnection (additionally block source thread until slot returns)
     */
    void DebugModeLeft();
};

class GRenderWindow : public QWidget, public EmuWindow {
    Q_OBJECT

public:
    GRenderWindow(QWidget* parent, EmuThread* emu_thread);
    ~GRenderWindow();

    void BackupGeometry();
    void RestoreGeometry();
    void restoreGeometry(const QByteArray& geometry); // overridden
    QByteArray saveGeometry();                        // overridden

    void closeEvent(QCloseEvent* event) override;

    void ShowFrames();

    /**
     * Overriden to return the context to the gui thread breifly to draw the frame
     */
    void SwapBuffers();

    /// Polls window events aka do nothing
    void PollEvents();

    void MoveContext();

    Framebuffer* GetDefaultScreen();

public slots:
    void FrameFinished();

    void OnEmulationStarting(EmuThread* emu_thread);
    void OnEmulationStopping();

signals:
    /// Emitted when the window is closed
    void Closed();

protected:
    void showEvent(QShowEvent* event) override;

private:
    /// Create the screens from the values in the settings
    void UpdateScreensFromSettings();

    QByteArray geometry;

    EmuThread* emu_thread;
    /// Motion sensors emulation
    std::unique_ptr<Motion::MotionEmu> motion_emu;

    QOpenGLContext* render_context;

    std::atomic<bool> frame_finished;
    std::mutex frame_drawing_mutex;
    std::condition_variable frame_drawing_cv;
};
