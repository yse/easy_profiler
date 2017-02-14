project(profiler_reader)

set(CPP_FILES
    main.cpp
)

set(SOURCES
    ${CPP_FILES}
)

add_executable(${PROJECT_NAME} ${SOURCES})

if(MINGW OR UNIX)
    set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=gnu++11")
endif(MINGW OR UNIX)

if(UNIX)
    set(SPEC_LIB ${SPEC_LIB} pthread)
endif(UNIX)

target_link_libraries(${PROJECT_NAME} easy_profiler ${SPEC_LIB})
