
add_executable(profiler_reader main.cpp)
target_link_libraries(profiler_reader easy_profiler)

install(
    TARGETS
    profiler_reader
    RUNTIME
    DESTINATION
    bin
)

set_property(TARGET profiler_reader PROPERTY INSTALL_RPATH_USE_LINK_PATH TRUE)
