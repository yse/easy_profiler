cmake_minimum_required(VERSION 3.0)
project(easy_profiler CXX)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

set(EASY_PROGRAM_VERSION_MAJOR 1)
set(EASY_PROGRAM_VERSION_MINOR 3)
set(EASY_PROGRAM_VERSION_PATCH 0)
set(EASY_PRODUCT_VERSION_STRING "${EASY_PROGRAM_VERSION_MAJOR}.${EASY_PROGRAM_VERSION_MINOR}.${EASY_PROGRAM_VERSION_PATCH}")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/bin)

# set(CMAKE_INSTALL_PREFIX ${CMAKE_CURRENT_LIST_DIR}/sdk)

macro(easy_define_target_option TARGET SOURCE_OPTION TARGET_DEFINITION)
    if (${SOURCE_OPTION})
        set(_VALUE 1)
    else ()
        set(_VALUE 0)
    endif ()
    target_compile_options(${TARGET} PUBLIC -D${TARGET_DEFINITION}=${_VALUE})
endmacro()

SET(CMAKE_INSTALL_RPATH "$ORIGIN")

add_subdirectory(easy_profiler_core)
add_subdirectory(profiler_gui)

if (NOT EASY_PROFILER_NO_SAMPLES)
    add_subdirectory(sample)
    add_subdirectory(reader)
endif ()
