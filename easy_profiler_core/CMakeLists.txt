project(easy_profiler)

IF (NOT DEFINED LIB_NAME)
    SET(LIB_NAME ${PROJECT_NAME})
ENDIF()


if(NOT DEFINED EASY_PROGRAM_VERSION_MAJOR)
    file (STRINGS ${CMAKE_CURRENT_LIST_DIR}/version.info EASY_PRODUCT_VERSION_STRING)
    string(REPLACE "." ";" VERSION_LIST ${EASY_PRODUCT_VERSION_STRING})

    list(GET VERSION_LIST 0 EASY_PROGRAM_VERSION_MAJOR)
    list(GET VERSION_LIST 1 EASY_PROGRAM_VERSION_MINOR)
    list(GET VERSION_LIST 2 EASY_PROGRAM_VERSION_PATCH)

    # EasyProfiler version
    add_definitions(
        -DEASY_PROFILER_VERSION_MAJOR=${EASY_PROGRAM_VERSION_MAJOR}
        -DEASY_PROFILER_VERSION_MINOR=${EASY_PROGRAM_VERSION_MINOR}
        -DEASY_PROFILER_VERSION_PATCH=${EASY_PROGRAM_VERSION_PATCH}
    )
    # EasyProfiler version

    set(EASY_PROGRAM_VERSION_MAJOR ${EASY_PROGRAM_VERSION_MAJOR} PARENT_SCOPE)
    set(EASY_PROGRAM_VERSION_MINOR ${EASY_PROGRAM_VERSION_MINOR} PARENT_SCOPE)
    set(EASY_PROGRAM_VERSION_PATCH ${EASY_PROGRAM_VERSION_PATCH} PARENT_SCOPE)

endif(NOT DEFINED EASY_PROGRAM_VERSION_MAJOR)

message(STATUS "")
message(STATUS "EASY_PROFILER.Core version = ${EASY_PROGRAM_VERSION_MAJOR}.${EASY_PROGRAM_VERSION_MINOR}.${EASY_PROGRAM_VERSION_PATCH}")
message(STATUS "")


# EasyProfiler options:----------------------------------------------
set(EASY_DEFAULT_PORT 28077) # default listening port
set(EASY_OPTION_LISTEN OFF) # enable automatic startListen on startup
set(EASY_OPTION_PROFILE_SELF OFF) # enable self profiling (measure time for internal storage expand)
set(EASY_OPTION_PROFILE_SELF_BLOCKS_ON OFF) # storage expand default status (profiler::ON or profiler::OFF)
set(EASY_OPTION_LOG OFF) # print errors to stderr

if(WIN32)
 set(EASY_OPTION_EVENT_TRACING ON) # Enable event tracing by default
 set(EASY_OPTION_LOW_PRIORITY_EVENT_TRACING ON) # Set low priority for event tracing thread
endif(WIN32)

MESSAGE(STATUS "EASY_PROFILER OPTIONS:--------------")
MESSAGE(STATUS "  Default listening port\t\t= ${EASY_DEFAULT_PORT}")
MESSAGE(STATUS "  Auto-start listening\t\t= ${EASY_OPTION_LISTEN}")
MESSAGE(STATUS "  Profile self\t\t\t= ${EASY_OPTION_PROFILE_SELF}")
MESSAGE(STATUS "  Profile self blocks initial status\t= ${EASY_OPTION_PROFILE_SELF_BLOCKS_ON}")
if(WIN32)
 MESSAGE(STATUS "  Event tracing\t\t\t= ${EASY_OPTION_EVENT_TRACING}")
 if(EASY_OPTION_LOW_PRIORITY_EVENT_TRACING)
  MESSAGE(STATUS "  Event tracing has low priority\t= Yes")
 else()
  MESSAGE(STATUS "  Event tracing has low priority\t= No")
 endif(EASY_OPTION_LOW_PRIORITY_EVENT_TRACING)
endif(WIN32)
MESSAGE(STATUS "  Print errors to stderr\t\t= ${EASY_OPTION_LOG}")
MESSAGE(STATUS "END EASY_PROFILER OPTIONS.----------")
MESSAGE(STATUS "")
# END EasyProfiler options.~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~


add_definitions(-DEASY_DEFAULT_PORT=${EASY_DEFAULT_PORT})
if(EASY_OPTION_LISTEN)
 add_definitions(-DEASY_OPTION_START_LISTEN_ON_STARTUP=1)
else()
 add_definitions(-DEASY_OPTION_START_LISTEN_ON_STARTUP=0)
endif(EASY_OPTION_LISTEN)

if(EASY_OPTION_PROFILE_SELF)
 add_definitions(-DEASY_OPTION_MEASURE_STORAGE_EXPAND=1)
 if(EASY_OPTION_PROFILE_SELF_BLOCKS_ON)
  add_definitions(-DEASY_OPTION_STORAGE_EXPAND_BLOCKS_ON=true)
 else()
  add_definitions(-DEASY_OPTION_STORAGE_EXPAND_BLOCKS_ON=false)
 endif(EASY_OPTION_PROFILE_SELF_BLOCKS_ON)
else()
 add_definitions(-DEASY_OPTION_MEASURE_STORAGE_EXPAND=0)
endif(EASY_OPTION_PROFILE_SELF)

if(WIN32)
 if(EASY_OPTION_EVENT_TRACING)
  add_definitions(-DEASY_OPTION_EVENT_TRACING_ENABLED=true)
 else()
  add_definitions(-DEASY_OPTION_EVENT_TRACING_ENABLED=false)
 endif(EASY_OPTION_EVENT_TRACING)
 if(EASY_OPTION_LOW_PRIORITY_EVENT_TRACING)
  add_definitions(-DEASY_OPTION_LOW_PRIORITY_EVENT_TRACING=true)
 else()
  add_definitions(-DEASY_OPTION_LOW_PRIORITY_EVENT_TRACING=false)
 endif(EASY_OPTION_LOW_PRIORITY_EVENT_TRACING)
endif(WIN32)

if(EASY_OPTION_LOG)
 add_definitions(-DEASY_OPTION_LOG_ENABLED=1)
else()
 add_definitions(-DEASY_OPTION_LOG_ENABLED=0)
endif(EASY_OPTION_LOG)

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
include_directories(
	include
)

set(INCLUDE_FILES
	include/easy/profiler.h
	include/easy/reader.h
	include/easy/easy_net.h
	include/easy/easy_socket.h
	include/easy/easy_compiler_support.h
	include/easy/profiler_aux.h
	include/easy/profiler_colors.h
	include/easy/reader.h
	include/easy/serialized_block.h
)
source_group(include FILES ${INCLUDE_FILES})

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

if (CMAKE_VERSION VERSION_LESS "3.1")
    if (UNIX)
      set (CMAKE_CXX_FLAGS "-std=gnu++11 ${CMAKE_CXX_FLAGS}")
    endif (UNIX)
  else ()
    set (CMAKE_CXX_STANDARD 11)
endif (CMAKE_VERSION VERSION_LESS "3.1")

if(WIN32)
    add_definitions(
        -D_WINSOCK_DEPRECATED_NO_WARNINGS
		-D_CRT_SECURE_NO_WARNINGS
    )
endif(WIN32)

add_library(${LIB_NAME} SHARED ${SOURCES} resources.rc)

if(UNIX)
    set(PLATFORM_LIBS pthread)
endif(UNIX)

target_link_libraries(${LIB_NAME} ${PLATFORM_LIBS})
