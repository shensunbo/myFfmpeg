#include "FfmpegH264Parser.h"
#include "log.h"

extern "C"{
    #include <stdio.h>
    #include <stdlib.h>
    #include <string.h>
    #include <assert.h>
    #include <unistd.h>

    #include "libavcodec/avcodec.h"
}


FfmegH264Parser::FfmegH264Parser(FrameQueue& queue, std::string filenameOrUrl):
    nal_queue(queue), filename_or_url(filenameOrUrl), decode_start(false) {

}

FfmegH264Parser::~FfmegH264Parser(){
    mylog(MyLogLevel::I, "FfmegH264Parser::~FfmegH264Parser() call");
}

int FfmegH264Parser::h264ParserByFileRun(){
    const AVCodec *codec;
    AVCodecParserContext *parser;
    AVCodecContext *c= NULL;
    FILE *f;
    AVFrame *frame;
    uint8_t *data;
    size_t   data_size;
    int ret;
    AVPacket *pkt;


    printf("filename_or_url.c_str()[%s] \r\n", filename_or_url.c_str());
    //Thread ID: 3
    pthread_t thread_id = pthread_self();
    (void)pthread_setname_np(thread_id, "FfmegH264Parser::h264ParserByFileRun");
    mylog(MyLogLevel::I,"shenunbo h264ParserByFileRun Thread ID: %d\n",thread_id);

    // initFrameQueue(&nal_queue);

    pkt = av_packet_alloc();
    if (!pkt)
        exit(1);

    /* set end of buffer to 0 (this ensures that no overreading happens for damaged MPEG streams) */
    uint8_t* inbuf = (uint8_t*)malloc(INBUF_SIZE);
    memset(inbuf, 0, INBUF_SIZE);

    /* find the MPEG-1 video decoder */
    codec = avcodec_find_decoder(AV_CODEC_ID_H264);
    if (!codec) {
        fprintf(stderr, "Codec not found\n");
        exit(1);
    }

    parser = av_parser_init(codec->id);
    if (!parser) {
        fprintf(stderr, "parser not found\n");
        exit(1);
    }

    c = avcodec_alloc_context3(codec);
    if (!c) {
        fprintf(stderr, "Could not allocate video codec context\n");
        exit(1);
    }

    /* For some codecs, such as msmpeg4 and mpeg4, width and height
       MUST be initialized there because this information is not
       available in the bitstream. */

    /* open it */
    if (avcodec_open2(c, codec, NULL) < 0) {
        fprintf(stderr, "Could not open codec\n");
        exit(1);
    }

    f = fopen(filename_or_url.c_str(), "rb");
    if (!f) {
        fprintf(stderr, "Could not open %s\n", filename_or_url.c_str());
        exit(1);
    }

    frame = av_frame_alloc();
    if (!frame) {
        fprintf(stderr, "Could not allocate video frame\n");
        exit(1);
    }

    while(!decode_start){
        usleep(30 * 1000);
        mylog(MyLogLevel::I, "wait for decode_start");
    }

    while (!feof(f)) {
        /* read raw data from the input file */
        data_size = fread(inbuf, 1, INBUF_SIZE, f);
        if (!data_size)
            break;

        /* use the parser to split the data into frames */
        data = inbuf;
        while (data_size > 0) {
            ret = av_parser_parse2(parser, c, &pkt->data, &pkt->size,
                                   data, data_size, AV_NOPTS_VALUE, AV_NOPTS_VALUE, 0);
            if (ret < 0) {
                fprintf(stderr, "Error while parsing\n");
                exit(1);
            }
            data      += ret;
            data_size -= ret;

            if (pkt->size > 0){
                printf("0x%x 0x%x 0x%x 0x%x \r\n", pkt->data[0], pkt->data[1], pkt->data[2], pkt->data[3]);
                FrameQueuePush(&nal_queue, reinterpret_cast<char*>(pkt->data), pkt->size, false);
                //TODO
                usleep(30 * 1000);
            }
            
            // decode(c, frame, pkt, outfilename);
        }
    }

    FrameQueuePush(&nal_queue, reinterpret_cast<char*>(pkt->data), pkt->size, true);
    FrameQueuePush(&nal_queue, reinterpret_cast<char*>(pkt->data), pkt->size, true);
    FrameQueuePush(&nal_queue, reinterpret_cast<char*>(pkt->data), pkt->size, true);

    // /* flush the decoder */
    // decode(c, frame, NULL, outfilename);

    fclose(f);

    av_parser_close(parser);
    avcodec_free_context(&c);
    av_frame_free(&frame);
    av_packet_free(&pkt);
    free(inbuf);
    inbuf = nullptr;

    mylog(MyLogLevel::I, "FfmegH264Parser::h264ParserByFileRun end!!");
    return 0;
}

int FfmegH264Parser::h264ParserByRtspRun(){
    return 0;
}

void FfmegH264Parser::setDecoderState(bool ifStart){
    decode_start = ifStart;
}