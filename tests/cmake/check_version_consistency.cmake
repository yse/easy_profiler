# Compares every place the product version is written against the VERSION file.
#
# Run with -DEASY_SOURCE_DIR=<repo>. The checks here hold on every commit.
# Release-only checks (the tag name, the released CHANGELOG heading, the stable
# README badge) live in the release workflow because between releases the
# development version and the last released version differ on purpose.

if (NOT DEFINED EASY_SOURCE_DIR)
    message(FATAL_ERROR "EASY_SOURCE_DIR must be set")
endif ()

set(failures "")

macro(easy_fail MESSAGE)
    list(APPEND failures "${MESSAGE}")
endmacro()

file(STRINGS "${EASY_SOURCE_DIR}/VERSION" version LIMIT_COUNT 1)
if (NOT version MATCHES "^([0-9]+)\\.([0-9]+)\\.([0-9]+)$")
    message(FATAL_ERROR "VERSION must hold a single MAJOR.MINOR.PATCH line, found '${version}'")
endif ()
set(major ${CMAKE_MATCH_1})
set(minor ${CMAKE_MATCH_2})
set(patch ${CMAKE_MATCH_3})

# Bazel module version.
file(READ "${EASY_SOURCE_DIR}/MODULE.bazel" module_bazel)
if (NOT module_bazel MATCHES "version[ \t]*=[ \t]*\"([^\"]*)\"")
    easy_fail("MODULE.bazel has no module version")
elseif (NOT CMAKE_MATCH_1 STREQUAL version)
    easy_fail("MODULE.bazel version is '${CMAKE_MATCH_1}', VERSION is '${version}'")
endif ()

# Bazel compile definitions.
file(READ "${EASY_SOURCE_DIR}/version.bzl" version_bzl)
foreach (part MAJOR MINOR PATCH)
    if (part STREQUAL "MAJOR")
        set(expected ${major})
    elseif (part STREQUAL "MINOR")
        set(expected ${minor})
    else ()
        set(expected ${patch})
    endif ()
    if (NOT version_bzl MATCHES "EASY_VERSION_${part}[ \t]*=[ \t]*([0-9]+)")
        easy_fail("version.bzl does not define EASY_VERSION_${part}")
    elseif (NOT CMAKE_MATCH_1 STREQUAL expected)
        easy_fail("version.bzl EASY_VERSION_${part} is ${CMAKE_MATCH_1}, VERSION says ${expected}")
    endif ()
endforeach ()

# The wire format version is a property of the reader and writer code. No build
# system may define it, or a release could change the on-disk format silently.
foreach (build_file CMakeLists.txt easy_profiler_core/CMakeLists.txt
                    easy_profiler_core/BUILD version.bzl MODULE.bazel)
    file(READ "${EASY_SOURCE_DIR}/${build_file}" contents)
    if (contents MATCHES "EASY_PROFILER_FORMAT_VERSION_(MAJOR|MINOR|PATCH)")
        easy_fail("${build_file} defines EASY_PROFILER_FORMAT_VERSION_*; it belongs only in profiler.cpp")
    endif ()
endforeach ()

# The changelog must have a section for the version under development.
file(READ "${EASY_SOURCE_DIR}/CHANGELOG.md" changelog)
if (NOT changelog MATCHES "(^|\n)## ${major}\\.${minor}\\.${patch}([ \t]|\n|$)")
    easy_fail("CHANGELOG.md has no '## ${version}' section")
endif ()

# The development badge tracks VERSION; the stable badge names the last
# release and is checked only when a tag is being built. Both the badge alt
# text and the shields.io URL spell the version, so every occurrence counts.
file(READ "${EASY_SOURCE_DIR}/README.md" readme)
string(REGEX MATCHALL "development[ -]([0-9]+\\.[0-9]+\\.[0-9]+)" development_badges "${readme}")
if (NOT development_badges)
    easy_fail("README.md has no development version badge")
endif ()
foreach (badge IN LISTS development_badges)
    string(REGEX REPLACE "^development[ -]" "" found "${badge}")
    if (NOT found STREQUAL version)
        easy_fail("README.md says 'development ${found}', VERSION is '${version}'")
    endif ()
endforeach ()

# Release-only, passed by the release workflow when a tag is being built: the
# stable badge must have been moved to the version going out.
if (DEFINED EASY_RELEASED_VERSION)
    string(REGEX MATCHALL "stable[ -]([0-9]+\\.[0-9]+\\.[0-9]+)" stable_badges "${readme}")
    if (NOT stable_badges)
        easy_fail("README.md has no stable version badge")
    endif ()
    foreach (badge IN LISTS stable_badges)
        string(REGEX REPLACE "^stable[ -]" "" found "${badge}")
        if (NOT found STREQUAL EASY_RELEASED_VERSION)
            easy_fail("README.md says 'stable ${found}', this release is '${EASY_RELEASED_VERSION}'")
        endif ()
    endforeach ()
endif ()

if (failures)
    string(REPLACE ";" "\n  " report "${failures}")
    message(FATAL_ERROR "version is not consistent with VERSION (${version}):\n  ${report}")
endif ()

message(STATUS "version ${version} is consistent across CMake, Bazel, CHANGELOG.md and README.md")
