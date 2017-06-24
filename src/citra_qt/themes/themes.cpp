// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <QString>
#include "citra_qt/themes/themes.h"

std::vector<Theme> themes;

void SetUpThemes() {
    themes.push_back(std::make_pair(QString("Qt Default"), QString("qt-default")));
    themes.push_back(std::make_pair(QString("Dark"), QString("qdarkstyle")));
}
