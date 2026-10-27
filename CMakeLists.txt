cmake_minimum_required(VERSION 3.5...${CMAKE_VERSION})

# The VERSION file is the only place the product version is written. Bazel
# cannot read it, so version.bzl repeats it and a test compares the two.
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" EASY_PRODUCT_VERSION_STRING LIMIT_COUNT 1)
if (NOT EASY_PRODUCT_VERSION_STRING MATCHES "^([0-9]+)\\.([0-9]+)\\.([0-9]+)$")
    message(FATAL_ERROR
        "VERSION must hold a single MAJOR.MINOR.PATCH line, found '${EASY_PRODUCT_VERSION_STRING}'")
endif ()
set(EASY_PROGRAM_VERSION_MAJOR ${CMAKE_MATCH_1})
set(EASY_PROGRAM_VERSION_MINOR ${CMAKE_MATCH_2})
set(EASY_PROGRAM_VERSION_PATCH ${CMAKE_MATCH_3})

project(easy_profiler VERSION ${EASY_PRODUCT_VERSION_STRING} LANGUAGES CXX)

if (CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(EASY_PROFILER_TOP_LEVEL ON)
else ()
    set(EASY_PROFILER_TOP_LEVEL OFF)
endif ()

if (EASY_PROFILER_TOP_LEVEL)
    include(CTest)
endif ()

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

if (CMAKE_VERSION VERSION_LESS "3.1")
    if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
      set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=gnu++11")
    endif ()
else ()
    set(CMAKE_CXX_STANDARD 11)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
endif ()

if (MSVC)
    if (NOT (MSVC_VERSION LESS 1914))
        # turn on valid __cplusplus macro value for visual studio (available since msvc 2017 update 7)
        set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /Zc:__cplusplus")
    endif ()
endif ()

option(EASY_PROFILER_NO_GUI "Build easy_profiler without the GUI application (required Qt)" OFF)
option(EASY_PROFILER_NO_SAMPLES "Build easy_profiler without samples and command-line reader" OFF)
option(EASY_PROFILER_REQUIRE_GUI "Fail at configure time when Qt for the GUI is missing" OFF)

# A release package ships profiler_reader but no samples, which the single
# NO_SAMPLES switch cannot express. Default to the historic behaviour.
if (EASY_PROFILER_NO_SAMPLES)
    set(_easy_reader_default OFF)
else ()
    set(_easy_reader_default ON)
endif ()
option(EASY_PROFILER_BUILD_READER "Build the profiler_reader command-line tool"
       ${_easy_reader_default})
unset(_easy_reader_default)
option(EASY_PROFILER_STRICT_ALIGNMENT "Use memcpy-based access for packed profiler data" OFF)
option(EASY_PROFILER_COVERAGE "Instrument Clang builds for LLVM source coverage" OFF)
set(EASY_PROFILER_SANITIZERS "" CACHE STRING
    "Comma-separated sanitizers for GCC/Clang builds (for example: address,undefined or thread)")

if (EASY_PROFILER_SANITIZERS)
    if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
        add_compile_options(
            -fsanitize=${EASY_PROFILER_SANITIZERS}
            -fno-omit-frame-pointer
        )
        set(CMAKE_EXE_LINKER_FLAGS
            "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=${EASY_PROFILER_SANITIZERS}")
        set(CMAKE_SHARED_LINKER_FLAGS
            "${CMAKE_SHARED_LINKER_FLAGS} -fsanitize=${EASY_PROFILER_SANITIZERS}")
    else ()
        message(FATAL_ERROR "EASY_PROFILER_SANITIZERS requires GCC or Clang")
    endif ()
endif ()

if (EASY_PROFILER_COVERAGE)
    if (CMAKE_CXX_COMPILER_ID MATCHES "Clang|AppleClang")
        add_compile_options(-fprofile-instr-generate -fcoverage-mapping)
        set(CMAKE_EXE_LINKER_FLAGS
            "${CMAKE_EXE_LINKER_FLAGS} -fprofile-instr-generate -fcoverage-mapping")
        set(CMAKE_SHARED_LINKER_FLAGS
            "${CMAKE_SHARED_LINKER_FLAGS} -fprofile-instr-generate -fcoverage-mapping")
    else ()
        message(FATAL_ERROR "EASY_PROFILER_COVERAGE requires Clang")
    endif ()
endif ()

if (EASY_PROFILER_TOP_LEVEL AND NOT DEFINED BUILD_SHARED_LIBS)
    option(BUILD_SHARED_LIBS "Build libraries as shared libraries" ON)
endif ()

if (EASY_PROFILER_TOP_LEVEL)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/bin)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/bin)
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/bin)
endif ()

# set(CMAKE_INSTALL_PREFIX ${CMAKE_CURRENT_LIST_DIR}/sdk)

macro(easy_define_target_option TARGET SOURCE_OPTION TARGET_DEFINITION)
    if (${SOURCE_OPTION})
        set(_VALUE 1)
    else ()
        set(_VALUE 0)
    endif ()
    target_compile_definitions(${TARGET} PUBLIC ${TARGET_DEFINITION}=${_VALUE})
endmacro()

# Tools install to bin/ and the library to lib/, so a loader path relative to
# the binary itself must climb out of bin/. The bare origin entry keeps a
# side-by-side layout working, which is how the GUI bundle ships.
if (APPLE)
    set(CMAKE_INSTALL_RPATH "@loader_path/../lib" "@loader_path")
else ()
    set(CMAKE_INSTALL_RPATH "$ORIGIN/../lib" "$ORIGIN")
endif ()

add_subdirectory(easy_profiler_core)
if (NOT EASY_PROFILER_NO_GUI)
    add_subdirectory(profiler_gui)
endif()
add_subdirectory(easy_profiler_converter)

if (NOT EASY_PROFILER_NO_SAMPLES)
    add_subdirectory(sample)
endif ()

if (EASY_PROFILER_BUILD_READER)
    add_subdirectory(reader)
endif ()

if (EASY_PROFILER_TOP_LEVEL AND BUILD_TESTING)
    add_subdirectory(tests)
endif ()
