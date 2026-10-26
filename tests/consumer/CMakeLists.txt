cmake_minimum_required(VERSION 3.5...${CMAKE_VERSION})
project(easy_profiler_install_consumer LANGUAGES CXX)

find_package(easy_profiler 3.0 CONFIG REQUIRED)

add_executable(easy_profiler_install_consumer main.cpp)
target_link_libraries(easy_profiler_install_consumer PRIVATE
    easy_profiler::easy_profiler
)
set_target_properties(easy_profiler_install_consumer PROPERTIES
    CXX_STANDARD 11
    CXX_STANDARD_REQUIRED ON
)

# Windows does not use an executable RPATH. Put the imported runtime next to
# the smoke-test executable so the shared-library configuration can run too.
if (WIN32)
    add_custom_command(TARGET easy_profiler_install_consumer POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:easy_profiler::easy_profiler>
            $<TARGET_FILE_DIR:easy_profiler_install_consumer>
        VERBATIM
    )
endif ()

# Building the external project also runs the executable, proving that the
# exported link interface and the installed runtime are usable.
add_custom_target(run_consumer
    COMMAND $<TARGET_FILE:easy_profiler_install_consumer>
    DEPENDS easy_profiler_install_consumer
    VERBATIM
)
