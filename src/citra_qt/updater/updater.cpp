// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.
//
// Based on the original work by Felix Barx
// Copyright (c) 2015, Felix Barz
// All rights reserved.

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QTimer>
#include <QXmlStreamReader>

#include "citra_qt/ui_settings.h"
#include "citra_qt/updater/admin_auth.h"
#include "citra_qt/updater/updater.h"
#include "citra_qt/updater/updater_p.h"
#include "common/logging/log.h"

#ifdef Q_OS_OSX
#define DEFAULT_TOOL_PATH QStringLiteral("../../../maintenancetool")
#else
#define DEFAULT_TOOL_PATH QStringLiteral("../maintenancetool")
#endif

Updater::Updater(QObject* parent) : Updater(DEFAULT_TOOL_PATH, parent) {}

Updater::Updater(const QString& maintenance_tool_path, QObject* parent)
    : QObject(parent), backend(new UpdaterPrivate(this)) {
    backend->tool_path = UpdaterPrivate::toSystemExe(maintenance_tool_path);
}

Updater::~Updater() {}

bool Updater::ExitedNormally() const {
    return backend->normal_exit;
}

int Updater::ErrorCode() const {
    return backend->last_error_code;
}

QByteArray Updater::ErrorLog() const {
    return backend->last_error_log;
}

bool Updater::IsRunning() const {
    return backend->running;
}

QList<Updater::UpdateInfo> Updater::LatestUpdateInfo() const {
    return backend->update_info;
}

bool Updater::CheckForUpdates() {
    return backend->StartUpdateCheck();
}

void Updater::AbortUpdateCheck(int max_delay, bool async) {
    backend->StopUpdateCheck(max_delay, async);
}

Updater::UpdateInfo::UpdateInfo() : name(), version(), size(0ull) {}

Updater::UpdateInfo::UpdateInfo(const Updater::UpdateInfo& other)
    : name(other.name), version(other.version), size(other.size) {}

Updater::UpdateInfo::UpdateInfo(QString name, QVersionNumber version, quint64 size)
    : name(name), version(version), size(size) {}

UpdaterPrivate::UpdaterPrivate(Updater* q_ptr)
    : QObject(nullptr), q(q_ptr), tool_path(), update_info(), normal_exit(true),
      last_error_code(EXIT_SUCCESS), last_error_log(), running(false), main_process(nullptr),
      run_arguments() {
    connect(qApp, &QCoreApplication::aboutToQuit, this, &UpdaterPrivate::AboutToExit,
            Qt::DirectConnection);
}

UpdaterPrivate::~UpdaterPrivate() {
    if (UISettings::values.update_on_close)
        LOG_WARNING(Frontend,
                    "Updater destroyed with run on exit active before the application quit");

    if (main_process && main_process->state() != QProcess::NotRunning) {
        main_process->kill();
        main_process->waitForFinished(1000);
    }
}

const QString UpdaterPrivate::toSystemExe(QString basePath) {
#if defined(Q_OS_WIN32)
    if (!basePath.endsWith(QStringLiteral(".exe")))
        return basePath + QStringLiteral(".exe");
    else
        return basePath;
#elif defined(Q_OS_OSX)
    if (basePath.endsWith(QStringLiteral(".app")))
        basePath.truncate(basePath.lastIndexOf(QStringLiteral(".")));
    return basePath + QStringLiteral(".app/Contents/MacOS/") + QFileInfo(basePath).fileName();
#elif defined(Q_OS_UNIX)
    return basePath;
#endif
}

bool UpdaterPrivate::StartUpdateCheck() {
    if (running)
        return false;
    else {
        update_info.clear();
        normal_exit = true;
        last_error_code = EXIT_SUCCESS;
        last_error_log.clear();

        QFileInfo toolInfo(QCoreApplication::applicationDirPath(), tool_path);
        main_process = new QProcess(this);
        main_process->setProgram(toolInfo.absoluteFilePath());
        main_process->setArguments({QStringLiteral("--checkupdates"), QStringLiteral("-v")});

        connect(main_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
                &UpdaterPrivate::UpdaterReady, Qt::QueuedConnection);
        connect(main_process, &QProcess::errorOccurred, this, &UpdaterPrivate::UpdaterError,
                Qt::QueuedConnection);

        main_process->start(QIODevice::ReadOnly);
        running = true;

        emit q->UpdateInfoChanged(update_info);
        emit q->RunningChanged(true);
        return true;
    }
}

