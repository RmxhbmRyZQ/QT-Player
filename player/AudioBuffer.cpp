//
// Created by Admin on 2025/7/1.
//

#include "include/AudioBuffer.h"
#include <cstring>
#include <iostream>

AudioBuffer::AudioBuffer(size_t cap)
    : capacity(cap),
      buffer_size(0),
      buffer(new uint8_t[cap]),
      write_pos(0),
      read_pos(0) {}

AudioBuffer::~AudioBuffer() {
    delete[] buffer;
}

bool AudioBuffer::isValid() const {
    return buffer != nullptr;
}

size_t AudioBuffer::size() {
    std::lock_guard<std::mutex> lock(mtx);
    return buffer_size;
}

bool AudioBuffer::writeFrame(const AVFrame* frame, const int per_sample_len) {
    if (!frame || !frame->data[0] || per_sample_len <= 0)
        return false;
    return writeRaw(frame->data[0], frame->nb_samples * per_sample_len);
}

bool AudioBuffer::writeRaw(const uint8_t* data, size_t len) {
    if (!data || len == 0)
        return false;

    std::unique_lock<std::mutex> lock(mtx);
    not_full.wait(lock, [&]() {
        return closed || paused || (capacity - buffer_size >= len);
    });

    if (closed || paused)
        return false;

    // 写入数据（分两段处理）
    size_t first = std::min(len, capacity - write_pos);
    std::memcpy(buffer + write_pos, data, first);
    std::memcpy(buffer, data + first, len - first);
    write_pos = (write_pos + len) % capacity;
    buffer_size += len;

    not_empty.notify_one();
    return true;
}

bool AudioBuffer::readFrame(AVFrame* frame, int per_sample_len) {
    if (!frame || !frame->data[0] || per_sample_len <= 0)
        return false;
    return readRaw(frame->data[0], frame->nb_samples * per_sample_len);
}

bool AudioBuffer::readRaw(uint8_t* dst, size_t len) {
    if (!dst || len == 0)
        return false;

    std::unique_lock<std::mutex> lock(mtx);
    not_empty.wait(lock, [&]() {
        return closed || paused || (buffer_size >= len);
    });

    if (closed || paused)
        return false;

    // 读取数据（分两段处理）
    size_t first = std::min(len, capacity - read_pos);
    std::memcpy(dst, buffer + read_pos, first);
    std::memcpy(dst + first, buffer, len - first);
    read_pos = (read_pos + len) % capacity;
    buffer_size -= len;

    not_full.notify_one();
    return true;
}

void AudioBuffer::clear() {
    std::lock_guard<std::mutex> lock(mtx);
    read_pos = 0;
    write_pos = 0;
    buffer_size = 0;
    not_full.notify_all();  // 唤醒写线程
}

void AudioBuffer::close() {
    std::lock_guard<std::mutex> lock(mtx);
    closed = true;
    not_empty.notify_all();
    not_full.notify_all();
}

void AudioBuffer::open() {
    std::lock_guard<std::mutex> lock(mtx);
    closed = false;
}

void AudioBuffer::pause() {
    setPaused(true);
}

void AudioBuffer::resume() {
    setPaused(false);
}

void AudioBuffer::setPaused(bool pause_flag) {
    paused = pause_flag;
    if (!pause_flag) {
        not_empty.notify_all();
        not_full.notify_all();
    }
}

bool AudioBuffer::isPaused() const {
    return paused;
}
