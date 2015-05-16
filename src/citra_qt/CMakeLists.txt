set(CMAKE_AUTOMOC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(SRCS
            config/controller_config.cpp
            config/controller_config_util.cpp
            config.cpp
            debugger/callstack.cpp
            debugger/disassembler.cpp
            debugger/graphics.cpp
            debugger/graphics_breakpoint_observer.cpp
            debugger/graphics_breakpoints.cpp
            debugger/graphics_cmdlists.cpp
            debugger/graphics_framebuffer.cpp
            debugger/graphics_vertex_shader.cpp
            debugger/profiler.cpp
            debugger/ramview.cpp
            debugger/registers.cpp
            util/spinbox.cpp
            bootmanager.cpp
            hotkeys.cpp
            main.cpp
            citra-qt.rc
            )

set(HEADERS
            config/controller_config.h
            config/controller_config_util.h
            config.h
            debugger/callstack.h
            debugger/disassembler.h
            debugger/graphics.h
            debugger/graphics_breakpoint_observer.h
            debugger/graphics_breakpoints.h
            debugger/graphics_breakpoints_p.h
            debugger/graphics_cmdlists.h
            debugger/graphics_framebuffer.h
            debugger/graphics_vertex_shader.h
            debugger/profiler.h
            debugger/ramview.h
            debugger/registers.h
            util/spinbox.h
            bootmanager.h
            hotkeys.h
            main.h
            version.h
            )

set(UIS
            config/controller_config.ui
            debugger/callstack.ui
            debugger/disassembler.ui
            debugger/profiler.ui
            debugger/registers.ui
            hotkeys.ui
            main.ui
            )

create_directory_groups(${SRCS} ${HEADERS} ${UIS})

if (Qt5_FOUND)
    qt5_wrap_ui(UI_HDRS ${UIS})
else()
    qt4_wrap_ui(UI_HDRS ${UIS})
endif()

if (APPLE)
    add_executable(citra-qt MACOSX_BUNDLE ${SRCS} ${HEADERS} ${UI_HDRS})
else()
    add_executable(citra-qt ${SRCS} ${HEADERS} ${UI_HDRS})
endif()
target_link_libraries(citra-qt core common video_core qhexedit)
target_link_libraries(citra-qt ${OPENGL_gl_LIBRARY} ${CITRA_QT_LIBS})
target_link_libraries(citra-qt ${PLATFORM_LIBRARIES} ${CURL_LIBRARIES})

#install(TARGETS citra-qt RUNTIME DESTINATION ${bindir})

if (MSVC)
    # /NJH /NJS /NDL /NFL /NC /NS /NP - Silence any output
    # cmake adds an extra check for command success which doesn't work too well with robocopy 
    # so trick it into thinking the command was successful with the || cmd /c "exit /b 0"
    set(SILENT_ROBOCOPY "/NJH /NJS /NDL /NFL /NC /NS /NP || cmd /c \"exit /b 0\"")
    set(CURL_DLL_SRC "${CURL_BUILT_PREFIX}/bin/")
    set(DLL_DEST "${CMAKE_BINARY_DIR}/bin/$<CONFIG>/")
    # windows commandline expects the / to be \ so switch them
    string(REPLACE "/" "\\" CURL_DLL_SRC ${CURL_DLL_SRC})
    string(REPLACE "/" "\\" DLL_DEST ${DLL_DEST})
    add_custom_command(TARGET citra-qt POST_BUILD
        COMMAND robocopy "${CURL_DLL_SRC}" "${DLL_DEST}" "libcurl.dll" ${SILENT_ROBOCOPY}
    )
    unset(CURL_DLL_SRC)
    if (Qt5_FOUND)
        set(Qt5_DLL_DIR "${Qt5_DIR}/../../../bin")
        set(Qt5_PLATFORMS_DIR "${Qt5_DIR}/../../../plugins/platforms/")
        set(Qt5_DLLS
            icudt*.dll
            icuin*.dll
            icuuc*.dll
            Qt5Core$<$<CONFIG:Debug>:d>.*
            Qt5Gui$<$<CONFIG:Debug>:d>.*
            Qt5OpenGL$<$<CONFIG:Debug>:d>.*
            Qt5Widgets$<$<CONFIG:Debug>:d>.*
        )
        set(PLATFORMS ${DLL_DEST}platforms/)
        
        string(REPLACE "/" "\\" Qt5_DLL_DIR ${Qt5_DLL_DIR})
        string(REPLACE "/" "\\" Qt5_PLATFORMS_DIR ${Qt5_PLATFORMS_DIR})
        string(REPLACE "/" "\\" PLATFORMS ${PLATFORMS})
        
        add_custom_command(TARGET citra-qt POST_BUILD
            COMMAND robocopy ${Qt5_DLL_DIR} ${DLL_DEST} ${Qt5_DLLS} ${SILENT_ROBOCOPY}
            COMMAND if not exist ${PLATFORMS} mkdir ${PLATFORMS} 2> nul
            COMMAND robocopy ${Qt5_PLATFORMS_DIR} ${PLATFORMS} qwindows$<$<CONFIG:Debug>:d>.* ${SILENT_ROBOCOPY}
        )
        unset(Qt5_DLLS)
        unset(Qt5_DLL_DIR)
        unset(Qt5_PLATFORMS_DIR)
        unset(PLATFORMS)
    endif()
    unset(DLL_DEST)
    unset(SILENT_ROBOCOPY)
endif()
