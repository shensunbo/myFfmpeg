#if 1
#include "myffmpeg.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern "C" {
    static void log_packet(const AVFormatContext *fmt_ctx, const AVPacket *pkt)
    {
        AVRational *time_base = &fmt_ctx->streams[pkt->stream_index]->time_base;

        //printf("pts:%s pts_time:%s dts:%s dts_time:%s duration:%s duration_time:%s stream_index:%d\n",
        //       av_ts2str(pkt->pts), av_ts2timestr(pkt->pts, time_base),
        //       av_ts2str(pkt->dts), av_ts2timestr(pkt->dts, time_base),
        //       av_ts2str(pkt->duration), av_ts2timestr(pkt->duration, time_base),
        //       pkt->stream_index);
    }
}

int MyFfmpeg::ReadNv12(AVFrame *frame, FILE* fd, int width, int height){
    assert(frame && fd);
    if(!frame || !fd){
        printf("Error: frame or fd is null\n");
        return -1;
    }

    size_t y = 0, u = 0, v = 0;

    y = fread(frame->data[0], 1, width * height, fd); // read Y
    u = fread(frame->data[1], 1, width * height / 4, fd); // read U
    v = fread(frame->data[2], 1, width * height / 4, fd); // read V

    if (y != width * height || u != width * height / 4 || v != width * height / 4) {
        printf("Error: read error y:%zu u:%zu v:%zu\n", y, u, v);
        return -1;
    }

    return 0;
}


int MyFfmpeg::get_video_frame(OutputStream *ost, std::string filename, int width, int height){
    assert(ost);
    if(!ost){
        printf("Error: ost is null\n");
        return -1;
    }

    static FILE *fd = NULL;
    if(!fd){
        int err = fopen_s(&fd, filename.c_str(), "rb");
        if (err != 0) {
            printf("Could not open input file pic.uyvy\n");
            return -1;
        }
    }

    if (av_frame_make_writable(ost->frame) < 0){
        exit(1);
    }

    long int currentPosition = ftell(fd);
    static int count = 0;
    count ++;
    printf("count = %d, currentPosition = %d\n", count, currentPosition);

    int ret = MyFfmpeg::ReadNv12(ost->frame, fd, width, height);
    if(ret < 0){
        printf("ReadNv12 end\n");
        fclose(fd);
        ost->frame->pts = ost->next_pts++;
        fd = NULL;
        return 1;
    }

    ost->frame->pts = ost->next_pts++;

    return 0;
}

int MyFfmpeg::get_video_frame(OutputStream *ost)
{
    AVCodecContext *c = ost->enc;

    /* check if we want to generate more frames */
    if (av_compare_ts(ost->next_pts, c->time_base,
                      10, AVRational{ 1, 1 }) > 0)
        return -1;

    /* when we pass a frame to the encoder, it may keep a reference to it
     * internally; make sure we do not overwrite it here */
    if (av_frame_make_writable(ost->frame) < 0)
        exit(1);

    fill_yuv_image(ost->frame, ost->next_pts, c->width, c->height);

    ost->frame->pts = ost->next_pts++;

    return 0;
}

MyFfmpeg::MyFfmpeg(){
    m_out_filename = "output/OUT_1920_1080_25.mp4";
    m_in_filename = "res/OUT_1920_1080_25.h264";
    m_bit_rate = 4000000;
    m_width = SRC_WIDTH;
    m_height = SRC_HEIGHT;
    m_gop_size = 1;
    m_pix_fmt = AV_PIX_FMT_YUV420P;
    m_filter_descr = "movie=logo.png[wm];[in][wm]overlay=5:5[out]";
    m_time_base = AVRational{1, 25};
}

MyFfmpeg::MyFfmpeg(const std::string& out_filename, const std::string& in_filename, int bit_rate, int width, int height, int gop_size, AVPixelFormat pix_fmt):
m_out_filename(out_filename), m_in_filename(in_filename), m_bit_rate(bit_rate), m_width(width), m_height(height), m_gop_size(gop_size), m_pix_fmt(pix_fmt)
{
  
}

