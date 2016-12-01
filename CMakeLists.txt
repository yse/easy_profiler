project(profiling_tool)
cmake_minimum_required(VERSION 2.8)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

set(
    EASY_ROOT
    ${CMAKE_CURRENT_LIST_DIR}
)

set(OUTPUT_DIR
    ${EASY_ROOT}/bin
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
add_subdirectory(profiler_gui)

add_subdirectory(sample)
add_subdirectory(reader)