void UpdaterPrivate::StopUpdateCheck(int delay, bool async) {
    if (main_process && main_process->state() != QProcess::NotRunning) {
        if (delay > 0) {
            main_process->terminate();
            if (async) {
                QTimer::singleShot(delay, this, [this]() { StopUpdateCheck(0, false); });
            } else {
                if (!main_process->waitForFinished(delay)) {
                    main_process->kill();
                    main_process->waitForFinished(100);
                }
            }
        } else {
            main_process->kill();
            main_process->waitForFinished(100);
        }
    }
}

XMLParseResult UpdaterPrivate::ParseResult(const QByteArray& output,
                                           QList<Updater::UpdateInfo>& out) {
    const auto outString = QString::fromUtf8(output);
    const auto xmlBegin = outString.indexOf(QStringLiteral("<updates>"));
    if (xmlBegin < 0)
        return XMLParseResult::NO_UPDATE;
    const auto xmlEnd = outString.indexOf(QStringLiteral("</updates>"), xmlBegin);
    if (xmlEnd < 0)
        return XMLParseResult::NO_UPDATE;

    QList<Updater::UpdateInfo> updates;
    QXmlStreamReader reader(outString.mid(xmlBegin, (xmlEnd + 10) - xmlBegin));

    reader.readNextStartElement();
    // should always work because it was search for
    if (reader.name() != QStringLiteral("updates")) {
        return XMLParseResult::INVALID_XML;
    }

    while (reader.readNextStartElement()) {
        if (reader.name() != QStringLiteral("update"))
            return XMLParseResult::INVALID_XML;

        auto ok = false;
        Updater::UpdateInfo info(
            reader.attributes().value(QStringLiteral("name")).toString(),
            QVersionNumber::fromString(
                reader.attributes().value(QStringLiteral("version")).toString()),
            reader.attributes().value(QStringLiteral("size")).toULongLong(&ok));

        if (info.name.isEmpty() || info.version.isNull() || !ok)
            return XMLParseResult::INVALID_XML;
        if (reader.readNextStartElement())
            return XMLParseResult::INVALID_XML;

        updates.append(info);
    }

    if (reader.hasError()) {
        LOG_ERROR(Frontend, "Cannot read xml for update: %s", reader.errorString());
        return XMLParseResult::INVALID_XML;
    }

    out = updates;
    return XMLParseResult::SUCCESS;
}

void UpdaterPrivate::UpdaterReady(int exitCode, QProcess::ExitStatus exitStatus) {
    if (main_process) {
        if (exitStatus == QProcess::NormalExit) {
            normal_exit = true;
            last_error_code = exitCode;
            last_error_log = main_process->readAllStandardError();
            const auto updateOut = main_process->readAllStandardOutput();
            main_process->deleteLater();
            main_process = nullptr;

            running = false;
            emit q->RunningChanged(false);
            QList<Updater::UpdateInfo> update_info;
            auto err = ParseResult(updateOut, update_info);
            bool hasError = false;
            if (err == XMLParseResult::SUCCESS) {
                if (!update_info.isEmpty())
                    emit q->UpdateInfoChanged(update_info);
            } else if (err == XMLParseResult::INVALID_XML) {
                hasError = true;
            }
            emit q->CheckUpdatesDone(!update_info.isEmpty(), hasError);
        } else {
            UpdaterError(QProcess::Crashed);
        }
    }
}

void UpdaterPrivate::UpdaterError(QProcess::ProcessError error) {
    if (main_process) {
        normal_exit = false;
        last_error_code = error;
        last_error_log = main_process->errorString().toUtf8();
        main_process->deleteLater();
        main_process = nullptr;

        running = false;
        emit q->RunningChanged(false);
        emit q->CheckUpdatesDone(false, true);
    }
}

void UpdaterPrivate::AboutToExit() {
    if (UISettings::values.update_on_close) {
        QFileInfo toolInfo(QCoreApplication::applicationDirPath(), tool_path);
        bool success = false;
        std::string user;
        if (UISettings::values.update_as_admin) {
            AdminAuthorization authorizer;
            success = authorizer.executeAsAdmin(toolInfo.absoluteFilePath(), run_arguments);
            user = "admin/root";
        } else {
            success = QProcess::startDetached(toolInfo.absoluteFilePath(), run_arguments,
                                              toolInfo.absolutePath());
            user = "current user";
        }

        if (!success) {
            LOG_WARNING(Frontend, "Unable to start program %s with arguments %s as %s",
                        toolInfo.absoluteFilePath(), run_arguments, user);
        }
    }
}
