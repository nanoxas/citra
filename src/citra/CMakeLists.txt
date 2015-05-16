set(SRCS
            emu_window/emu_window_glfw.cpp
            citra.cpp
            config.cpp
            citra.rc
            )
set(HEADERS
            emu_window/emu_window_glfw.h
            config.h
            default_ini.h
            resource.h
            )

create_directory_groups(${SRCS} ${HEADERS})

add_executable(citra ${SRCS} ${HEADERS})
target_link_libraries(citra core common video_core)
target_link_libraries(citra ${GLFW_LIBRARIES} ${OPENGL_gl_LIBRARY} inih)
target_link_libraries(citra ${PLATFORM_LIBRARIES} ${CURL_LIBRARIES})

#install(TARGETS citra RUNTIME DESTINATION ${bindir})

if (MSVC)
    set(CURL_DLL_SRC "${CURL_BUILT_PREFIX}/bin/libcurl.dll")
    set(DLL_DEST "${CMAKE_BINARY_DIR}/bin/$<CONFIG>/")
    set(SILENT_ROBOCOPY "/NJH /NJS /NDL /NFL /NC /NS /NP || cmd /c \"exit /b 0\"")
    string(REPLACE "/" "\\" CURL_DLL_SRC ${CURL_DLL_SRC})
    string(REPLACE "/" "\\" DLL_DEST ${DLL_DEST})
    add_custom_command(TARGET citra POST_BUILD
        COMMAND robocopy "${CURL_DLL_SRC}" "${DLL_DEST}" "libcurl.dll" ${SILENT_ROBOCOPY}
    )
    unset(CURL_DLL_SRC)
    unset(DLL_DEST)
    unset(SILENT_ROBOCOPY)
endif()
