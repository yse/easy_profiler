cmake_minimum_required(VERSION 3.0.0 FATAL_ERROR)

project(profiler_gui)

#set(CMAKE_PREFIX_PATH f:/qt/5.5/5.6/msvc2013_64/lib/cmake)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

find_package(Qt5Widgets REQUIRED)
find_package(Qt5Network REQUIRED)

include_directories(${ROOT}/3rdparty/nanomsg/src)

add_executable(${PROJECT_NAME}
    main.cpp
    blocks_graphics_view.h
    blocks_graphics_view.cpp
    blocks_tree_widget.h
    blocks_tree_widget.cpp
    descriptors_tree_widget.h
    descriptors_tree_widget.cpp
    easy_chronometer_item.h
    easy_chronometer_item.cpp
    easy_graphics_item.h
    easy_graphics_item.cpp
    easy_graphics_scrollbar.h
    easy_graphics_scrollbar.cpp
    globals.h
    globals.cpp
    globals_qobjects.cpp
    main_window.h
    main_window.cpp
    tree_widget_item.h
    tree_widget_item.cpp
    tree_widget_loader.h
    tree_widget_loader.cpp
    #treemodel.h
    #treemodel.cpp
    #treeitem.h
    #treeitem.cpp
    resources.qrc
    resources.rc
)

if(UNIX)
    set(SPECIAL_LIB pthread)
endif(UNIX)

target_link_libraries(${PROJECT_NAME} Qt5::Widgets  Qt5::Network easy_profiler ${SPECIAL_LIB})
