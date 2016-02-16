project(profiler_sample)

set(CPP_FILES
	main.cpp
)

set(SOURCES
	${CPP_FILES}
)

add_executable(${PROJECT_NAME} ${SOURCES})

target_link_libraries(${PROJECT_NAME} easy_profiler)