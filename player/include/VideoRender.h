//
// Created by Admin on 2025/7/1.
//

#ifndef VIDEORENDER_H
#define VIDEORENDER_H

#include "PlayerContext.h"
#include <QThread>

extern "C" {
#include <libavutil/time.h>
}

class VideoRender : public QThread{
    Q_OBJECT
public:
    VideoRender(PlayerContext *ctx);
    ~VideoRender();
    virtual void run();

signals:
    void onRefreshVideo();
    void finished();

private:
    int64_t calculateWaitTime(int64_t pts);

private:
    PlayerContext *player_ctx = nullptr;
    int64_t last_pts = 0;
    int64_t speed_offset = 0;
};



#endif //VIDEORENDER_H
