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
    if (CMAKE_VERSION VERSION_LESS 3.11)
        message(FATAL_ERROR
            "Catch2 3.x was not found. CMake 3.11+ is required to fetch it; "
            "install Catch2 or configure with a newer CMake.")
    endif ()

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
    cmake_parse_arguments(ARG "NO_DISCOVERY" "TIMEOUT" "SOURCES;DEFINES;LABELS" ${ARGN})

    if (NOT ARG_TIMEOUT)
        set(ARG_TIMEOUT 30)
    endif ()

    add_executable(${target} ${ARG_SOURCES})

    target_link_libraries(${target} PRIVATE easy_profiler Catch2::Catch2WithMain)

    target_include_directories(${target} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${PROJECT_SOURCE_DIR}/easy_profiler_core
    )

    target_compile_definitions(${target} PRIVATE
        EASY_TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
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
    if (NOT ARG_NO_DISCOVERY)
        # catch_discover_tests treats PROPERTIES as a flat list, so a CMake
        # semicolon would become another property name. A comma keeps the
        # labels in one value; CTest's -L regex still matches each component.
        string(REPLACE ";" "," EASY_TEST_LABELS "${ARG_LABELS}")
        catch_discover_tests(${target}
            PROPERTIES
                LABELS "${EASY_TEST_LABELS}"
                TIMEOUT ${ARG_TIMEOUT}
        )
    endif ()
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
        test_description_stream.cpp
        test_writer.cpp
        test_legacy_formats.cpp
        test_golden_profiles.cpp
        test_context_switch.cpp
        test_reader_types.cpp
)

# Threading is split out: these tests are slower and noisier than the rest.
easy_add_test_target(easy_profiler_thread_tests
    LABELS "unit;threads"
    TIMEOUT 60
    SOURCES
        test_threads.cpp
        test_spin_lock.cpp
        test_reader_thread_pool.cpp
)

# Interposing pthread_create replaces the symbol for a whole process, so the one
# test that needs a failing std::thread gets a target to itself. The interposer
# is a shared library: defined in the executable it would not reach a shared
# easy_profiler on macOS (see support/thread_failure_shim.cpp). Windows has no
# equivalent seam.
if (NOT WIN32)
    add_library(easy_profiler_thread_failure_shim SHARED support/thread_failure_shim.cpp)
    target_link_libraries(easy_profiler_thread_failure_shim PRIVATE ${CMAKE_DL_LIBS})
    set_target_properties(easy_profiler_thread_failure_shim PROPERTIES
        CXX_STANDARD 11
        CXX_STANDARD_REQUIRED ON
        FOLDER tests
    )

    # 20000 descriptors are loaded twice with statistics: 36-43 s under TSan.
    easy_add_test_target(easy_profiler_thread_failure_tests
        LABELS "unit;threads"
        TIMEOUT 120
        SOURCES
            test_reader_thread_failure.cpp
    )
    target_link_libraries(easy_profiler_thread_failure_tests PRIVATE easy_profiler_thread_failure_shim)
endif ()

# Parsing corrupt .prof files. Crashes are possible here, hence a separate binary.
easy_add_test_target(easy_profiler_robustness_tests
    LABELS "robustness"
    SOURCES
        test_reader_robustness.cpp
)

