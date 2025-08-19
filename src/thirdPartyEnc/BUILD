cc_library(
    name = "FfmpegMuxer",
    srcs = [
        "FfmpegMuxer.cpp",
        "muxer_queue.c"
    ],
    hdrs = [
        "CircularBuffer.h",
        "FfmpegMuxer.h",
        "muxer_queue.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "@myffmpeg//:ffmpeg",
        "//include/log:log"
    ]
)