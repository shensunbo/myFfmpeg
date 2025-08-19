#include <iostream>
#include <chrono>
#include <thread>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

#include "log.h"

const int RECORD_DURATION_SEC = 15; // Record for 30 seconds

int main() {
    // Initialize FFmpeg
    avformat_network_init();
    av_log_set_level(AV_LOG_INFO);

    // Input stream parameters
    const char* input_url = "tcp://192.168.56.1:1234";
    const char* output_filename = "output/pullFromWin.mp4";

    // Open input stream
    AVFormatContext* input_ctx = nullptr;
    if (avformat_open_input(&input_ctx, input_url, nullptr, nullptr) < 0) {
        mylog(MyLogLevel::E, "Failed to open input stream");
        return -1;
    }

    // Retrieve stream information
    if (avformat_find_stream_info(input_ctx, nullptr) < 0) {
        mylog(MyLogLevel::E, "Failed to retrieve stream information");
        avformat_close_input(&input_ctx);
        return -1;
    }

    av_dump_format(input_ctx, 0, input_url, 0);

    // Find video stream
    int video_stream_idx = -1;
    for (unsigned int i = 0; i < input_ctx->nb_streams; i++) {
        if (input_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            video_stream_idx = i;
            break;
        }
    }

    if (video_stream_idx == -1) {
        mylog(MyLogLevel::E, "No video stream found");
        avformat_close_input(&input_ctx);
        return -1;
    }

    // Get video decoder parameters
    AVCodecParameters* codec_params = input_ctx->streams[video_stream_idx]->codecpar;
    
    // Find decoder
    const AVCodec* codec = avcodec_find_decoder(codec_params->codec_id);
    if (!codec) {
        mylog(MyLogLevel::E, "Decoder not found");
        avformat_close_input(&input_ctx);
        return -1;
    }

    // Create decoder context
    AVCodecContext* codec_ctx = avcodec_alloc_context3(codec);
    if (avcodec_parameters_to_context(codec_ctx, codec_params) < 0) {
        mylog(MyLogLevel::E, "Failed to copy decoder parameters");
        avformat_close_input(&input_ctx);
        avcodec_free_context(&codec_ctx);
        return -1;
    }

    // Open decoder
    if (avcodec_open2(codec_ctx, codec, nullptr) < 0) {
        mylog(MyLogLevel::E, "Failed to open decoder");
        avformat_close_input(&input_ctx);
        avcodec_free_context(&codec_ctx);
        return -1;
    }

    // Prepare output file
    AVFormatContext* output_ctx = nullptr;
    if (avformat_alloc_output_context2(&output_ctx, nullptr, nullptr, output_filename) < 0) {
        mylog(MyLogLevel::E, "Failed to create output context");
        avformat_close_input(&input_ctx);
        avcodec_free_context(&codec_ctx);
        return -1;
    }

    // Add video stream to output file
    AVStream* out_stream = avformat_new_stream(output_ctx, nullptr);
    if (!out_stream) {
        mylog(MyLogLevel::E, "Failed to create output stream");
        avformat_close_input(&input_ctx);
        avcodec_free_context(&codec_ctx);
        avformat_free_context(output_ctx);
        return -1;
    }

    // Copy codec parameters
    if (avcodec_parameters_copy(out_stream->codecpar, codec_params) < 0) {
        mylog(MyLogLevel::E, "Failed to copy codec parameters");
        avformat_close_input(&input_ctx);
        avcodec_free_context(&codec_ctx);
        avformat_free_context(output_ctx);
        return -1;
    }

    out_stream->codecpar->codec_tag = 0;  // Clear codec tag

    // Open output file
    if (!(output_ctx->oformat->flags & AVFMT_NOFILE)) {
        if (avio_open(&output_ctx->pb, output_filename, AVIO_FLAG_WRITE) < 0) {
            mylog(MyLogLevel::E, "Failed to open output file");
            avformat_close_input(&input_ctx);
            avcodec_free_context(&codec_ctx);
            avformat_free_context(output_ctx);
            return -1;
        }
    }

    // Write file header
    if (avformat_write_header(output_ctx, nullptr) < 0) {
        mylog(MyLogLevel::E, "Failed to write file header");
        avformat_close_input(&input_ctx);
        avcodec_free_context(&codec_ctx);
        if (!(output_ctx->oformat->flags & AVFMT_NOFILE)) {
            avio_closep(&output_ctx->pb);
        }
        avformat_free_context(output_ctx);
        return -1;
    }

    // Prepare frame and packet
    AVFrame* frame = av_frame_alloc();
    AVPacket* packet = av_packet_alloc();
    if (!frame || !packet) {
        mylog(MyLogLevel::E, "Failed to allocate frame or packet");
        avformat_close_input(&input_ctx);
        avcodec_free_context(&codec_ctx);
        if (!(output_ctx->oformat->flags & AVFMT_NOFILE)) {
            avio_closep(&output_ctx->pb);
        }
        avformat_free_context(output_ctx);
        av_frame_free(&frame);
        av_packet_free(&packet);
        return -1;
    }

    // Record start time
    auto start_time = std::chrono::steady_clock::now();
    int64_t frame_count = 0;

    // Main loop: read, decode, write
    while (true) {
        // Check if 30 seconds have elapsed
        auto current_time = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(current_time - start_time).count();
        if (elapsed >= RECORD_DURATION_SEC) {
            break;
        }

        // Read packet
        if (av_read_frame(input_ctx, packet) < 0) {
            mylog(MyLogLevel::W, "Error reading frame or EOF reached");
            break;
        }

        // Process only video stream
        if (packet->stream_index == video_stream_idx) {
            // Send packet to decoder
            if (avcodec_send_packet(codec_ctx, packet) < 0) {
                mylog(MyLogLevel::E, "Failed to send packet to decoder");
                av_packet_unref(packet);
                continue;
            }

            // Receive decoded frames
            while (avcodec_receive_frame(codec_ctx, frame) >= 0) {
                // Write frame to output file
                frame->pts = frame_count++;
                if (av_interleaved_write_frame(output_ctx, packet) < 0) {
                    mylog(MyLogLevel::E, "Failed to write frame");
                    break;
                }
            }
        }

        av_packet_unref(packet);
    }

    // Write file trailer
    av_write_trailer(output_ctx);

    // Dump output file information
    av_dump_format(output_ctx, 0, output_filename, 1);

    // Clean up resources
    avformat_close_input(&input_ctx);
    avcodec_free_context(&codec_ctx);
    if (!(output_ctx->oformat->flags & AVFMT_NOFILE)) {
        avio_closep(&output_ctx->pb);
    }
    avformat_free_context(output_ctx);
    av_frame_free(&frame);
    av_packet_free(&packet);

    mylog(MyLogLevel::I, "Successfully recorded %d seconds of video to %s", RECORD_DURATION_SEC, output_filename);
    return 0;
}