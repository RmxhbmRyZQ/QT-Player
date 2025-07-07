//
// Created by Admin on 2025/7/5.
//

#pragma once
#include <QObject>
#include <QPixmap>
#include <atomic>
#include <mutex>

extern "C" {
    #include <libavutil/frame.h>
    #include <libavformat/avformat.h>
    #include <libswscale/swscale.h>
    #include <libavcodec/avcodec.h>
}

class ThumbnailWorker : public QObject {
    Q_OBJECT
public:
    explicit ThumbnailWorker(QObject* parent = nullptr);
    ~ThumbnailWorker();

    int init(const char *filename);
    void reset();
    bool isRunning();

public slots:
    void onRequestPreview(int mouseX, int status, int64_t value);

signals:
    // 工作线程处理完毕，发信号回UI线程
    void thumbnailReady(const AVFrame *frame, int mouseX, int status, int64_t value);

private:
    void release();
    int readFrame(int64_t value);
    int scaleFrame();

private:
    std::atomic<bool> running = false;
    std::atomic<bool> focus = false;

    // 缓存
    AVFrame* frame = nullptr;
    AVFrame* read_frame = nullptr;
    AVPacket* packet = nullptr;
    AVFrame* rescaled_frame = nullptr;
    int64_t lastPts = -1;
    int64_t lastTime = -1000000;

    // 视频
    AVFormatContext* pFormatCtx = nullptr;
    AVCodecContext* pCodecCtx = nullptr;
    SwsContext* swsCtx = nullptr;
    int streamIdx = -1;

    bool initialized = false;
};

