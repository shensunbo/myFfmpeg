#pragma once

#include "muxer_queue.h"
#include <string>

class FfmpegMuxer{
public:
    FfmpegMuxer() = delete;
    FfmpegMuxer(FrameQueue& queue, const std::string filename);
    ~FfmpegMuxer();

public:
    int FfmpegMp4Muxer();
    bool isMuxingEnd();
    void setStartRecord(){ start_record = true;}
public:
    FrameQueue& frame_queue;
    const std::string output_filename;
    bool muxing_end;

    bool start_record = false;
};

