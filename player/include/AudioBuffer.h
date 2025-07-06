//
// Created by Admin on 2025/7/1.
//

#ifndef AUDIO_BUFFER_H
#define AUDIO_BUFFER_H

extern "C" {
#include <libavutil/frame.h>
#include <libavutil/samplefmt.h>
}

#include <mutex>
#include <condition_variable>
#include <atomic>

class AudioBuffer {
public:
    explicit AudioBuffer(size_t capacity);
    ~AudioBuffer();

    bool isValid() const;
    size_t size();

    // 写入接口
    bool writeFrame(const AVFrame* frame, int per_sample_len);                      // 从 AVFrame 写入
    bool writeRaw(const uint8_t* data, size_t len);   // 从裸数组写入

    // 读取接口
    bool readFrame(AVFrame* frame, int per_sample_len);                       // 读出并填入 AVFrame
    bool readRaw(uint8_t* dst, size_t len);           // 读出为裸 PCM

    // 控制接口
    void clear();          // 清空缓冲
    void close();          // 关闭队列
    void open();
    void pause();
    void resume();
    void setPaused(bool pause);
    bool isPaused() const;

private:
    uint8_t* buffer = nullptr;
    size_t buffer_size = 0;
    size_t capacity = 0;

    size_t write_pos = 0;
    size_t read_pos = 0;

    std::mutex mtx;
    std::condition_variable not_empty, not_full;
    std::atomic<bool> paused{false};
    std::atomic<bool> closed{false};
};

#endif // AUDIO_BUFFER_H