MyFfmpeg::~MyFfmpeg(){}

int MyFfmpeg::VideoMuxing(){

    //init
    MuxingInit();
    SetupVideoEncoder();
    OpenOutputFile();
    av_dump_format(m_out_fmt_ctx, 0, m_out_filename.c_str(), 1);

    WriteOutputFile();

    MuxingDeinit();

    return 0;
}

int MyFfmpeg::H264Muxing(){
    MuxingInit();
    OpenOutputFile();
    av_dump_format(m_out_fmt_ctx, 0, m_out_filename.c_str(), 1);

    return 0;
}

int MyFfmpeg::VideoFilterAndMuxing(){
    //init
    MuxingInit();
    FiltersInit(m_filter_descr.c_str());
    SetupVideoEncoder();
    OpenOutputFile();
    av_dump_format(m_out_fmt_ctx, 0, m_out_filename.c_str(), 1);

    WriteOutputFileWithFilter();

    av_dump_format(m_out_fmt_ctx, 0, m_out_filename.c_str(), 1);

    FiltersDeinit();
    MuxingDeinit();

    return 0;
}

int MyFfmpeg::FiltersInit(const char *filters_descr){
    char args[512];
    int ret = 0;
    const AVFilter *buffersrc  = avfilter_get_by_name("buffer");
    const AVFilter *buffersink = avfilter_get_by_name("buffersink");
    AVFilterInOut *outputs = avfilter_inout_alloc();
    AVFilterInOut *inputs  = avfilter_inout_alloc();
    AVRational time_base = m_time_base;
    enum AVPixelFormat pix_fmts[] = { AV_PIX_FMT_YUV420P, AV_PIX_FMT_NONE };
 
    m_filt_frame = av_frame_alloc();
    m_filter_graph = avfilter_graph_alloc();
    if (!outputs || !inputs || !m_filter_graph || !m_filt_frame) {
        ret = AVERROR(ENOMEM);
        goto end;
    }
 
    /* buffer video source: the decoded frames from the decoder will be inserted here. */
    snprintf(args, sizeof(args),
           "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
           SRC_WIDTH, SRC_HEIGHT, AV_PIX_FMT_YUV420P,
           time_base.num, time_base.den,
           1, 1);
 
    ret = avfilter_graph_create_filter(&m_buffersrc_ctx, buffersrc, "in",
                                       args, NULL, m_filter_graph);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot create buffer source\n");
        goto end;
    }
 
    /* buffer video sink: to terminate the filter chain. */
    ret = avfilter_graph_create_filter(&m_buffersink_ctx, buffersink, "out",
                                       NULL, NULL, m_filter_graph);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot create buffer sink\n");
        goto end;
    }
 
    ret = av_opt_set_int_list(m_buffersink_ctx, "pix_fmts", pix_fmts,
                              AV_PIX_FMT_NONE, AV_OPT_SEARCH_CHILDREN);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot set output pixel format\n");
        goto end;
    }
 
    /*
     * Set the endpoints for the filter graph. The m_filter_graph will
     * be linked to the graph described by filters_descr.
     */
 
    /*
     * The buffer source output must be connected to the input pad of
     * the first filter described by filters_descr; since the first
     * filter input label is not specified, it is set to "in" by
     * default.
     */
    outputs->name       = av_strdup("in");
    outputs->filter_ctx = m_buffersrc_ctx;
    outputs->pad_idx    = 0;
    outputs->next       = NULL;
 
    /*
     * The buffer sink input must be connected to the output pad of
     * the last filter described by filters_descr; since the last
     * filter output label is not specified, it is set to "out" by
     * default.
     */
    inputs->name       = av_strdup("out");
    inputs->filter_ctx = m_buffersink_ctx;
    inputs->pad_idx    = 0;
    inputs->next       = NULL;
 
    if ((ret = avfilter_graph_parse_ptr(m_filter_graph, filters_descr,
                                    &inputs, &outputs, NULL)) < 0)
        goto end;
 
    if ((ret = avfilter_graph_config(m_filter_graph, NULL)) < 0)
        goto end;
 
