cmake_minimum_required(VERSION 2.8)
project(profiler_sample CXX)

set(CPP_FILES
    main.cpp
)

set(SOURCES
    ${CPP_FILES}
)

include_directories(
    ../easy_profiler_core/include
)
link_directories(${CMAKE_SOURCE_DIR}/../bin)

add_executable(${PROJECT_NAME} ${SOURCES})

set(DISABLED_PROFILER_NAME
    "${PROJECT_NAME}_disabled_profiler"
)
add_executable(${DISABLED_PROFILER_NAME} ${SOURCES})

if (MINGW)

    set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
    
elseif (CMAKE_VERSION VERSION_LESS "3.1")

    if (UNIX OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=gnu++11")
    endif (UNIX OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    
else ()

    set_target_properties(${PROJECT_NAME} PROPERTIES
        CXX_STANDARD 11
        CXX_STANDARD_REQUIRED ON
    )
    
    set_target_properties(${DISABLED_PROFILER_NAME} PROPERTIES
        CXX_STANDARD 11
        CXX_STANDARD_REQUIRED ON
    )
    
endif (MINGW)

if(UNIX)
    set(SPEC_LIB ${SPEC_LIB} pthread)
endif(UNIX)

target_link_libraries(${PROJECT_NAME} easy_profiler ${SPEC_LIB})
target_link_libraries(${DISABLED_PROFILER_NAME} easy_profiler ${SPEC_LIB})

target_compile_definitions(${PROJECT_NAME} PRIVATE BUILD_WITH_EASY_PROFILER)
