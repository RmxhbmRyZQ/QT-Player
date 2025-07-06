//
// Created by Admin on 2025/6/30.
//

#ifndef VIDEODECODER_H
#define VIDEODECODER_H
#include "PlayerContext.h"

extern "C" {
#include <libavutil/time.h>
}

class VideoDecoder {
public:
    VideoDecoder(PlayerContext *ctx);
    ~VideoDecoder();
    int decode();
    int stopDecode();

private:
    void videoDecode();

private:
    PlayerContext *player_ctx = nullptr;
    std::thread *video_decode_thread = nullptr;
};



#endif //VIDEODECODER_H
