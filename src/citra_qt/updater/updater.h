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
    Q_OBJECT

    //! Holds the path of the attached maintenancetool
    Q_PROPERTY(QString maintenanceToolPath READ maintenanceToolPath CONSTANT FINAL)
    //! Specifies whether the updater is currently checking for updates or not
    Q_PROPERTY(bool running READ isRunning NOTIFY runningChanged)
    //! Holds extended information about the last update check
    Q_PROPERTY(QList<UpdateInfo> updateInfo READ updateInfo NOTIFY updateInfoChanged)

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
    explicit Updater(const QString& maintenanceToolPath, QObject* parent = nullptr);
    //! Destroys the updater and kills the update check (if running)
    ~Updater();

    //! Returns `true`, if the updater exited normally
    bool exitedNormally() const;
    //! Returns the mainetancetools error code of the last update
    int errorCode() const;
    //! returns the error output (stderr) of the last update
    QByteArray errorLog() const;

    //! Returns `true` if the maintenancetool will be started on exit
    bool willRunOnExit() const;

    //! readAcFn{Updater::maintenanceToolPath}
    QString maintenanceToolPath() const;
    //! readAcFn{Updater::running}
    bool isRunning() const;
    //! readAcFn{Updater::updateInfo}
    QList<UpdateInfo> updateInfo() const;

public slots:
    //! Starts checking for updates
    bool checkForUpdates();
    //! Aborts checking for updates
    void abortUpdateCheck(int maxDelay = 5000, bool async = false);

    //! Runs the maintenancetool as updater on exit, using the given admin authorisation
    void runUpdaterOnExit(AdminAuthorizer* authorizer = nullptr);
    //! Runs the maintenancetool as updater on exit, using the given arguments and admin
    //! authorisation
    void runUpdaterOnExit(const QStringList& arguments, AdminAuthorizer* authorizer = nullptr);
    //! The updater will not run the maintenancetool on exit anymore
    void cancelExitRun();

signals:
    //! Will be emitted as soon as the updater finished checking for updates
    void checkUpdatesDone(bool hasUpdates, bool hasError);

    //! notifyAcFn{Updater::running}
    void runningChanged(bool running);
    //! notifyAcFn{Updater::updateInfo}
    void updateInfoChanged(QList<Updater::UpdateInfo> updateInfo);

private:
    std::unique_ptr<UpdaterPrivate> backend;
};
