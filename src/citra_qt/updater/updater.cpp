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

#include "citra_qt/updater/updater.h"
#include "citra_qt/updater/updater_p.h"
#include "common/logging/log.h"

#ifdef Q_OS_OSX
#define DEFAULT_TOOL_PATH QStringLiteral("../../../maintenancetool")
#else
#define DEFAULT_TOOL_PATH QStringLiteral("../maintenancetool")
#endif

Updater::Updater(QObject* parent) : Updater(DEFAULT_TOOL_PATH, parent) {}

Updater::Updater(const QString& maintenanceToolPath, QObject* parent)
    : QObject(parent), backend(new UpdaterPrivate(this)) {
    backend->toolPath = UpdaterPrivate::toSystemExe(maintenanceToolPath);
}

Updater::~Updater() {}

bool Updater::exitedNormally() const {
    return backend->normalExit;
}

int Updater::errorCode() const {
    return backend->lastErrorCode;
}

QByteArray Updater::errorLog() const {
    return backend->lastErrorLog;
}

bool Updater::willRunOnExit() const {
    return backend->runOnExit;
}

QString Updater::maintenanceToolPath() const {
    return backend->toolPath;
}

bool Updater::isRunning() const {
    return backend->running;
}

QList<Updater::UpdateInfo> Updater::updateInfo() const {
    return backend->updateInfos;
}

bool Updater::checkForUpdates() {
    return backend->startUpdateCheck();
}

void Updater::abortUpdateCheck(int maxDelay, bool async) {
    backend->stopUpdateCheck(maxDelay, async);
}

void Updater::runUpdaterOnExit(AdminAuthorizer* authorizer) {
    runUpdaterOnExit({QStringLiteral("--updater")}, authorizer);
}

void Updater::runUpdaterOnExit(const QStringList& arguments, AdminAuthorizer* authorizer) {
    backend->runOnExit = true;
    backend->runArguments = arguments;
    backend->adminAuth.reset(authorizer);
}

void Updater::cancelExitRun() {
    backend->runOnExit = false;
    backend->adminAuth.reset();
}

Updater::UpdateInfo::UpdateInfo() : name(), version(), size(0ull) {}

Updater::UpdateInfo::UpdateInfo(const Updater::UpdateInfo& other)
    : name(other.name), version(other.version), size(other.size) {}

Updater::UpdateInfo::UpdateInfo(QString name, QVersionNumber version, quint64 size)
    : name(name), version(version), size(size) {}

UpdaterPrivate::UpdaterPrivate(Updater* q_ptr)
    : QObject(nullptr), q(q_ptr), toolPath(), updateInfos(), normalExit(true),
      lastErrorCode(EXIT_SUCCESS), lastErrorLog(), running(false), mainProcess(nullptr),
      runOnExit(false), runArguments(), adminAuth(nullptr) {
    connect(qApp, &QCoreApplication::aboutToQuit, this, &UpdaterPrivate::appAboutToExit,
            Qt::DirectConnection);
}

UpdaterPrivate::~UpdaterPrivate() {
    if (runOnExit)
        LOG_WARNING(Frontend,
                    "Updater destroyed with run on exit active before the application quit");

    if (mainProcess && mainProcess->state() != QProcess::NotRunning) {
        mainProcess->kill();
        mainProcess->waitForFinished(1000);
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

bool UpdaterPrivate::startUpdateCheck() {
    if (running)
        return false;
    else {
        updateInfos.clear();
        normalExit = true;
        lastErrorCode = EXIT_SUCCESS;
        lastErrorLog.clear();

        QFileInfo toolInfo(QCoreApplication::applicationDirPath(), toolPath);
        mainProcess = new QProcess(this);
        mainProcess->setProgram(toolInfo.absoluteFilePath());
        mainProcess->setArguments({QStringLiteral("--checkupdates"), QStringLiteral("-v")});

        connect(mainProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
                &UpdaterPrivate::updaterReady, Qt::QueuedConnection);
        connect(mainProcess, &QProcess::errorOccurred, this, &UpdaterPrivate::updaterError,
                Qt::QueuedConnection);

        mainProcess->start(QIODevice::ReadOnly);
        running = true;

        emit q->updateInfoChanged(updateInfos);
        emit q->runningChanged(true);
        return true;
    }
}

void UpdaterPrivate::stopUpdateCheck(int delay, bool async) {
    if (mainProcess && mainProcess->state() != QProcess::NotRunning) {
        if (delay > 0) {
            mainProcess->terminate();
            if (async) {
                QTimer::singleShot(delay, this, [this]() { stopUpdateCheck(0, false); });
            } else {
                if (!mainProcess->waitForFinished(delay)) {
                    mainProcess->kill();
                    mainProcess->waitForFinished(100);
                }
            }
        } else {
            mainProcess->kill();
            mainProcess->waitForFinished(100);
        }
    }
}

XMLParseResult UpdaterPrivate::parseResult(const QByteArray& output,
                                           QList<Updater::UpdateInfo>& out) {

    const auto outString = QString::fromUtf8(output);
    LOG_INFO(Frontend, "Updates %s", output.toStdString());
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
    Q_ASSERT(reader.name() == QStringLiteral("updates"));

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

void UpdaterPrivate::updaterReady(int exitCode, QProcess::ExitStatus exitStatus) {
    if (mainProcess) {
        if (exitStatus == QProcess::NormalExit) {
            normalExit = true;
            lastErrorCode = exitCode;
            lastErrorLog = mainProcess->readAllStandardError();
            const auto updateOut = mainProcess->readAllStandardOutput();
            mainProcess->deleteLater();
            mainProcess = nullptr;

            running = false;
            emit q->runningChanged(false);
            QList<Updater::UpdateInfo> updateInfos;
            auto err = parseResult(updateOut, updateInfos);
            bool hasError = false;
            if (err == XMLParseResult::SUCCESS) {
                if (!updateInfos.isEmpty())
                    emit q->updateInfoChanged(updateInfos);
            } else if (err == XMLParseResult::INVALID_XML) {
                hasError = true;
            }
            emit q->checkUpdatesDone(!updateInfos.isEmpty(), hasError);
        } else {
            updaterError(QProcess::Crashed);
        }
    }
}

void UpdaterPrivate::updaterError(QProcess::ProcessError error) {
    if (mainProcess) {
        normalExit = false;
        lastErrorCode = error;
        lastErrorLog = mainProcess->errorString().toUtf8();
        mainProcess->deleteLater();
        mainProcess = nullptr;

        running = false;
        emit q->runningChanged(false);
        emit q->checkUpdatesDone(false, true);
    }
}

void UpdaterPrivate::appAboutToExit() {
    if (runOnExit) {
        QFileInfo toolInfo(QCoreApplication::applicationDirPath(), toolPath);
        auto ok = false;
        if (adminAuth && !adminAuth->hasAdminRights())
            ok = adminAuth->executeAsAdmin(toolInfo.absoluteFilePath(), runArguments);
        else {
            ok = QProcess::startDetached(toolInfo.absoluteFilePath(), runArguments,
                                         toolInfo.absolutePath());
        }

        if (!ok) {
            LOG_WARNING(Frontend, "Unable to start program %s with arguments %s as %s",
                        toolInfo.absoluteFilePath(), runArguments,
                        (adminAuth ? "admin/root" : "current user"));
        }

        runOnExit = false; // prevent warning
    }
}
