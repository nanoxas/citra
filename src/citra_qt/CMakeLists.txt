set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${PROJECT_SOURCE_DIR}/CMakeModules)

set(SRCS
            configuration/config.cpp
            configuration/configure_audio.cpp
            configuration/configure_debug.cpp
            configuration/configure_dialog.cpp
            configuration/configure_general.cpp
            configuration/configure_graphics.cpp
            configuration/configure_input.cpp
            configuration/configure_system.cpp
            debugger/graphics/graphics.cpp
            debugger/graphics/graphics_breakpoint_observer.cpp
            debugger/graphics/graphics_breakpoints.cpp
            debugger/graphics/graphics_cmdlists.cpp
            debugger/graphics/graphics_surface.cpp
            debugger/graphics/graphics_tracing.cpp
            debugger/graphics/graphics_vertex_shader.cpp
            debugger/profiler.cpp
            debugger/registers.cpp
            debugger/wait_tree.cpp
            themes/themes.cpp
            util/spinbox.cpp
            util/util.cpp
            bootmanager.cpp
            game_list.cpp
            hotkeys.cpp
            main.cpp
            ui_settings.cpp
            citra-qt.rc
            Info.plist
            )

set(HEADERS
            configuration/config.h
            configuration/configure_audio.h
            configuration/configure_debug.h
            configuration/configure_dialog.h
            configuration/configure_general.h
            configuration/configure_graphics.h
            configuration/configure_input.h
            configuration/configure_system.h
            debugger/graphics/graphics.h
            debugger/graphics/graphics_breakpoint_observer.h
            debugger/graphics/graphics_breakpoints.h
            debugger/graphics/graphics_breakpoints_p.h
            debugger/graphics/graphics_cmdlists.h
            debugger/graphics/graphics_surface.h
            debugger/graphics/graphics_tracing.h
            debugger/graphics/graphics_vertex_shader.h
            debugger/profiler.h
            debugger/registers.h
            debugger/wait_tree.h
            themes/themes.h
            util/spinbox.h
            util/util.h
            bootmanager.h
            game_list.h
            game_list_p.h
            hotkeys.h
            main.h
            ui_settings.h
            )

set(UIS
            configuration/configure.ui
            configuration/configure_audio.ui
            configuration/configure_debug.ui
            configuration/configure_general.ui
            configuration/configure_graphics.ui
            configuration/configure_input.ui
            configuration/configure_system.ui
            debugger/registers.ui
            hotkeys.ui
            main.ui
            )

set(THEMES
            themes/qdarkstyle/style.qss
            themes/qdarkstyle/style.qrc
            themes/qdarkstyle/rc/branch_closed.png
            themes/qdarkstyle/rc/branch_closed-on.png
            themes/qdarkstyle/rc/branch_open.png
            themes/qdarkstyle/rc/branch_open-on.png
            themes/qdarkstyle/rc/checkbox_checked.png
            themes/qdarkstyle/rc/checkbox_checked_disabled.png
            themes/qdarkstyle/rc/checkbox_checked_focus.png
            themes/qdarkstyle/rc/checkbox_indeterminate.png
            themes/qdarkstyle/rc/checkbox_indeterminate_disabled.png
            themes/qdarkstyle/rc/checkbox_indeterminate_focus.png
            themes/qdarkstyle/rc/checkbox_unchecked.png
            themes/qdarkstyle/rc/checkbox_unchecked_disabled.png
            themes/qdarkstyle/rc/checkbox_unchecked_focus.png
            themes/qdarkstyle/rc/close.png
            themes/qdarkstyle/rc/close-hover.png
            themes/qdarkstyle/rc/close-pressed.png
            themes/qdarkstyle/rc/down_arrow.png
            themes/qdarkstyle/rc/down_arrow_disabled.png
            themes/qdarkstyle/rc/Hmovetoolbar.png
            themes/qdarkstyle/rc/Hsepartoolbar.png
            themes/qdarkstyle/rc/left_arrow.png
            themes/qdarkstyle/rc/left_arrow_disabled.png
            themes/qdarkstyle/rc/radio_checked.png
            themes/qdarkstyle/rc/radio_checked_disabled.png
            themes/qdarkstyle/rc/radio_checked_focus.png
            themes/qdarkstyle/rc/radio_unchecked.png
            themes/qdarkstyle/rc/radio_unchecked_disabled.png
            themes/qdarkstyle/rc/radio_unchecked_focus.png
            themes/qdarkstyle/rc/right_arrow.png
            themes/qdarkstyle/rc/right_arrow_disabled.png
            themes/qdarkstyle/rc/sizegrip.png
            themes/qdarkstyle/rc/stylesheet-branch-end.png
            themes/qdarkstyle/rc/stylesheet-branch-more.png
            themes/qdarkstyle/rc/stylesheet-vline.png
            themes/qdarkstyle/rc/transparent.png
            themes/qdarkstyle/rc/undock.png
            themes/qdarkstyle/rc/up_arrow.png
            themes/qdarkstyle/rc/up_arrow_disabled.png
            themes/qdarkstyle/rc/Vmovetoolbar.png
            themes/qdarkstyle/rc/Vsepartoolbar.png
            )

create_directory_groups(${SRCS} ${HEADERS} ${UIS} ${THEMES})

if (Qt5_FOUND)
    qt5_wrap_ui(UI_HDRS ${UIS})
else()
    qt4_wrap_ui(UI_HDRS ${UIS})
endif()

if (APPLE)
    set(MACOSX_ICON "../../dist/citra.icns")
    set_source_files_properties(${MACOSX_ICON} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    add_executable(citra-qt MACOSX_BUNDLE ${SRCS} ${HEADERS} ${UI_HDRS} ${THEMES} ${MACOSX_ICON})
    set_target_properties(citra-qt PROPERTIES MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/Info.plist)
else()
    add_executable(citra-qt ${SRCS} ${HEADERS} ${UI_HDRS} ${THEMES})
endif()
target_link_libraries(citra-qt PRIVATE audio_core common core input_common video_core)
target_link_libraries(citra-qt PRIVATE Boost::boost glad nihstro-headers Qt5::OpenGL Qt5::Widgets)
target_link_libraries(citra-qt PRIVATE ${PLATFORM_LIBRARIES} Threads::Threads)

if(UNIX AND NOT APPLE)
    install(TARGETS citra-qt RUNTIME DESTINATION "${CMAKE_INSTALL_PREFIX}/bin")
endif()

if (MSVC)
    include(CopyCitraQt5Deps)
    include(CopyCitraSDLDeps)
    copy_citra_Qt5_deps(citra-qt)
    copy_citra_SDL_deps(citra-qt)
endif()