end:
    avfilter_inout_free(&inputs);
    avfilter_inout_free(&outputs);
 
    return ret;
}

void MyFfmpeg::FiltersDeinit(){
    av_frame_free(&m_filt_frame);
    avfilter_free(m_buffersink_ctx);
    avfilter_free(m_buffersrc_ctx);
    avfilter_graph_free(&m_filter_graph);
}

void MyFfmpeg::MuxingInit(){
    avformat_alloc_output_context2(&m_out_fmt_ctx, NULL, NULL, m_out_filename.c_str());
    if (!m_out_fmt_ctx) {
        m_logger.log(LogLevel::LOGERROR, __FUNCTION__, __LINE__, __FILE__, "avformat_alloc_output_context2 error ");
        assert(false);
    }

    m_out_fmt = m_out_fmt_ctx->oformat;
    m_codec_id = m_out_fmt_ctx->oformat->video_codec;

    VideoCodecInit();
    m_video_st.enc = m_video_codec_ctx;

    m_video_st.st = avformat_new_stream(m_out_fmt_ctx, NULL);
    if (!m_video_st.st) {
        m_logger.log(LogLevel::LOGERROR, __FUNCTION__, __LINE__, __FILE__, "avformat_new_stream error ");
        assert(false);
    }

    m_video_st.st->id = m_out_fmt_ctx->nb_streams - 1;
}

void MyFfmpeg::MuxingDeinit(){

    avcodec_free_context(&m_video_st.enc);
    av_frame_free(&m_video_st.frame);
    av_frame_free(&m_video_st.tmp_frame);
    sws_freeContext(m_video_st.sws_ctx);
    swr_free(&m_video_st.swr_ctx);
    // VideoCodecDeinit();

    if (!(m_out_fmt->flags & AVFMT_NOFILE)){
        /* Close the output file. */
        avio_closep(&m_out_fmt_ctx->pb);
    }
    
    avformat_free_context(m_out_fmt_ctx);
}

void MyFfmpeg::VideoCodecInit(){

    if(AV_CODEC_ID_NONE == m_codec_id){
        m_logger.log(LogLevel::LOGERROR, __FUNCTION__, __LINE__, __FILE__, "VideoCodecInit error codec_id is AV_CODEC_ID_NONE");
        assert(false);
    }

    m_video_codec = avcodec_find_encoder(m_codec_id);
    if (!m_video_codec) {
        m_logger.log(LogLevel::LOGERROR, __FUNCTION__, __LINE__, __FILE__, "avcodec_find_encoder error ");
        assert(false);
    }

    AVCodecContext *codecCtx = avcodec_alloc_context3(m_video_codec);
    if (!codecCtx) {
        m_logger.log(LogLevel::LOGERROR, __FUNCTION__, __LINE__, __FILE__, "avcodec_alloc_context3 error ");
        assert(false);
    }

    codecCtx->codec_id = m_codec_id;
    codecCtx->bit_rate = m_bit_rate;
    codecCtx->width    = m_width;
    codecCtx->height   = m_height;
    codecCtx->time_base     = m_time_base;
    codecCtx->gop_size      = m_gop_size; /* emit one intra frame every twelve frames at most */
    codecCtx->pix_fmt       = m_pix_fmt;

    m_video_codec_ctx = codecCtx;
}

void MyFfmpeg::VideoCodecDeinit(){
    avcodec_free_context(&m_video_codec_ctx);
}

