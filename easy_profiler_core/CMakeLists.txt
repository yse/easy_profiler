cmake_minimum_required(VERSION 2.8)
project(easy_profiler CXX)

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
set(EASY_DEFAULT_PORT 28077) # Default listening port
set(EASY_OPTION_LISTEN OFF) # Enable automatic startListen on startup
set(EASY_OPTION_PROFILE_SELF OFF) # Enable self profiling (measure time for internal storage expand)
set(EASY_OPTION_PROFILE_SELF_BLOCKS_ON OFF) # Storage expand default status (profiler::ON or profiler::OFF)
set(EASY_OPTION_LOG OFF) # Print errors to stderr
set(EASY_OPTION_PREDEFINED_COLORS ON) # Use predefined set of colors (see profiler_colors.h)
                                      # If you want to use your own colors palette you can turn this option OFF

if(WIN32)
 set(EASY_OPTION_EVENT_TRACING ON) # Enable event tracing by default
 set(EASY_OPTION_LOW_PRIORITY_EVENT_TRACING ON) # Set low priority for event tracing thread
endif(WIN32)

MESSAGE(STATUS "EASY_PROFILER OPTIONS:--------------")
MESSAGE(STATUS "  Default listening port = ${EASY_DEFAULT_PORT}")
MESSAGE(STATUS "  Auto-start listening = ${EASY_OPTION_LISTEN}")
MESSAGE(STATUS "  Profile self = ${EASY_OPTION_PROFILE_SELF}")
MESSAGE(STATUS "  Profile self blocks initial status = ${EASY_OPTION_PROFILE_SELF_BLOCKS_ON}")
if(WIN32)
 MESSAGE(STATUS "  Event tracing = ${EASY_OPTION_EVENT_TRACING}")
 if(EASY_OPTION_LOW_PRIORITY_EVENT_TRACING)
  MESSAGE(STATUS "  Event tracing has low priority = Yes")
 else()
  MESSAGE(STATUS "  Event tracing has low priority = No")
 endif(EASY_OPTION_LOW_PRIORITY_EVENT_TRACING)
endif(WIN32)
MESSAGE(STATUS "  Log messages = ${EASY_OPTION_LOG}")
MESSAGE(STATUS "  Use EasyProfiler colors palette = ${EASY_OPTION_PREDEFINED_COLORS}")
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

if(EASY_OPTION_PREDEFINED_COLORS)
 add_definitions(-DEASY_OPTION_BUILTIN_COLORS=1)
else()
 add_definitions(-DEASY_OPTION_BUILTIN_COLORS=0)
endif(EASY_OPTION_PREDEFINED_COLORS)

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

if(WIN32)
    add_definitions(
        -D_WINSOCK_DEPRECATED_NO_WARNINGS
		-D_CRT_SECURE_NO_WARNINGS
    )
endif(WIN32)

add_library(${LIB_NAME} SHARED ${SOURCES} resources.rc)

if (MINGW)
    add_definitions(
        -D_WIN32_WINNT=0x0600
        -DSTRSAFE_NO_DEPRECATE
    )
    set (PLATFORM_LIBS ${PLATFORM_LIBS} ws2_32 psapi)
    set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
elseif (CMAKE_VERSION VERSION_LESS "3.1")
    if (UNIX OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        set (CMAKE_CXX_FLAGS "-std=gnu++11 ${CMAKE_CXX_FLAGS}")
    endif (UNIX OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
else ()
    set_target_properties(${LIB_NAME} PROPERTIES
        CXX_STANDARD 11
        CXX_STANDARD_REQUIRED ON
    )
endif (MINGW)

if(UNIX)
    set(PLATFORM_LIBS ${PLATFORM_LIBS} pthread)
endif(UNIX)

target_link_libraries(${LIB_NAME} ${PLATFORM_LIBS})

####
# Installation 
set(config_install_dir "lib/cmake/${PROJECT_NAME}")
set(include_install_dir "include")

set(generated_dir "${CMAKE_CURRENT_BINARY_DIR}/generated")

# Configuration
set(version_config "${generated_dir}/${PROJECT_NAME}ConfigVersion.cmake")
set(project_config "${generated_dir}/${PROJECT_NAME}Config.cmake")
set(targets_export_name "${PROJECT_NAME}Targets")

include(CMakePackageConfigHelpers)
include(InstallRequiredSystemLibraries)

write_basic_package_version_file(
    "${version_config}" 
	VERSION
	${EASY_PRODUCT_VERSION_STRING}
	COMPATIBILITY 
	SameMajorVersion
)

configure_package_config_file(
    "cmake/config.cmake.in"
    "${project_config}"
    INSTALL_DESTINATION "${config_install_dir}"
)

install(
    FILES "${project_config}" "${version_config}"
    DESTINATION "${config_install_dir}"
)

install(
    FILES
    ${INCLUDE_FILES}
    DESTINATION
    include/easy
)

install(
    FILES
    version.info
    LICENSE.MIT
    LICENSE.APACHE
    DESTINATION
    .
)

install(
    TARGETS
    ${LIB_NAME}
	EXPORT 
	${targets_export_name}
    DESTINATION
    bin
	INCLUDES DESTINATION "${include_install_dir}"
)

install(
    EXPORT "${targets_export_name}"
    DESTINATION "${config_install_dir}"
)

target_compile_definitions(easy_profiler PUBLIC BUILD_WITH_EASY_PROFILER)
