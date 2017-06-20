set(SRCS
            cpr.cpp
            glad.cpp
            tests.cpp
            common/param_package.cpp
            core/file_sys/path_parser.cpp
            )

set(HEADERS
            )

create_directory_groups(${SRCS} ${HEADERS})

add_executable(tests ${SRCS} ${HEADERS})
target_include_directories(tests PRIVATE ${CPR_INCLUDE_DIRS})
target_link_libraries(tests PRIVATE common core cpr)
target_link_libraries(tests PRIVATE glad) # To support linker work-around
target_link_libraries(tests PRIVATE ${PLATFORM_LIBRARIES} catch-single-include Threads::Threads)

add_test(NAME tests COMMAND tests)
