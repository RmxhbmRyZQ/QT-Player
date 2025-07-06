//
// Created by Admin on 2025/6/30.
//

#ifndef PLAYERCONTEXT_H
#define PLAYERCONTEXT_H

#include "AudioBuffer.h"
#include "constant.h"
#include "VideoRenderWidget.h"

extern "C" {
    #include <libavcodec/avcodec.h>
    #include <libavformat/avformat.h>
    #include <libswscale/swscale.h>
    #include <libswresample/swresample.h>
}

#include <atomic>
#include <QAudioSink>

#include "AVPacketQueue.h"
#include "AVFrameQueue.h"

class PlayerContext {
public:
    PlayerContext();
    ~PlayerContext();

    // 初始化成员资源（注意：不含 AVFormatContext 打开）
    bool initPacketAndFrame();
    bool initQueues(size_t pkt_queue_size = PACKET_QUEUE_SIZE, size_t frame_queue_size = FRAME_QUEUE_SIZE, size_t audio_buffer_size = RING_QUEUE_SIZE);
    void closeFile();
    void freeQueues();
    void closeQueues();
    void openQueues();
    void freePacketAndFrame();
    void closeAudioDevice();
    void resetJump();
    void resetEnd();
    void resetValue();
    void waitJump(char *str);

public:
    char *filename = nullptr;
    AVCodecContext *v_codec_ctx = nullptr;
    AVCodecContext *a_codec_ctx = nullptr;
    AVFormatContext *fmt_ctx = nullptr;

    int audio_stream_idx = -1;
    int video_stream_idx = -1;

    AVPacket *v_packet = nullptr;
    AVFrame *v_frame = nullptr;
    AVPacket *a_packet = nullptr;
    AVFrame *a_frame = nullptr;

    // 帧包缓冲区
    AVPacketQueue *v_packet_queue = nullptr;
    AVPacketQueue *a_packet_queue = nullptr;
    AVFrameQueue *v_frame_queue = nullptr;
    AudioBuffer *audio_buffer = nullptr;

    // 音视频重采样
    SwrContext *swr_ctx = nullptr;
    SwsContext *sws_ctx = nullptr;
    uint8_t *temp_buf = nullptr;
    int temp_buf_size = 0;

    // 音视频渲染
    QAudioSink *audio_sink = nullptr;
    QIODevice *audio_device = nullptr;
    VideoRenderWidget *video_render = nullptr;

    //
    std::atomic<int> quit = 0;
    std::atomic<int> pause = 0;

    // 跳转
    std::atomic<int> jump = 0;
    std::atomic<int> jump_busy = 0;
    std::atomic<int> jump_free = 0;
    std::atomic<int> ready_to_jump = 0;
    std::atomic<int> jump_target_number = 0;

    float volume = 0.5;
    int mute = 0;
    int speed_idx = 2;
    const double speeds[FILTER_NUMBER] = {0.5, 0.75, 1.0, 1.5, 2.0, 3.0};

    // 结束判定
    std::atomic<bool> audio_decoder_end = false;
    std::atomic<bool> audio_render_end = false;
    std::atomic<bool> video_decoder_end = false;
    std::atomic<bool> video_render_end = false;
    std::atomic<bool> audio_demux_end = false;
    std::atomic<bool> video_demux_end = false;
    std::atomic<bool> demux_end = false;

    // 音视频同步
    double duration = 0;
    int64_t base_time = 0;
    int64_t audio_offset = 0;
    int64_t video_offset = 0;
};

#endif //PLAYERCONTEXT_H
