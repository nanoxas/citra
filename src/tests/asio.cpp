#include <asio.hpp>
#include <catch.hpp>

TEST_CASE("asio exists", "[dummy]") {
    asio::io_service io_service;
}