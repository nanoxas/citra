// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <catch.hpp>
#include <cpr/cpr.h>

// Validates that the CPR library can be linked against.
// Attempts to make a connection to google, but doesn't fail the test if it can't.
TEST_CASE("cpr", "[external_lib]") {
    auto r = cpr::GetAsync(cpr::Url{"http://www.httpbin.org/get"});
    REQUIRE(0 == 0);
}
