set(SRCS
            tests.cpp
            )

set(HEADERS
            )

if(ARCHITECTURE_x86_64)
    set(SRCS ${SRCS}
            core/arm/jit_x64/fuzz_arm_branch.cpp
            core/arm/jit_x64/fuzz_arm_common.cpp
            core/arm/jit_x64/fuzz_arm_data_processing.cpp
            core/arm/jit_x64/fuzz_thumb.cpp
            )

    set(HEADERS ${HEADERS}
            core/arm/jit_x64/fuzz_arm_common.h
            core/arm/jit_x64/rand_int.h
            )
endif()

create_directory_groups(${SRCS} ${HEADERS})

include_directories(../../externals/catch)

add_executable(tests ${SRCS} ${HEADERS})
target_link_libraries(tests core video_core audio_core common)
target_link_libraries(tests ${SDL2_LIBRARY} ${OPENGL_gl_LIBRARY} inih glad)
if (MSVC)
    target_link_libraries(tests getopt)
endif()
target_link_libraries(tests ${PLATFORM_LIBRARIES})

if(${CMAKE_SYSTEM_NAME} MATCHES "Linux|FreeBSD|OpenBSD|NetBSD")
    install(TARGETS tests RUNTIME DESTINATION "${CMAKE_INSTALL_PREFIX}/bin")
endif()

if (MSVC)
    include(WindowsCopyFiles)

    set(DLL_DEST "${CMAKE_BINARY_DIR}/bin/$<CONFIG>/")

    windows_copy_files(tests ${SDL2_DLL_DIR} ${DLL_DEST} SDL2.dll)

    unset(DLL_DEST)
endif()
