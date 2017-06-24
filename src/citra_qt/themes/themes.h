// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <vector>

class QString;

typedef std::pair<QString, QString> Theme;

extern std::vector<Theme> themes;

/**
 * SetUp the themes map with the current available ones.
 */
void SetUpThemes();