void MyFfmpeg::SetupVideoEncoder()
{
    int ret;
    AVCodecContext *codecCtx = m_video_st.enc;

    ret = avcodec_open2(codecCtx, m_video_codec, NULL);
    if (ret < 0) {
        m_logger.log(LogLevel::LOGERROR,  __FUNCTION__, __LINE__, __FILE__, "Could not open video codec: ");
        exit(1);
    }

    /* allocate and init a re-usable frame */
    m_video_st.frame = AllocFrame(codecCtx->pix_fmt, codecCtx->width, codecCtx->height);
    if (!m_video_st.frame) {
        m_logger.log(LogLevel::LOGERROR,  __FUNCTION__, __LINE__, __FILE__, "Could not allocate video frame\n");
        exit(1);
    }

    /* If the output format is not YUV420P, then a temporary YUV420P
     * picture is needed too. It is then converted to the required
     * output format. */
    m_video_st.tmp_frame = NULL;
    if (codecCtx->pix_fmt != AV_PIX_FMT_YUV420P) {
        m_video_st.tmp_frame = AllocFrame(AV_PIX_FMT_YUV420P, codecCtx->width, codecCtx->height);
        if (!m_video_st.tmp_frame) {
            m_logger.log(LogLevel::LOGERROR,  __FUNCTION__, __LINE__, __FILE__, "Could not allocate temporary picture\n");
            exit(1);
        }
    }

    /* copy the stream parameters to the muxer */
    ret = avcodec_parameters_from_context(m_video_st.st->codecpar, codecCtx);
    if (ret < 0) {
        m_logger.log(LogLevel::LOGERROR,  __FUNCTION__, __LINE__, __FILE__, "Could not copy the stream parameters\n");
        exit(1);
    }
}

AVFrame* MyFfmpeg::AllocFrame(enum AVPixelFormat pix_fmt, int width, int height){
    AVFrame *picture;
    int ret;

    picture = av_frame_alloc();
    if (!picture)
        return NULL;

    picture->format = pix_fmt;
    picture->width  = width;
    picture->height = height;

    /* allocate the buffers for the frame data */
    ret = av_frame_get_buffer(picture, 0);
    if (ret < 0) {
        m_logger.log(LogLevel::LOGERROR,  __FUNCTION__, __LINE__, __FILE__, "Could not allocate frame data.");
        exit(1);
    }

    return picture;
}

void MyFfmpeg::FreeFrame(AVFrame* frame){
    av_frame_free(&frame);
}

void MyFfmpeg::OpenOutputFile(){
    if (!(m_out_fmt->flags & AVFMT_NOFILE)) {
        int ret = avio_open(&m_out_fmt_ctx->pb, m_out_filename.c_str(), AVIO_FLAG_WRITE);
        if (ret < 0) {
            m_logger.log(LogLevel::LOGERROR, __FUNCTION__, __LINE__, __FILE__, "Could not open", m_out_filename.c_str(),
                    " :"  /*+ std::string(my_av_err2str(ret))*/);
            assert(false);
        }
    }
}

void MyFfmpeg::fill_yuv_image(AVFrame *pict, int frame_index,
                           int width, int height)
{
    int x, y, i;

    i = frame_index;

    /* Y */
    for (y = 0; y < height; y++)
        for (x = 0; x < width; x++)
            pict->data[0][y * pict->linesize[0] + x] = x + y + i * 3;

    /* Cb and Cr */
    for (y = 0; y < height / 2; y++) {
        for (x = 0; x < width / 2; x++) {
            pict->data[1][y * pict->linesize[1] + x] = 128 + y + i * 2;
            pict->data[2][y * pict->linesize[2] + x] = 64 + x + i * 5;
        }
    }
}

void MyFfmpeg::WriteOutputFile(){
    int ret = avformat_write_header(m_out_fmt_ctx, NULL);
    if (ret < 0) {
        m_logger.log(LogLevel::LOGERROR, __FUNCTION__, __LINE__, __FILE__, "Error occurred when opening output file: "
                 /*+ std::string(my_av_err2str(ret))*/);
        assert(false);
    }

    int encode_video = 1;
    while (encode_video){
        // ret = get_video_frame(&m_video_st, m_in_filename, SRC_WIDTH, SRC_HEIGHT);
        // if(-1 == ret){
        //     printf("get_video_frame failed ret %d\n", ret);
        //     break;
        // }else if(1 == ret){
        //     printf("get_video_frame end\n");
        //     break;
        // }
        ret = get_video_frame(&m_video_st);
        encode_video = !WriteFrame(0==ret?m_video_st.frame:NULL);
    }

    av_write_trailer(m_out_fmt_ctx);
}

