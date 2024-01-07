#include <iostream>

extern "C" {
    #include "libavcodec/avcodec.h"
    #include "libavformat/avformat.h"
    #include "libswscale/swscale.h"
    #include "libavutil/imgutils.h"
}

int main(){
    AVFormatContext	*pFormatCtx;

    pFormatCtx = avformat_alloc_context();

    if( avformat_open_input(&pFormatCtx,"input.mp4",NULL,NULL)!=0) {
		std::cout<<"Couldn't open input stream."<<std::endl;
		return -1;
	}

	if(avformat_find_stream_info(pFormatCtx,NULL)<0){
        std::cout<<"Couldn't find stream information."<<std::endl;
		return -1;
	}

    av_dump_format(pFormatCtx,0,"input.mp4",0);

	std::cout << "hello world" << std::endl;

    return 0;
}