#pragma once
#include <string>
#include <assert.h>
#include <iostream>

#include "mylog.h"

extern "C" {
    #include <stdio.h>
    #include <stdlib.h>
    #include <math.h>
    #include <libavcodec/avcodec.h>
    #include <libavformat/avformat.h>
    #include <libavfilter/buffersink.h>
    #include <libavfilter/buffersrc.h>
    #include <libavutil/opt.h>

    #include <libavutil/avassert.h>
    #include <libavutil/channel_layout.h>
    #include <libavutil/mathematics.h>
    #include <libavutil/timestamp.h>
    #include <libswscale/swscale.h>
    #include <libswresample/swresample.h>
}

typedef struct OutputStream {
    AVStream *st;
    AVCodecContext *enc;

    /* pts of the next frame that will be generated */
    int64_t next_pts;
    int samples_count;

    AVFrame *frame;
    AVFrame *tmp_frame;

    float t, tincr, tincr2;

    struct SwsContext *sws_ctx;
    struct SwrContext *swr_ctx;
}OutputStream;

class MyFfmpeg {
    public:
        MyFfmpeg();
        MyFfmpeg(const std::string& out_filename, const std::string& in_filename, int bit_rate, int width, int height, int gop_size, AVPixelFormat pix_fmt);
        ~MyFfmpeg();

        int VideoMuxing();
        int VideoFilterAndMuxing();
        int H264Muxing();

    private:
        void MuxingInit();
        void MuxingDeinit();
        void VideoCodecInit();
        void VideoCodecDeinit();

        int FiltersInit(const char *filters_descr);
        void FiltersDeinit();

        void SetupVideoEncoder();
        AVFrame* AllocFrame(enum AVPixelFormat pix_fmt, int width, int height);
        void FreeFrame(AVFrame* frame);

        void OpenOutputFile();
        void WriteOutputFile();
        void WriteOutputFileWithFilter();
        int WriteFrame(AVFrame *frame);

    //tool
    static int get_video_frame(OutputStream *ost, std::string filename, int width, int height);
    static int get_video_frame(OutputStream *ost);
    static int ReadNv12(AVFrame *frame, FILE* fd, int width, int height);
    static void fill_yuv_image(AVFrame *pict, int frame_index, int width, int height);

    private:
        Logger m_logger;
        std::string m_out_filename;
        std::string m_in_filename;

        FILE* in_f = NULL;

        //codec
        AVCodecID m_codec_id = AV_CODEC_ID_NONE;
        AVCodec *m_video_codec = NULL;
        AVCodecContext *m_video_codec_ctx= NULL;
        int m_bit_rate;
        int m_width;
        int m_height;
        int m_gop_size;
        AVPixelFormat m_pix_fmt;
        AVRational m_time_base;

        //muxing
        AVOutputFormat *m_out_fmt = NULL;
        AVFormatContext *m_out_fmt_ctx = NULL;
        OutputStream m_video_st;

        //filter
        AVFrame *m_filt_frame = NULL;
        AVFilterContext *m_buffersink_ctx = NULL;
        AVFilterContext *m_buffersrc_ctx = NULL;
        AVFilterGraph *m_filter_graph = NULL;
        std::string m_filter_descr = "null";

    private:
        const int SRC_WIDTH = 1920;
        const int SRC_HEIGHT = 1080;
        const int DST_WIDTH = 1920;
        const int DST_HEIGHT = 1536;


};
