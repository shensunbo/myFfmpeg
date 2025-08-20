#pragma once

extern "C"{
#include "muxer_queue.h"
}

#include <string>

class FfmegH264Parser {
public:
    FfmegH264Parser() = delete;
    FfmegH264Parser(FrameQueue& queue, std::string filenameOrUrl);
    ~FfmegH264Parser();

    int h264ParserByFileRun();
    int h264ParserByRtspRun();
    void setDecoderState(bool ifStart);

private:
    FrameQueue& nal_queue;
    std::string filename_or_url;
    bool decode_start;

    const int INBUF_SIZE = 1024 * 500;
};