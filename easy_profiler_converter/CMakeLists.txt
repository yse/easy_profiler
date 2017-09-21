set(CPP_FILES
    converter.cpp
    reader.cpp)

set(HEADER_FILES
    converter.h
    reader.h)

add_executable(profiler_converter ${HEADER_FILES} ${CPP_FILES} main.cpp)
target_link_libraries(profiler_converter easy_profiler)