void MyFfmpeg::WriteOutputFileWithFilter(){
    assert(m_out_fmt_ctx && m_buffersrc_ctx && m_buffersink_ctx && m_filt_frame);

    int ret = avformat_write_header(m_out_fmt_ctx, NULL);
    if (ret < 0) {
        m_logger.log(LogLevel::LOGERROR, __FUNCTION__, __LINE__, __FILE__, "Error occurred when opening output file: "
                 /*+ std::string(my_av_err2str(ret))*/);
        assert(false);
    }

    int encode_video = 1;
    while (encode_video){
        /* push frame into the filtergraph */
        // get_video_frame(&m_video_st);
        ret = get_video_frame(&m_video_st, m_in_filename, SRC_WIDTH, SRC_HEIGHT);
        if(-1 == ret){
            printf("get_video_frame failed ret %d\n", ret);
            break;
        }else if(1 == ret){
            printf("get_video_frame end\n");
            break;
        }

        if (av_buffersrc_add_frame_flags(m_buffersrc_ctx,m_video_st.frame, AV_BUFFERSRC_FLAG_KEEP_REF) < 0) {
           //TODO need err msg
            av_log(NULL, AV_LOG_ERROR, "Error while feeding the filtergraph\n");
            break;
        }

        ret = av_buffersink_get_frame(m_buffersink_ctx, m_filt_frame);
        if(ret == AVERROR(EAGAIN)){
            printf("av_buffersink_get_frame ret %d continue\n", ret);
            continue;
        }

        if (ret == AVERROR_EOF){
            printf("av_buffersink_get_frame ret %d\n", ret);
            break;
        }

        if (ret < 0){
            av_log(NULL, AV_LOG_ERROR, "av_buffersink_get_frame err %d\n" , ret);
            assert(false);
        }

        /* pull filtered frames from the filtergraph */
        encode_video = !WriteFrame(m_filt_frame);
        av_frame_unref(m_filt_frame);
    }

    av_write_trailer(m_out_fmt_ctx);
}

int MyFfmpeg::WriteFrame(AVFrame *frame)
{
    int ret;

    // send the frame to the encoder
    ret = avcodec_send_frame(m_video_codec_ctx, frame);
    if (ret < 0) {
        m_logger.log(LogLevel::LOGERROR, __FUNCTION__, __LINE__, __FILE__, "Error sending a frame to the encoder: "
                 /*+ std::string(my_av_err2str(ret))*/);
        assert(false);
    }

    while (ret >= 0) {
        AVPacket pkt = { 0 };

        ret = avcodec_receive_packet(m_video_codec_ctx, &pkt);
        if (ret == AVERROR(EAGAIN)){
            break;
            m_logger.log(LogLevel::LOGERROR, __FUNCTION__, __LINE__, __FILE__, "avcodec_receive_packet AVERROR(EAGAIN) ");
        }
        else if(ret == AVERROR_EOF)
            break;
        else if (ret < 0) {
            m_logger.log(LogLevel::LOGERROR, __FUNCTION__, __LINE__, __FILE__, "Error encoding a frame: "
                 /*+ std::string(my_av_err2str(ret))*/);
            assert(false);
        }

        /* rescale output packet timestamp values from codec to stream timebase */
        av_packet_rescale_ts(&pkt, m_video_codec_ctx->time_base, m_video_st.st->time_base);
        pkt.stream_index = m_video_st.st->index;

        /* Write the compressed frame to the media file. */
        log_packet(m_out_fmt_ctx, &pkt);
        ret = av_interleaved_write_frame(m_out_fmt_ctx, &pkt);
        av_packet_unref(&pkt);
        if (ret < 0) {
            m_logger.log(LogLevel::LOGERROR, __FUNCTION__, __LINE__, __FILE__, "Error while writing output packet: " 
                 /*+ std::string(my_av_err2str(ret))*/);
            assert(false);
        }
    }

    return ret == AVERROR_EOF ? 1 : 0;
}


#endif