load("@rules_cc//cc:cc_library.bzl", "cc_library")

config_setting(
    name = "windows",
    constraint_values = ["@platforms//os:windows"],
)

cc_library(
    name = "easy_profiler",
    srcs = [
        "alignment_helpers.h",
        "base_block_descriptor.cpp",
        "block.cpp",
        "block_descriptor.cpp",
        "block_descriptor.h",
        "chunk_allocator.h",
        "current_thread.h",
        "current_time.h",
        "easy_socket.cpp",
        "event_trace_status.h",
        "event_trace_win.h",
        "hashed_cstr.h",
        "nonscoped_block.cpp",
        "nonscoped_block.h",
        "profile_manager.cpp",
        "profile_manager.h",
        "profiler.cpp",
        "reader.cpp",
        "serialized_block.cpp",
        "spin_lock.h",
        "stack_buffer.h",
        "thread_storage.cpp",
        "thread_storage.h",
        "timestamp_scaling.h",
        "writer.cpp",
    ] + select({
        ":windows": [
            "event_trace_win.cpp",
            "resources.rc",
        ],
        "//conditions:default": [],
    }),
    hdrs = [
        "include/easy/arbitrary_value.h",
        "include/easy/details/arbitrary_value_aux.h",
        "include/easy/details/arbitrary_value_public_types.h",
        "include/easy/details/easy_compiler_support.h",
        "include/easy/details/profiler_aux.h",
        "include/easy/details/profiler_colors.h",
        "include/easy/details/profiler_in_use.h",
        "include/easy/details/profiler_public_types.h",
        "include/easy/easy_net.h",
        "include/easy/easy_socket.h",
        "include/easy/profiler.h",
        "include/easy/reader.h",
        "include/easy/serialized_block.h",
        "include/easy/utility.h",
        "include/easy/writer.h",
    ],
    defines = [
        "BUILD_WITH_EASY_PROFILER=1",
        "EASY_PROFILER_STATIC=1",
        "EASY_PROFILER_VERSION_MAJOR=3",
        "EASY_PROFILER_VERSION_MINOR=0",
        "EASY_PROFILER_VERSION_PATCH=0",
    ],
    includes = ["include"],
    local_defines = ["_BUILD_PROFILER=1"] + select({
        ":windows": [
            "_CRT_SECURE_NO_WARNINGS",
            "_WIN32_WINNT=0x0600",
            "_WINSOCK_DEPRECATED_NO_WARNINGS",
        ],
        "//conditions:default": [],
    }),
    linkopts = select({
        ":windows": [
            "psapi.lib",
            "ws2_32.lib",
        ],
        "//conditions:default": ["-pthread"],
    }),
    visibility = ["//visibility:public"],
)
