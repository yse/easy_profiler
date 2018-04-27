cmake_minimum_required(VERSION 3.0.0 FATAL_ERROR)

project(profiler_gui CXX)

#set(CMAKE_PREFIX_PATH f:/qt/5.5/5.6/msvc2013_64/lib/cmake)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

find_package(Qt5Widgets)

if(Qt5Widgets_FOUND)

if(NOT DEFINED EASY_PROGRAM_VERSION_MAJOR)
    set(EASY_PROGRAM_VERSION_MAJOR 1)
    set(EASY_PROGRAM_VERSION_MINOR 0)
    set(EASY_PROGRAM_VERSION_PATCH 0)
    set(EASY_PRODUCT_VERSION_STRING "1.0.0")
endif(NOT DEFINED EASY_PROGRAM_VERSION_MAJOR)

add_definitions(
	-DEASY_PROFILER_VERSION_MAJOR=${EASY_PROGRAM_VERSION_MAJOR}
	-DEASY_PROFILER_VERSION_MINOR=${EASY_PROGRAM_VERSION_MINOR}
	-DEASY_PROFILER_VERSION_PATCH=${EASY_PROGRAM_VERSION_PATCH}
)

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
    resources.qrc
    resources.rc
)

if (MINGW)
    add_definitions(
        -D_WIN32_WINNT=0x0600
        -DSTRSAFE_NO_DEPRECATE
    )
    set (SPECIAL_LIB ${SPECIAL_LIB} ws2_32 psapi)
    set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
elseif (CMAKE_VERSION VERSION_LESS "3.1")
    if (UNIX OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        set (CMAKE_CXX_FLAGS "-std=gnu++11 ${CMAKE_CXX_FLAGS}")
    endif (UNIX OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
else ()
    set_target_properties(${PROJECT_NAME} PROPERTIES
        CXX_STANDARD 11
        CXX_STANDARD_REQUIRED ON
    )
endif (MINGW)

if(UNIX)
    set(SPECIAL_LIB ${SPECIAL_LIB} pthread)
endif(UNIX)

target_link_libraries(${PROJECT_NAME} Qt5::Widgets  easy_profiler ${SPECIAL_LIB})
else()

message(STATUS "INFO\n\n\tQt5 not found! Generating EasyProfiler projects without GUI.\n")

endif(Qt5Widgets_FOUND)

