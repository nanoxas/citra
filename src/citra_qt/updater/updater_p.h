// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.
//
// Based on the original work by Felix Barx
// Copyright (c) 2015, Felix Barz
// All rights reserved.

#pragma once

#include <QtCore/QProcess>

#include "updater.h"

enum class XMLParseResult {
    SUCCESS,
    NO_UPDATE,
    INVALID_XML,
};

class UpdaterPrivate : public QObject {
public:
    UpdaterPrivate(Updater* q_ptr);
    ~UpdaterPrivate();

    Updater* q;

    QString toolPath;
    QList<Updater::UpdateInfo> updateInfos;
    bool normalExit;
    int lastErrorCode;
    QByteArray lastErrorLog;

    bool running;
    QProcess* mainProcess;

    bool runOnExit;
    QStringList runArguments;
    QScopedPointer<AdminAuthorizer> adminAuth;

    static const QString toSystemExe(QString basePath);

    bool startUpdateCheck();
    void stopUpdateCheck(int delay, bool async);
    XMLParseResult parseResult(const QByteArray& output, QList<Updater::UpdateInfo>& out);

public slots:
    void updaterReady(int exitCode, QProcess::ExitStatus exitStatus);
    void updaterError(QProcess::ProcessError error);

    void appAboutToExit();
};
