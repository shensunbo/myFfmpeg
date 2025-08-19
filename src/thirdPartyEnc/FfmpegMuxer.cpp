
#include <iostream>
#include <fstream>
#include <vector>
#include <string>

extern "C" {
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <libavformat/avformat.h>
#include <time.h>
#include <sys/time.h>
} // extern "C"

#include "FfmpegMuxer.h"
#include "CircularBuffer.h"
#include "log.h"

//ffmpeg read data callback function
static int read_packet_callback(void *opaque, uint8_t *buf, int buf_size) {
    FrameQueue* frameQueuePtr = (FrameQueue*)opaque;
    FrameData* tmpFrameData = FrameQueuePop(frameQueuePtr);

    if(tmpFrameData->endflag){
        return AVERROR_EOF;
    }

    memcpy(buf, tmpFrameData->dataptr, tmpFrameData->size);
    return tmpFrameData->size; 
}

FfmpegMuxer::FfmpegMuxer(FrameQueue& queue, const std::string filename):
    frame_queue(queue),
    output_filename(filename),
    muxing_end(false) {
}

FfmpegMuxer::~FfmpegMuxer(){

}

/*  @return  0 success, 1 failed */
int FfmpegMuxer::FfmpegMp4Muxer(){
    // std::string& outputFilename
    // initFrameQueue(&frame_queue);

    AVFormatContext *input_ctx = NULL;
    input_ctx = avformat_alloc_context();
    if (!input_ctx) {
        mylog(MyLogLevel::E, "Error: Failed to allocate input context\n");
        return 1;
    }

    int bufferSize = MAX_DATA_SIZE;
    unsigned char* tmpFrameBuffer = (unsigned char *)av_malloc(bufferSize);
    if(NULL == tmpFrameBuffer){
        mylog(MyLogLevel::E, "tmpFrameBuffer av_malloc(bufferSize) failed");
        assert(false);
    }

    AVIOContext *avio_ctx = avio_alloc_context(
        tmpFrameBuffer, 
        bufferSize, 
        0, 
        static_cast<void*>(&frame_queue),
        read_packet_callback, 
        NULL,
        NULL 
    );

    if(NULL == avio_ctx){
        mylog(MyLogLevel::E, "avio_ctx null, avio_alloc_context failed\n");
        assert(false);
        return -1;
    }

     input_ctx->pb = avio_ctx;
     input_ctx->flags |= AVFMT_FLAG_CUSTOM_IO;

    if (avformat_open_input(&input_ctx, "", NULL, NULL) != 0) {
        mylog(MyLogLevel::E, "Error: Failed to open input file\n");
        return 1;
    }

    if (avformat_find_stream_info(input_ctx, NULL) < 0) {
        mylog(MyLogLevel::E, "Error: Failed to find stream information\n");
        return 1;
    }

    // Find the H.264 video stream
    AVCodecParameters *video_codec_params = NULL;
    video_codec_params = input_ctx->streams[(int)AVMEDIA_TYPE_VIDEO]->codecpar;

    // Open output file
    AVFormatContext *output_ctx = NULL;
    if (avformat_alloc_output_context2(&output_ctx, NULL, NULL, output_filename.c_str()) != 0) {
        mylog(MyLogLevel::E, "Error: Failed to allocate output context[%s]\n",output_filename.c_str());
        assert(false);
        return 1;
    }

    // Add video stream to output file
    AVStream *video_stream = avformat_new_stream(output_ctx, NULL);
    if (!video_stream) {
        mylog(MyLogLevel::E, "Error: Failed to create video stream in output file\n");
        return 1;
    }
    if (avcodec_parameters_copy(video_stream->codecpar, video_codec_params) < 0) {
        mylog(MyLogLevel::E, "Error: Failed to copy codec parameters to video stream\n");
        return 1;
    }

    // Set video dimensions
    video_stream->codecpar->width = video_codec_params->width;
    video_stream->codecpar->height = video_codec_params->height;

    output_ctx->bit_rate = video_stream->codecpar->width * video_stream->codecpar->height * \
        input_ctx->streams[(int)AVMEDIA_TYPE_VIDEO]->r_frame_rate.num / 6;
    mylog(MyLogLevel::I, "width[%d], height[%d], r_frame_rate[%d], bit_rate[%ld], \n", video_stream->codecpar->width, video_stream->codecpar->height, \
        input_ctx->streams[(int)AVMEDIA_TYPE_VIDEO]->r_frame_rate.num, output_ctx->bit_rate);
    // assert(25 == input_ctx->streams[(int)AVMEDIA_TYPE_VIDEO]->r_frame_rate.num);

    // Open output file for writing
    if (!(output_ctx->oformat->flags & AVFMT_NOFILE)) {
        if (avio_open(&output_ctx->pb, output_filename.c_str(), AVIO_FLAG_WRITE) < 0) {
            mylog(MyLogLevel::E, "Error: Failed to open output file '%s'\n", output_filename.c_str());
            return 1;
        }
    }

    // Write the file header
    if (avformat_write_header(output_ctx, NULL) < 0) {
        mylog(MyLogLevel::E, "Error: Failed to write output file header\n");
        return 1;
    }

    // Read packets from input file and write to output file
    //TODO: 内存如何释放
    AVPacket pkt;
    CircularBuffer<AVPacket> circlePkgBuf(50);
    // AVPacket pkt[600];
    unsigned long mypts = 0;
    unsigned int output_ctx_timebase = output_ctx->streams[(int)AVMEDIA_TYPE_VIDEO]->time_base.den / input_ctx->streams[(int)AVMEDIA_TYPE_VIDEO]->r_frame_rate.num;
    // av_init_packet(&pkt);

    // while(!start_record){
    //     usleep(1000 * 100);
    // }

    bool keyFrameFound = false;

    while(!start_record){
        AVPacket tmppkt;
        av_read_frame(input_ctx, &tmppkt);

        if(circlePkgBuf.isFull()){
            AVPacket unrefPkt;
            circlePkgBuf.dequeue(unrefPkt);
            av_packet_unref(&unrefPkt);
        }

        circlePkgBuf.enqueue(tmppkt);
    }

    while(!circlePkgBuf.isEmpty()){
        AVPacket savePkt;
        circlePkgBuf.dequeue(savePkt);
        if (savePkt.stream_index == (int)AVMEDIA_TYPE_VIDEO) {
            if(!keyFrameFound){
                if(AV_PKT_FLAG_KEY == savePkt.flags){
                    mylog(MyLogLevel::I, "found Key frame !!!!\n");
                    keyFrameFound = true;
                }else {
                    av_packet_unref(&savePkt);
                    continue;
                }
            }
            // Set the packet's stream index to the output stream
            savePkt.stream_index = video_stream->index;
            savePkt.pts = mypts;
            savePkt.dts = mypts;
            savePkt.duration = output_ctx_timebase ;
            mypts += output_ctx_timebase;
            // mylog(MyLogLevel::I, "av_read_frame pts[%d]\n",pkt[i].pts);
            // Write the packet to the output file
            
            if (av_interleaved_write_frame(output_ctx, &savePkt) < 0) {
                mylog(MyLogLevel::E, "Error: Failed to write packet to output file\n");
                return 1;
            }
        }

        av_packet_unref(&savePkt);
    }

    while (av_read_frame(input_ctx, &pkt) == 0) {
        // if(start_record){
            if (pkt.stream_index == (int)AVMEDIA_TYPE_VIDEO) {
                // if(!keyFrameFound){
                //     if(AV_PKT_FLAG_KEY == pkt.flags){
                //         keyFrameFound = true;
                //     }else {
                //         // av_packet_unref(&pkt);
                //         continue;
                //     }
                // }
                // Set the packet's stream index to the output stream
                pkt.stream_index = video_stream->index;
                pkt.pts = mypts;
                pkt.dts = mypts;
                pkt.duration = output_ctx_timebase ;
                mypts += output_ctx_timebase;
                // mylog(MyLogLevel::I, "av_read_frame pts[%d]\n",pkt.pts);
                // Write the packet to the output file
                
                if (av_interleaved_write_frame(output_ctx, &pkt) < 0) {
                    mylog(MyLogLevel::E, "Error: Failed to write packet to output file\n");
                    return 1;
                }
            }
        // }

        av_packet_unref(&pkt);
    }

    // Write the file trailer
    if (av_write_trailer(output_ctx) < 0) {
        mylog(MyLogLevel::E, "Error: Failed to write output file trailer\n");
        return 1;
    }

    av_dump_format(output_ctx, 0, output_filename.c_str(), 1);
    mylog(MyLogLevel::I, "bit_rate[%ld], nb_frames[%ld]\n",output_ctx->bit_rate, output_ctx->streams[0]->nb_frames);

    // Close output file
    if (!(output_ctx->oformat->flags & AVFMT_NOFILE)) {
        avio_close(output_ctx->pb);
    }

    // Free resources
    avformat_close_input(&input_ctx);
    avformat_free_context(output_ctx);

    muxing_end = true;
    // destroyFrameQueue(&frame_queue);

    mylog(MyLogLevel::I,"MP4 file '%s' created successfully\n", output_filename.c_str());

    return 0;
}

bool FfmpegMuxer::isMuxingEnd(){
    return muxing_end;
}