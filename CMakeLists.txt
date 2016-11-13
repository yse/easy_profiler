project(profiling_tool)
cmake_minimum_required(VERSION 2.8)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

set(
    ROOT
    ${CMAKE_CURRENT_LIST_DIR}
)

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

install(FILES
LICENSE.APACHE
LICENSE.GPL3
DESTINATION .
)

set(CPACK_PACKAGE_NAME "easy_profiler")
set(CPACK_PACKAGE_RELEASE 1)
set(CPACK_PACKAGE_CONTACT "Easy Profiler Team")
set(CPACK_PACKAGE_VENDOR "Easy Profiler Team")
if(UNIX)
	set(CPACK_GENERATOR "TGZ")
else()
	set(CPACK_GENERATOR "ZIP")
endif(UNIX)

set(CPACK_PACKAGE_VERSION "v1.0.0")
include(CPack)
