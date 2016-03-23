// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <random>

template <typename T>
T RandInt(T min, T max) {
    static std::random_device rd;
    static std::mt19937 mt(rd());
    std::uniform_int_distribution<T> rand(min, max);
    return rand(mt);
}
