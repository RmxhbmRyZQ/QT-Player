//
// Created by Admin on 2025/6/30.
//

#ifndef AUDIODECODER_H
#define AUDIODECODER_H
#include "PlayerContext.h"


class AudioDecoder {
public:
    AudioDecoder(PlayerContext *ctx);
    ~AudioDecoder();
    int decode();
    int stopDecode();

private:
    void audioDecode();

private:
    PlayerContext *player_ctx = nullptr;
    std::thread *audio_decode_thread = nullptr;
};



#endif //AUDIODECODER_H
