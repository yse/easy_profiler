project(easy_profiler)

set(CPP_FILES
    block.cpp
    profile_manager.cpp
    reader.cpp
    event_trace_win.cpp
    easy_socket.cpp
)

set(H_FILES

    profile_manager.h
    spin_lock.h
    event_trace_win.h
    current_time.h
)

if(DEFINED EASY_ROOT)
    set(INCLUDE_FILES
        ${EASY_ROOT}/include/easy/profiler.h
        ${EASY_ROOT}/include/easy/reader.h
        ${EASY_ROOT}/include/easy/easy_net.h
        ${EASY_ROOT}/include/easy/easy_socket.h
        ${EASY_ROOT}/include/easy/easy_compiler_support.h
        ${EASY_ROOT}/include/easy/profiler_aux.h
        ${EASY_ROOT}/include/easy/profiler_colors.h
        ${EASY_ROOT}/include/easy/reader.h
        ${EASY_ROOT}/include/easy/serialized_block.h
    )
    source_group(include FILES ${INCLUDE_FILES})
endif(DEFINED EASY_ROOT)

if(NOT DEFINED EASY_PRODUCT_VERSION_STRING)
    file (STRINGS ${CMAKE_CURRENT_LIST_DIR}/version.info EASY_PRODUCT_VERSION_STRING)
    string(REPLACE "." ";" VERSION_LIST ${EASY_PRODUCT_VERSION_STRING})

    list(GET VERSION_LIST 0 EASY_PROGRAM_VERSION_MAJOR)
    list(GET VERSION_LIST 1 EASY_PROGRAM_VERSION_MINOR)
    list(GET VERSION_LIST 2 EASY_PROGRAM_VERSION_PATCH)

    message(STATUS "EASY_PROGRAM_VERSION_MAJOR: ${EASY_PROGRAM_VERSION_MAJOR}")
    message(STATUS "EASY_PROGRAM_VERSION_MINOR: ${EASY_PROGRAM_VERSION_MINOR}")
    message(STATUS "EASY_PROGRAM_VERSION_PATCH: ${EASY_PROGRAM_VERSION_PATCH}")

    # EasyProfiler version
    add_definitions(
        -DEASY_PROFILER_VERSION_MAJOR=${EASY_PROGRAM_VERSION_MAJOR}
        -DEASY_PROFILER_VERSION_MINOR=${EASY_PROGRAM_VERSION_MINOR}
        -DEASY_PROFILER_VERSION_PATCH=${EASY_PROGRAM_VERSION_PATCH}
        -DEASY_PROFILER_PRODUCT_VERSION=\"v${EASY_PRODUCT_VERSION_STRING}\"
    )
    # EasyProfiler version

    set(EASY_PROGRAM_VERSION_MAJOR ${EASY_PROGRAM_VERSION_MAJOR} PARENT_SCOPE)
    set(EASY_PROGRAM_VERSION_MINOR ${EASY_PROGRAM_VERSION_MINOR} PARENT_SCOPE)
    set(EASY_PROGRAM_VERSION_PATCH ${EASY_PROGRAM_VERSION_PATCH} PARENT_SCOPE)
    set(EASY_PRODUCT_VERSION_STRING ${EASY_PRODUCT_VERSION_STRING} PARENT_SCOPE)

endif(NOT DEFINED EASY_PRODUCT_VERSION_STRING)

set(SOURCES
    ${CPP_FILES}
    ${H_FILES}
    ${INCLUDE_FILES}
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
