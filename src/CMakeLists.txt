project(easy_profiler)

set(CPP_FILES
    block.cpp
    profile_manager.cpp
    reader.cpp
    event_trace_win.cpp
    easy_socket.cpp
)

set(H_FILES
    ${ROOT}/include/easy/profiler.h
    ${ROOT}/include/easy/reader.h
    ${ROOT}/include/easy/easy_net.h
    ${ROOT}/include/easy/easy_socket.h
    profile_manager.h
    spin_lock.h
    event_trace_win.h
    current_time.h
)

set(SOURCES
    ${CPP_FILES}
    ${H_FILES}
)
add_definitions(
    -D_BUILD_PROFILER
    -DBUILD_WITH_EASY_PROFILER
    #-DEASY_PROFILER_API_DISABLED # uncomment this to disable profiler api only (you will have to rebuild only easy_profiler)
)

if(WIN32)
add_definitions(
    -D_WINSOCK_DEPRECATED_NO_WARNINGS
)
endif(WIN32)

add_library(${PROJECT_NAME} SHARED ${SOURCES} resources.rc)

if(UNIX)
    set(PLATFORM_LIBS pthread)
endif(UNIX)

target_link_libraries(${PROJECT_NAME} ${PLATFORM_LIBS})
