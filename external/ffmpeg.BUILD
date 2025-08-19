cc_library(
    name = "ffmpeg",
    hdrs = glob([
        "libavcodec/*.h",
        "libavformat/*.h",
        "libavutil/*.h",
        "libswscale/*.h",
        "libswresample/*.h",
        "libavfilter/*.h"
    ]),
    includes = ["/usr/include/x86_64-linux-gnu"],
    linkopts = [
        "-lavcodec",
        "-lavformat",
        "-lavutil",
        "-lswscale",
        "-lswresample",
        "-lavfilter"
    ],
    visibility = ["//visibility:public"],
)