#set(CMAKE_PREFIX_PATH f:/qt/5.5/5.6/msvc2013_64/lib/cmake)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

find_package(Qt5Widgets)

if (Qt5Widgets_FOUND)
    if (NOT("${CMAKE_BUILD_TYPE}" STREQUAL "Debug") AND WIN32)
        set(APPLICATION_PLATFORM WIN32)
    endif ()
    add_executable(profiler_gui ${APPLICATION_PLATFORM}
        main.cpp
        arbitrary_value_inspector.h
        arbitrary_value_inspector.cpp
        blocks_graphics_view.h
        blocks_graphics_view.cpp
        blocks_tree_widget.h
        blocks_tree_widget.cpp
        common_functions.h
        common_functions.cpp
        common_types.h
        descriptors_tree_widget.h
        descriptors_tree_widget.cpp
        easy_chronometer_item.h
        easy_chronometer_item.cpp
        easy_frame_rate_viewer.h
        easy_frame_rate_viewer.cpp
        easy_graphics_item.h
        easy_graphics_item.cpp
        easy_graphics_scrollbar.h
        easy_graphics_scrollbar.cpp
        easy_qtimer.h
        easy_qtimer.cpp
        globals.h
        globals.cpp
        globals_qobjects.cpp
        main_window.h
        main_window.cpp
        tree_widget_item.h
        tree_widget_item.cpp
        tree_widget_loader.h
        tree_widget_loader.cpp
        treeview_first_column_delegate.h
        treeview_first_column_delegate.cpp
        resources.qrc
        resources.rc
    )
    target_link_libraries(profiler_gui Qt5::Widgets easy_profiler)
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
    message(STATUS "INFO\n\n\tQt5 not found! Generating EasyProfiler projects without GUI.\n")
endif ()

