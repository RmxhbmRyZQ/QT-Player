//
// Created by Admin on 2025/7/1.
//

#ifndef AVPACKETQUEUE_H
#define AVPACKETQUEUE_H



#pragma once

extern "C" {
#include <libavcodec/avcodec.h>
}

#include <vector>
#include <mutex>
#include <condition_variable>

class AVPacketQueue {
public:
    explicit AVPacketQueue(size_t capacity);
    ~AVPacketQueue();

    bool isValid() const;

    bool push(AVPacket* pkt);
    bool pop(AVPacket* out);
    void clear();
    void close();
    void open();
    size_t size();

    // 新增功能：暂停与恢复
    void pause();
    void resume();
    void setPaused(bool pause);
    bool isPaused();
    void notify();

private:
    size_t capacity_;
    std::vector<AVPacket*> buffer_;
    size_t head_, tail_, size_;
    bool initialized_;
    bool is_closed_;
    bool paused_;

    std::mutex mutex_;
    std::condition_variable cond_not_empty_;
    std::condition_variable cond_not_full_;
    std::condition_variable cond_pause_;
};





#endif //AVPACKETQUEUE_H
