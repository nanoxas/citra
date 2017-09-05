// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.
//
// Based on the original work by Felix Barx
// Copyright (c) 2015, Felix Barz
// All rights reserved.

#pragma once

#include <memory>

#include <QDateTime>
#include <QList>
#include <QScopedPointer>
#include <QString>
#include <QStringList>
#include <QVersionNumber>

#include "citra_qt/updater/admin_auth.h"

class UpdaterPrivate;

//! The main updater. Can check for updates and run the maintenancetool as updater
class Updater : public QObject {
    Q_OBJECT;

    //! Specifies whether the updater is currently checking for updates or not
    Q_PROPERTY(bool running READ IsRunning NOTIFY RunningChanged);
    //! Holds extended information about the last update check
    Q_PROPERTY(QList<UpdateInfo> update_info READ LatestUpdateInfo NOTIFY UpdateInfoChanged);

public:
    //! Provides information about updates for components
    struct UpdateInfo {
        //! The name of the component that has an update
        QString name;
        //! The new version for that compontent
        QVersionNumber version;
        //! The update download size (in Bytes)
        quint64 size;

        //! Default Constructor
        UpdateInfo();
        //! Copy Constructor
        UpdateInfo(const UpdateInfo& other);
        //! Constructor that takes name, version and size
        UpdateInfo(QString name, QVersionNumber version, quint64 size);
    };

    //! Default constructor
    explicit Updater(QObject* parent = nullptr);
    //! Constructor with an explicitly set path
    explicit Updater(const QString& maintenance_tool_path, QObject* parent = nullptr);
    //! Destroys the updater and kills the update check (if running)
    ~Updater();

    //! Returns `true`, if the updater exited normally
    bool ExitedNormally() const;
    //! Returns the mainetancetools error code of the last update
    int ErrorCode() const;
    //! returns the error output (stderr) of the last update
    QByteArray ErrorLog() const;

    //! readAcFn{Updater::running}
    bool IsRunning() const;
    //! readAcFn{Updater::updateInfo}
    QList<UpdateInfo> LatestUpdateInfo() const;

public slots:
    //! Starts checking for updates
    bool CheckForUpdates();
    //! Aborts checking for updates
    void AbortUpdateCheck(int maxDelay = 5000, bool async = false);

signals:
    //! Will be emitted as soon as the updater finished checking for updates
    void CheckUpdatesDone(bool hasUpdates, bool hasError);

    //! notifyAcFn{Updater::running}
    void RunningChanged(bool running);
    //! notifyAcFn{Updater::updateInfo}
    void UpdateInfoChanged(QList<Updater::UpdateInfo> UpdateInfo);

private:
    std::unique_ptr<UpdaterPrivate> backend;
};