# The network layer: loopback sockets and protocol structure layout.
easy_add_test_target(easy_profiler_net_tests
    LABELS "net;threads"
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

# Regression tests for defects found while reviving the project. These belong
# to the normal green suite: every fixed defect remains covered permanently.
easy_add_test_target(easy_profiler_regression_tests
    LABELS "regression"
    SOURCES
        test_known_issues.cpp
)

# Coverage runs all enabled-core cases in one process and therefore produces a
# single compatible LLVM profile. The regular split targets above remain the
# authoritative CTest suite for crash isolation and per-case timeouts.
option(EASY_PROFILER_BUILD_COVERAGE_SUITE "Build the single-process coverage runner" OFF)
if (EASY_PROFILER_BUILD_COVERAGE_SUITE)
    get_target_property(EASY_COVERAGE_UNIT_SOURCES easy_profiler_unit_tests SOURCES)
    get_target_property(EASY_COVERAGE_THREAD_SOURCES easy_profiler_thread_tests SOURCES)
    get_target_property(EASY_COVERAGE_ROBUSTNESS_SOURCES easy_profiler_robustness_tests SOURCES)
    get_target_property(EASY_COVERAGE_NET_SOURCES easy_profiler_net_tests SOURCES)
    get_target_property(EASY_COVERAGE_REGRESSION_SOURCES easy_profiler_regression_tests SOURCES)

    easy_add_test_target(easy_profiler_coverage_tests
        NO_DISCOVERY
        LABELS "coverage"
        SOURCES
            ${EASY_COVERAGE_UNIT_SOURCES}
            ${EASY_COVERAGE_THREAD_SOURCES}
            ${EASY_COVERAGE_ROBUSTNESS_SOURCES}
            ${EASY_COVERAGE_NET_SOURCES}
            ${EASY_COVERAGE_REGRESSION_SOURCES}
    )
    file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/coverage-profiles)
    add_test(NAME easy_profiler.coverage_suite
        COMMAND easy_profiler_coverage_tests "~[isolated]" --order lex
    )
    set_tests_properties(easy_profiler.coverage_suite PROPERTIES
        LABELS "coverage"
        TIMEOUT 180
        ENVIRONMENT
            "LLVM_PROFILE_FILE=${CMAKE_CURRENT_BINARY_DIR}/coverage-profiles/core.profraw"
    )
endif ()

option(EASY_PROFILER_BUILD_FUZZER "Build the libFuzzer harness for the .prof reader" OFF)
if (EASY_PROFILER_BUILD_FUZZER)
    if (NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        message(FATAL_ERROR
            "EASY_PROFILER_BUILD_FUZZER requires upstream LLVM Clang with libFuzzer "
            "(AppleClang does not ship the runtime)")
    endif ()

    add_executable(easy_profiler_reader_fuzzer fuzz_reader.cpp)
    target_link_libraries(easy_profiler_reader_fuzzer PRIVATE easy_profiler)
    target_compile_options(easy_profiler_reader_fuzzer PRIVATE -fsanitize=fuzzer)
    set_property(TARGET easy_profiler_reader_fuzzer APPEND_STRING PROPERTY
        LINK_FLAGS " -fsanitize=fuzzer")
    set_target_properties(easy_profiler_reader_fuzzer PROPERTIES
        CXX_STANDARD 11
        CXX_STANDARD_REQUIRED ON
        FOLDER tests
    )

    file(COPY fuzz_corpus DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
    file(COPY
        ${CMAKE_CURRENT_SOURCE_DIR}/golden/v1_3.prof.hex
        ${CMAKE_CURRENT_SOURCE_DIR}/golden/v2_0.prof.hex
        ${CMAKE_CURRENT_SOURCE_DIR}/golden/v2_1.prof.hex
        DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/fuzz_corpus
    )

    add_test(NAME easy_profiler.reader_fuzzer_smoke
        COMMAND easy_profiler_reader_fuzzer
            -runs=1000
            -max_len=4096
            -dict=${CMAKE_CURRENT_SOURCE_DIR}/fuzz_reader.dict
            ${CMAKE_CURRENT_BINARY_DIR}/fuzz_corpus
    )
    set_tests_properties(easy_profiler.reader_fuzzer_smoke PROPERTIES
        LABELS "fuzz;robustness"
        TIMEOUT 60
    )
endif ()

#####################################################################
# Build-system contract tests. They deliberately configure fresh projects so
# an in-tree target cannot hide a broken install export or parent-project leak.
#####################################################################
configure_file(
    cmake/run_install_consumer.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/run_install_consumer.cmake
    @ONLY
)
add_test(NAME easy_profiler.install_consumer
    COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/run_install_consumer.cmake
)
set_tests_properties(easy_profiler.install_consumer PROPERTIES
    LABELS "package"
    TIMEOUT 180
    RUN_SERIAL TRUE
)

configure_file(
    cmake/run_subdirectory_consumer.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/run_subdirectory_consumer.cmake
    @ONLY
)
add_test(NAME easy_profiler.subdirectory_consumer
    COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/run_subdirectory_consumer.cmake
)
set_tests_properties(easy_profiler.subdirectory_consumer PROPERTIES
    LABELS "package"
    TIMEOUT 180
    RUN_SERIAL TRUE
)
