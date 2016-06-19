project(profiler_reader)

set(CPP_FILES
	main.cpp
)

set(SOURCES
	${CPP_FILES}
)

add_definitions(
	#-DFULL_DISABLE_PROFILER
)
add_executable(${PROJECT_NAME} ${SOURCES})

if(UNIX)
	set(SPEC_LIB pthread)
endif(UNIX)

target_link_libraries(${PROJECT_NAME} easy_profiler ${SPEC_LIB})