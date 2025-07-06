//
// Created by Admin on 2025/7/1.
//

#ifndef AUDIOFILTER_H
#define AUDIOFILTER_H

extern "C" {
    #include "libavutil/opt.h"
    #include "libavfilter/avfilter.h"
    #include "libavfilter/buffersrc.h"
    #include "libavfilter/buffersink.h"
}


#include "PlayerContext.h"


class AudioFilter {
public:
    AudioFilter(PlayerContext *ctx);
    ~AudioFilter();
    int initFilter(double speed);
    int filterIn(AVFrame *inFrame);
    int filterOut(AVFrame *outFrame);
    int isValid();

private:
    PlayerContext *player_ctx = nullptr;
    AVFilterContext* buf_ctx = nullptr;           // 输入滤镜上下文
    AVFilterContext* sink_ctx = nullptr;          // 输出滤镜上下文
    AVFilterGraph *graph = nullptr;               // 滤镜图
    bool initialized = false;
};



#endif //AUDIOFILTER_H
