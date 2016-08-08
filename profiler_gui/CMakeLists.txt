cmake_minimum_required(VERSION 3.0.0 FATAL_ERROR)

project(profiler_gui)

#set(CMAKE_PREFIX_PATH f:/qt/5.5/5.6/msvc2013_64/lib/cmake)

set(CMAKE_AUTOMOC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

find_package(Qt5Widgets REQUIRED)

add_executable(${PROJECT_NAME}
    main.cpp
    blocks_graphics_view.h
    blocks_graphics_view.cpp
    blocks_tree_widget.h
    blocks_tree_widget.cpp
    globals.h
    globals.cpp
    globals_qobjects.cpp
    graphics_scrollbar.h
    graphics_scrollbar.cpp
    main_window.h
    main_window.cpp
    treemodel.h
    treemodel.cpp
    treeitem.h
    treeitem.cpp
)

target_link_libraries(${PROJECT_NAME} Qt5::Widgets easy_profiler)
