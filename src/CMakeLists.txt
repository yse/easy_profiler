project(easy_profiler)

set(CPP_FILES
    block.cpp
    profile_manager.cpp
    reader.cpp
    event_trace_win.cpp
    easy_socket.cpp
)

set(H_FILES
    ${ROOT}/include/profiler/profiler.h
    ${ROOT}/include/profiler/reader.h
    ${ROOT}/include/profiler/easy_net.h
    ${ROOT}/include/profiler/easy_socket.h
    profile_manager.h
    spin_lock.h
    event_trace_win.h
)

set(SOURCES
    ${CPP_FILES}
    ${H_FILES}
)
add_definitions(
    -D_BUILD_PROFILER
)

add_library(${PROJECT_NAME} SHARED ${SOURCES})

if(UNIX)
set(PLATFORM_LIBS pthread)
endif(UNIX)

target_link_libraries(${PROJECT_NAME} ${PLATFORM_LIBS})
