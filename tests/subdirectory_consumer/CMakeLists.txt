cmake_minimum_required(VERSION 3.5...${CMAKE_VERSION})
project(easy_profiler_subdirectory_consumer LANGUAGES CXX)

set(expected_runtime_directory "${CMAKE_CURRENT_BINARY_DIR}/parent-bin")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${expected_runtime_directory}")
set(BUILD_SHARED_LIBS OFF)
set(EASY_PROFILER_NO_GUI ON CACHE BOOL "" FORCE)
set(EASY_PROFILER_NO_SAMPLES ON CACHE BOOL "" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)

add_subdirectory("${EASY_PROFILER_SOURCE_DIR}" easy_profiler)

if (NOT CMAKE_RUNTIME_OUTPUT_DIRECTORY STREQUAL expected_runtime_directory)
    message(FATAL_ERROR "easy_profiler overwrote its parent output directory")
endif ()
if (BUILD_SHARED_LIBS)
    message(FATAL_ERROR "easy_profiler overwrote its parent's BUILD_SHARED_LIBS")
endif ()
if (NOT TARGET easy_profiler::easy_profiler)
    message(FATAL_ERROR "easy_profiler namespaced target is missing")
endif ()

get_target_property(easy_profiler_type easy_profiler TYPE)
if (NOT easy_profiler_type STREQUAL "STATIC_LIBRARY")
    message(FATAL_ERROR "easy_profiler did not honor parent BUILD_SHARED_LIBS=OFF")
endif ()

add_executable(easy_profiler_subdirectory_consumer main.cpp)
target_link_libraries(easy_profiler_subdirectory_consumer PRIVATE
    easy_profiler::easy_profiler
)
set_target_properties(easy_profiler_subdirectory_consumer PROPERTIES
    CXX_STANDARD 11
    CXX_STANDARD_REQUIRED ON
)

add_custom_target(run_consumer
    COMMAND $<TARGET_FILE:easy_profiler_subdirectory_consumer>
    DEPENDS easy_profiler_subdirectory_consumer
    VERBATIM
)
