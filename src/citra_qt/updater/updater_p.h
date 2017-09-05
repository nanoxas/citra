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
    Q_OBJECT;

public:
    explicit UpdaterPrivate(Updater* q_ptr);
    ~UpdaterPrivate();

    static const QString toSystemExe(QString basePath);

    bool StartUpdateCheck();
    void StopUpdateCheck(int delay, bool async);

public slots:
    void UpdaterReady(int exit_code, QProcess::ExitStatus exit_status);
    void UpdaterError(QProcess::ProcessError error);

    void AboutToExit();

private:
    XMLParseResult ParseResult(const QByteArray& output, QList<Updater::UpdateInfo>& out);

    Updater* q;

    QString tool_path;
    QList<Updater::UpdateInfo> update_info;
    bool normal_exit;
    int last_error_code;
    QByteArray last_error_log;

    bool running;
    QProcess* main_process;

    QStringList run_arguments;

    friend class Updater;
};
