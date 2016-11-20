project(profiling_tool)
cmake_minimum_required(VERSION 2.8)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

set(
    ROOT
    ${CMAKE_CURRENT_LIST_DIR}
)


file (STRINGS ${ROOT}/version.info PRODUCT_VERSION_STRING)
string(REPLACE "." ";" VERSION_LIST ${PRODUCT_VERSION_STRING})

list(GET VERSION_LIST 0 PROGRAM_VERSION_MAJOR)
list(GET VERSION_LIST 1 PROGRAM_VERSION_MINOR)
list(GET VERSION_LIST 2 PROGRAM_VERSION_PATCH)

message(STATUS "PROGRAM_VERSION_MAJOR: ${PROGRAM_VERSION_MAJOR}")
message(STATUS "PROGRAM_VERSION_MINOR: ${PROGRAM_VERSION_MINOR}")
message(STATUS "PROGRAM_VERSION_PATCH: ${PROGRAM_VERSION_PATCH}")

# EasyProfiler version
add_definitions(
    -DEASY_PROFILER_VERSION_MAJOR=${PROGRAM_VERSION_MAJOR}
    -DEASY_PROFILER_VERSION_MINOR=${PROGRAM_VERSION_MINOR}
    -DEASY_PROFILER_VERSION_PATCH=${PROGRAM_VERSION_PATCH}
    -DEASY_PROFILER_PRODUCT_VERSION=\"v${PRODUCT_VERSION_STRING}\"
)
# EasyProfiler version

set(OUTPUT_DIR
    ${ROOT}/bin
)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY 
    ${OUTPUT_DIR}
)

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY
    ${OUTPUT_DIR}
)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY
    ${OUTPUT_DIR}
)

include_directories(
    include
)

if(UNIX)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=gnu++11")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wno-long-long -Wno-reorder -pedantic -O3" )
else()
    add_definitions(
        -D_CRT_SECURE_NO_WARNINGS
    )
endif(UNIX)

set(CMAKE_INSTALL_PREFIX ${CMAKE_CURRENT_LIST_DIR}/sdk)

add_subdirectory(src)
add_subdirectory(sample)
add_subdirectory(reader)
add_subdirectory(profiler_gui)
