project(profiling_tool)
cmake_minimum_required(VERSION 2.8)

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
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wno-long-long -pedantic" )
else()
	add_definitions(
		-D_CRT_SECURE_NO_WARNINGS
	)
endif(UNIX)

add_subdirectory(src)
add_subdirectory(sample)
add_subdirectory(reader)
