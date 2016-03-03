project(easy_profiler)

set(CPP_FILES
	block.cpp
	profile_manager.cpp
)

set(H_FILES
	${ROOT}/include/profiler/profiler.h
	profile_manager.h
	spin_lock.h
	${ROOT}/include/profiler/color.h
)

set(SOURCES
	${CPP_FILES}
	${H_FILES}
)
add_definitions(
	-D_BUILD_PROFILER
)
add_library(${PROJECT_NAME} SHARED ${SOURCES})

