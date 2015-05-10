// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/service/service.h"

namespace Service {
namespace HTTP {

class HTTP_C_Interface : public Service::Interface {
public:
    HTTP_C_Interface();

    std::string GetPortName() const override {
        return "http:C";
    }
};

} // namespace
}
