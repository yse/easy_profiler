cc_library(
    name = "easy_profiler",
    srcs = glob(["**/*.cpp", "**/*.h"]),
    hdrs = glob(["include/**"]),
    includes = ["include", "."],
    visibility = ["//visibility:public"],
    defines = [
        "EASY_PROFILER_VERSION_MAJOR=2",
        "EASY_PROFILER_VERSION_MINOR=1",
        "EASY_PROFILER_VERSION_PATCH=0",
        "BUILD_WITH_EASY_PROFILER=1",
    ]
)