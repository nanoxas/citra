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

class AdminAuthorization {
public:
    bool hasAdminRights();
    bool executeAsAdmin(const QString& program, const QStringList& arguments);
};