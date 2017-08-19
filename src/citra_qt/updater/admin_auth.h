// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.
//
// Based on the original work by Felix Barx
// Copyright (c) 2015, Felix Barz
// All rights reserved.

#pragma once

#include <QString>
#include <QStringList>

class AdminAuthorizer {
public:
    //! Virtual destructor
    virtual inline ~AdminAuthorizer() {}
    //! Tests whether this program already has elevated rights or not
    virtual bool hasAdminRights() = 0;
    //! Runs a program with the given arguments with elevated rights
    virtual bool executeAsAdmin(const QString& program, const QStringList& arguments) = 0;
};
