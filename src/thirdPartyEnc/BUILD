cc_library(
    name = "FfmpegMuxer",
    srcs = [
        "FfmpegMuxer.cpp",
    ],
    hdrs = [
        "CircularBuffer.h",
        "FfmpegMuxer.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "@myffmpeg//:ffmpeg",
        "//include/log:log",
        ":muxer_queue"
    ]
)

cc_library(
    name = "muxer_queue",
    srcs = [
        "muxer_queue.c"
    ],
    hdrs = [
        "muxer_queue.h",
    ],
    includes = ["."],
    visibility = ["//visibility:public"],
)