#set(CMAKE_PREFIX_PATH f:/qt/5.5/5.6/msvc2013_64/lib/cmake)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

# Without this a release job that lost its Qt would quietly produce a package
# with no GUI in it and still succeed.
if (EASY_PROFILER_REQUIRE_GUI)
    set(_easy_qt_requirement REQUIRED)
else ()
    set(_easy_qt_requirement "")
endif ()

find_package(Qt6Widgets ${_easy_qt_requirement})
find_package(Qt6Core5Compat ${_easy_qt_requirement})
# Only loaded as a plugin at run time, but the GUI is unusable without it: the
# toolbar icons are SVG. Finding it here makes a broken Qt install fail now.
find_package(Qt6Svg ${_easy_qt_requirement})
unset(_easy_qt_requirement)

if (Qt6Widgets_FOUND)
    message(STATUS "Using Qt v${Qt6Widgets_VERSION}")
    if (NOT("${CMAKE_BUILD_TYPE}" STREQUAL "Debug") AND WIN32)
        set(APPLICATION_PLATFORM WIN32)
    endif ()
    add_executable(profiler_gui ${APPLICATION_PLATFORM}
        main.cpp
        arbitrary_value_inspector.h
        arbitrary_value_inspector.cpp
        arbitrary_value_tooltip.h
        arbitrary_value_tooltip.cpp
        blocks_graphics_view.h
        blocks_graphics_view.cpp
        blocks_tree_widget.h
        blocks_tree_widget.cpp
        bookmarks_editor.h
        bookmarks_editor.cpp
        common_functions.h
        common_functions.cpp
        common_types.h
        descriptors_tree_widget.h
        descriptors_tree_widget.cpp
        dialog.h
        dialog.cpp
        file_reader.h
        file_reader.cpp
        fps_widget.h
        fps_widget.cpp
        globals.h
        globals.cpp
        globals_qobjects.cpp
        graphics_block_item.h
        graphics_block_item.cpp
        graphics_image_item.h
        graphics_image_item.cpp
        graphics_ruler_item.h
        graphics_ruler_item.cpp
        graphics_scrollbar.h
        graphics_scrollbar.cpp
        graphics_slider_area.h
        graphics_slider_area.cpp
        main_window.h
        main_window.cpp
        round_progress_widget.h
        round_progress_widget.cpp
        socket_listener.h
        socket_listener.cpp
        text_highlighter.h
        text_highlighter.cpp
        timer.h
        timer.cpp
        thread_pool.h
        thread_pool.cpp
        thread_pool_task.h
        thread_pool_task.cpp
        tree_widget_item.h
        tree_widget_item.cpp
        tree_widget_loader.h
        tree_widget_loader.cpp
        window_header.h
        window_header.cpp
        resources.qrc
        resources.rc
    )
    target_link_libraries(profiler_gui Qt6::Widgets Qt6::Core5Compat easy_profiler)
    if (WIN32)
        target_compile_definitions(profiler_gui PRIVATE -D_WIN32_WINNT=0x0600)
    endif ()
    if (MINGW)
        target_compile_definitions(profiler_gui PRIVATE -DSTRSAFE_NO_DEPRECATE)
    endif ()

    install(
        TARGETS
        profiler_gui
        RUNTIME
        DESTINATION
        bin
    )

    set_property(TARGET profiler_gui PROPERTY INSTALL_RPATH_USE_LINK_PATH TRUE)
else ()
    message(STATUS "INFO\n\n\tQt6 not found! Generating EasyProfiler projects without GUI.\n")
endif ()

