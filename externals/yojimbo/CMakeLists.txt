# A simple cmake file to wrap yojimbo

add_library(yojimbo STATIC
    yojimbo/tlsf/tlsf.c
    yojimbo/tlsf/tlsf.h
    yojimbo/netcode.io/netcode.c
    yojimbo/netcode.io/netcode.h
    yojimbo/reliable.io/reliable.c
    yojimbo/reliable.io/reliable.h
    yojimbo/yojimbo.cpp
    yojimbo/yojimbo.h
)

target_include_directories(yojimbo PRIVATE yojimbo/netcode.io yojimbo/reliable.io)

if (MSVC)
    target_include_directories(yojimbo PRIVATE yojimbo/windows)
    target_link_libraries(yojimbo PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/yojimbo/windows/sodium)
else()
    find_package(libsodium)
    if (NOT ${sodium_FOUND})
        message(FATAL "Libsodium cannot be found! Please install libsodium")
    endif()
    target_include_directories(yojimbo PRIVATE ${sodium_INCLUDE_DIR})
    target_link_libraries(yojimbo PRIVATE ${sodium_LIBRARY_RELEASE})
endif()

target_compile_definitions(yojimbo INTERFACE -DYOJIMBO_WITH_MBEDTLS=0)
target_include_directories(yojimbo INTERFACE yojimbo)

create_target_directory_groups(yojimbo)
