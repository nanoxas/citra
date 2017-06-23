// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <spdlog/spdlog.h>

namespace Log {

class Formatter : public spdlog::formatter {

public:
    explicit Formatter() = default;
    Formatter(const Formatter&) = delete;
    Formatter& operator=(const Formatter&) = delete;
    void format(spdlog::details::log_msg& msg) override;
};

} // namespace Log