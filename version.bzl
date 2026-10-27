# Bazel cannot read the VERSION file: MODULE.bazel takes no load() and a .bzl
# file cannot read the workspace at load time. These values repeat VERSION and
# the easy_profiler.version_consistency test fails when they drift apart.

EASY_VERSION_MAJOR = 3
EASY_VERSION_MINOR = 0
EASY_VERSION_PATCH = 0

EASY_VERSION = "%d.%d.%d" % (EASY_VERSION_MAJOR, EASY_VERSION_MINOR, EASY_VERSION_PATCH)

EASY_VERSION_DEFINES = [
    "EASY_PROFILER_VERSION_MAJOR=%d" % EASY_VERSION_MAJOR,
    "EASY_PROFILER_VERSION_MINOR=%d" % EASY_VERSION_MINOR,
    "EASY_PROFILER_VERSION_PATCH=%d" % EASY_VERSION_PATCH,
]
