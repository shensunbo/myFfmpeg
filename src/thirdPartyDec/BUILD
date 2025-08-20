cc_library(
    name = "thirdPartyDec",
    srcs = [
        "FfmpegH264Parser.cpp",
    ],
    hdrs = [
        "FfmpegH264Parser.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "@myffmpeg//:ffmpeg",
        "//include/log:log",
        "//src/thirdPartyEnc:muxer_queue"
    ]
)