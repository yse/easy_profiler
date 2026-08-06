#####################################################################
# easy_profiler core tests.
#
# Framework: Catch2 v3. It requires C++14, so the test targets are built as
# C++17 while the easy_profiler library itself stays on C++11 — a deliberate
# divergence: the standard is raised for the tests ONLY.
#
# Catch2 is taken from the system when available, otherwise fetched with
# FetchContent. To build offline, install Catch2 3.x system-wide or set
# -DFETCHCONTENT_FULLY_DISCONNECTED=ON with a pre-populated cache.
#####################################################################

set(EASY_TESTS_CATCH2_VERSION "v3.7.1" CACHE STRING "Catch2 tag for FetchContent")

find_package(Catch2 3 QUIET)

if (NOT Catch2_FOUND)
    message(STATUS "Catch2 3.x not found on the system, fetching ${EASY_TESTS_CATCH2_VERSION}")

    include(FetchContent)
    FetchContent_Declare(
        Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG        ${EASY_TESTS_CATCH2_VERSION}
        GIT_SHALLOW    TRUE
    )

    # Catch2 must not leak into easy_profiler's install().
    set(CATCH_INSTALL_DOCS OFF CACHE BOOL "" FORCE)
    set(CATCH_INSTALL_EXTRAS OFF CACHE BOOL "" FORCE)

    FetchContent_MakeAvailable(Catch2)
    list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
else ()
    message(STATUS "Using system Catch2 ${Catch2_VERSION}")
endif ()

include(Catch)

#####################################################################
# Common test target setup.
#
# easy_profiler_core/ goes on the include path so that the header-only
# internals (chunk_allocator.h, hashed_cstr.h, alignment_helpers.h,
# spin_lock.h, stack_buffer.h) can be tested directly, with no linking and
# no symbol exports.
#####################################################################
function(easy_add_test_target target)
    cmake_parse_arguments(ARG "" "" "SOURCES;DEFINES;LABELS" ${ARGN})

    add_executable(${target} ${ARG_SOURCES})

    target_link_libraries(${target} PRIVATE easy_profiler Catch2::Catch2WithMain)

    target_include_directories(${target} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/easy_profiler_core
    )

    set_target_properties(${target} PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED ON
        FOLDER tests
    )

    if (ARG_DEFINES)
        target_compile_definitions(${target} PRIVATE ${ARG_DEFINES})
    endif ()

    if (MSVC)
        target_compile_options(${target} PRIVATE /W4)
    else ()
        target_compile_options(${target} PRIVATE -Wall -Wextra)
    endif ()

    # One CTest test per TEST_CASE: a crash in one case does not take its
    # neighbours with it.
    catch_discover_tests(${target}
        PROPERTIES LABELS "${ARG_LABELS}"
    )
endfunction()

#####################################################################
# Targets.
#####################################################################

# The deterministic core: allocator, layout, descriptors, blocks, values, format.
easy_add_test_target(easy_profiler_unit_tests
    LABELS "unit"
    SOURCES
        test_alignment.cpp
        test_chunk_allocator.cpp
        test_hashed_cstr.cpp
        test_stack_buffer.cpp
        test_layout_invariants.cpp
        test_colors_and_aux.cpp
        test_descriptors.cpp
        test_blocks_api.cpp
        test_values.cpp
        test_frame_time.cpp
        test_roundtrip.cpp
        test_tree_reconstruction.cpp
        test_bytes_per_block.cpp
        test_reader_statistics.cpp
        test_writer.cpp
        test_legacy_formats.cpp
        test_context_switch.cpp
        test_reader_types.cpp
)

# Threading is split out: these tests are slower and noisier than the rest.
easy_add_test_target(easy_profiler_thread_tests
    LABELS "unit;threads"
    SOURCES
        test_threads.cpp
        test_spin_lock.cpp
)

# Parsing corrupt .prof files. Crashes are possible here, hence a separate binary.
easy_add_test_target(easy_profiler_robustness_tests
    LABELS "robustness"
    SOURCES
        test_reader_robustness.cpp
)

# The network layer: loopback sockets and protocol structure layout.
easy_add_test_target(easy_profiler_net_tests
    LABELS "net"
    SOURCES
        test_net.cpp
        test_net_commands.cpp
)

# The fully disabled profiler path.
easy_add_test_target(easy_profiler_disabled_tests
    LABELS "unit;disabled"
    DEFINES DISABLE_EASY_PROFILER
    SOURCES
        test_disabled_api.cpp
)

# Confirmed core defects. By the maintainer's decision these tests are RED:
# they fail until the defects are fixed and serve as a checklist.
#   ctest -LE known-issue   # the ordinary run, must be green
#   ctest -L  known-issue   # the defect checklist, expected to be red
easy_add_test_target(easy_profiler_known_issues_tests
    LABELS "known-issue"
    SOURCES
        test_known_issues.cpp
)
