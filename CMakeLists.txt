cmake_minimum_required(VERSION 3.5...${CMAKE_VERSION})
project(easy_profiler CXX)

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

set(EASY_PROGRAM_VERSION_MAJOR 3)
set(EASY_PROGRAM_VERSION_MINOR 0)
set(EASY_PROGRAM_VERSION_PATCH 0)
set(EASY_PRODUCT_VERSION_STRING "${EASY_PROGRAM_VERSION_MAJOR}.${EASY_PROGRAM_VERSION_MINOR}.${EASY_PROGRAM_VERSION_PATCH}")

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

SET(CMAKE_INSTALL_RPATH "$ORIGIN")

add_subdirectory(easy_profiler_core)
if (NOT EASY_PROFILER_NO_GUI)
    add_subdirectory(profiler_gui)
endif()
add_subdirectory(easy_profiler_converter)

if (NOT EASY_PROFILER_NO_SAMPLES)
    add_subdirectory(sample)
    add_subdirectory(reader)
endif ()

if (EASY_PROFILER_TOP_LEVEL AND BUILD_TESTING)
    add_subdirectory(tests)
endif ()
