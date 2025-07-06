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
    PlayerContext *player_ctx = nullptr;
    int64_t last_pts = 0;
};



#endif //VIDEORENDER_H
