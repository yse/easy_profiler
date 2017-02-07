project(profiler_sample)
cmake_minimum_required(VERSION 2.8)

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

if(UNIX)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=gnu++11")
endif(UNIX)

add_executable(${PROJECT_NAME} ${SOURCES})

set(DISABLED_PROFILER_NAME
    "${PROJECT_NAME}_disabled_profiler"
)
add_executable(${DISABLED_PROFILER_NAME} ${SOURCES})

if(UNIX)
    set(SPEC_LIB pthread)
endif(UNIX)

target_link_libraries(${PROJECT_NAME} easy_profiler ${SPEC_LIB})
target_link_libraries(${DISABLED_PROFILER_NAME} easy_profiler ${SPEC_LIB})

target_compile_definitions(${PROJECT_NAME} PRIVATE BUILD_WITH_EASY_PROFILER)
