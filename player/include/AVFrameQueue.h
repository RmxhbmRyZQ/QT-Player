//
// Created by Admin on 2025/7/1.
//

#ifndef AVFRAMEQUEUE_H
#define AVFRAMEQUEUE_H



#pragma once

extern "C" {
#include <libavutil/frame.h>
}

#include <vector>
#include <mutex>
#include <condition_variable>

class AVFrameQueue {
public:
    explicit AVFrameQueue(size_t capacity);
    ~AVFrameQueue();

    bool isValid() const;

    bool push(AVFrame* frame);     // 入队（move）
    bool pop(AVFrame* out_frame); // 出队（ref 出来）
    size_t size();

    void clear();      // 清空内容
    void close();      // 关闭队列
    void open();
    void pause();      // 暂停
    void resume();     // 恢复
    void setPaused(bool pause);
    bool isPaused();

private:
    size_t capacity_;
    std::vector<AVFrame*> buffer_;
    size_t head_, tail_, size_;
    bool initialized_;
    bool is_closed_;
    bool paused_;

    std::mutex mutex_;
    std::condition_variable cond_not_full_;
    std::condition_variable cond_not_empty_;
};




#endif //AVFRAMEQUEUE_H
