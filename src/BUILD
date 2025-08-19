cc_binary(
    name = "demo_ffmpeg",
    srcs = [
        "main.cpp", 
    ],
    deps = [
        ":ffmpeg_app",
    ]
)

cc_library(
    name = "ffmpeg_app",
    srcs = [
        "myffmpeg.cpp"
    ],
    hdrs = [
        "myffmpeg.h",
        "mylog.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "@myffmpeg//:ffmpeg"
    ]
)
