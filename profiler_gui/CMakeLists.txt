cmake_minimum_required(VERSION 3.0.0 FATAL_ERROR)

project(profiler_gui)

set(CMAKE_AUTOMOC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

find_package(Qt5Widgets REQUIRED)
add_executable(${PROJECT_NAME} main.cpp treemodel.h treemodel.cpp treeitem.h treeitem.cpp)
target_link_libraries(${PROJECT_NAME} Qt5::Widgets easy_profiler)
