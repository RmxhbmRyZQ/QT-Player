//
// Created by Admin on 2025/6/30.
//

#ifndef PLAYER_H
#define PLAYER_H
#include <qobject.h>
#include <QMediaDevices>
#include <QTimer>

#include "AudioDecoder.h"
#include "AudioRender.h"
#include "PacketDemux.h"
#include "PlayerContext.h"
#include "VideoDecoder.h"
#include "VideoRender.h"


class Player : public QObject{
    Q_OBJECT
public:
    Player(QObject *parent = nullptr);
    ~Player();
    int setFile(const char *filename) const;
    int start();
    int pause(bool b) const;
    int isPlaying() const;
    int reset();
    int stop();
    int seek(int n);
    int forward();
    int backward();
    int setVolume(int volume);
    float getVolume() const;
    int setMute(bool mute);
    int getMute() const;
    int setSpeed(int idx);
    int haveVideo();
    int setVideoRenderWidgets(VideoRenderWidget* renderWidgets);

signals:
    void onRefreshVideo();
    void onProgressChange(double progress);
    void onProgressInit(double progress);

private slots:
    void refreshVideoHandler();
    void sendProgressChanged();
    void finishedHandler();

private:
    PlayerContext *player_ctx = nullptr;
    PacketDemux *packet_demux = nullptr;
    AudioDecoder *audio_decoder = nullptr;
    VideoDecoder *video_decoder = nullptr;

    VideoRender *video_render = nullptr;
    AudioRender *audio_render = nullptr;

    QTimer *progress_timer = nullptr;
    bool initialized_ = false;
};



#endif //PLAYER_H
