//
// Created by Admin on 2025/7/1.
//

#ifndef AUDIORENDER_H
#define AUDIORENDER_H
#include <QThread>

#include "AudioFilter.h"
#include "constant.h"
#include "PlayerContext.h"


class AudioRender : public QThread{
    Q_OBJECT
public:
    AudioRender(PlayerContext *ctx);
    ~AudioRender();
    virtual void run() override;

private:
    bool readFrame(AVFrame *frame, int sample_stride);

signals:
    void finished();

private:
    PlayerContext *player_ctx = nullptr;
    AudioFilter *audio_filters[FILTER_NUMBER] = {};
};

#endif //AUDIORENDER_H
