extern "C" {
    #include <libavcodec/avcodec.h>
    #include <libavformat/avformat.h>
    #include <libswscale/swscale.h>
    #include <libavutil/imgutils.h>
}

#include <memory>

int main() {
    const char *output_filename = "output/rgba2mp4_output.mp4";
    int width = 640, height = 480;
    int fps = 25;                 
    int num_frames = 100;         

    AVFormatContext *fmt_ctx = NULL;
    avformat_alloc_output_context2(&fmt_ctx, NULL, NULL, output_filename);
    if (!fmt_ctx) {
        fprintf(stderr, "Could not create output context\n");
        return -1;
    }

    AVCodec *codec = avcodec_find_encoder(AV_CODEC_ID_H264);
    if (!codec) {
        fprintf(stderr, "Codec not found\n");
        return -1;
    }

    AVStream *stream = avformat_new_stream(fmt_ctx, codec);
    if (!stream) {
        fprintf(stderr, "Could not create stream\n");
        return -1;
    }

    AVCodecContext *codec_ctx = avcodec_alloc_context3(codec);
    codec_ctx->width = width;
    codec_ctx->height = height;
    codec_ctx->time_base = (AVRational){1, fps};
    codec_ctx->pix_fmt = AV_PIX_FMT_YUV420P; 
    codec_ctx->bit_rate = 4000000;           

    if (avcodec_open2(codec_ctx, codec, NULL) < 0) {
        fprintf(stderr, "Could not open codec\n");
        return -1;
    }
    avcodec_parameters_from_context(stream->codecpar, codec_ctx);

    if (avio_open(&fmt_ctx->pb, output_filename, AVIO_FLAG_WRITE) < 0) {
        fprintf(stderr, "Could not open output file\n");
        return -1;
    }

    if (avformat_write_header(fmt_ctx, NULL) < 0) {
        fprintf(stderr, "Error writing header\n");
        return -1;
    }

    AVFrame *frame = av_frame_alloc();
    frame->format = codec_ctx->pix_fmt;
    frame->width = width;
    frame->height = height;
    av_frame_get_buffer(frame, 0);

    // Initialize the conversion context.
    struct SwsContext *sws_ctx = sws_getContext(
        width, height, AV_PIX_FMT_RGBA,   
        width, height, AV_PIX_FMT_YUV420P,
        SWS_BILINEAR, NULL, NULL, NULL);

    uint8_t* rgbaDataWhite = new uint8_t[width * height * 4];
    memset(rgbaDataWhite, 255, width * height * 4);

    uint8_t* rgbaDataBlack = new uint8_t[width * height * 4];
    memset(rgbaDataBlack, 0, width * height * 4);

    AVPacket *pkt = av_packet_alloc();
    for (int i = 0; i < num_frames; i++) {

        // 
        uint8_t *src_data[1] = {i % 2 == 0 ? rgbaDataWhite : rgbaDataBlack};
        int src_linesize[1] = {4 * width};
        sws_scale(sws_ctx, src_data, src_linesize, 0, height,
                  frame->data, frame->linesize);

        frame->pts = i;

        if (avcodec_send_frame(codec_ctx, frame) < 0) {
            fprintf(stderr, "Error sending frame\n");
            break;
        }

        while (avcodec_receive_packet(codec_ctx, pkt) == 0) {
            av_packet_rescale_ts(pkt, codec_ctx->time_base, stream->time_base);
            pkt->stream_index = stream->index;
            av_interleaved_write_frame(fmt_ctx, pkt);
            av_packet_unref(pkt);
        }
    }

    avcodec_send_frame(codec_ctx, NULL);
    while (avcodec_receive_packet(codec_ctx, pkt) == 0) {
        av_packet_rescale_ts(pkt, codec_ctx->time_base, stream->time_base);
        av_interleaved_write_frame(fmt_ctx, pkt);
        av_packet_unref(pkt);
    }

    av_write_trailer(fmt_ctx);
    av_frame_free(&frame);
    av_packet_free(&pkt);
    avcodec_free_context(&codec_ctx);
    avio_closep(&fmt_ctx->pb);
    avformat_free_context(fmt_ctx);
    sws_freeContext(sws_ctx);

    return 0;
}