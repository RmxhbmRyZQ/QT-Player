//
// Created by Admin on 2025/7/1.
//

#include "include/AVPacketQueue.h"

AVPacketQueue::AVPacketQueue(size_t capacity)
    : capacity_(capacity), head_(0), tail_(0), size_(0),
      initialized_(true), is_closed_(false), paused_(false) {

    buffer_.resize(capacity_);
    for (size_t i = 0; i < capacity_; ++i) {
        buffer_[i] = av_packet_alloc();
        if (!buffer_[i]) {
            initialized_ = false;
            break;
        }
    }

    if (!initialized_) {
        for (auto& pkt : buffer_) {
            if (pkt) av_packet_free(&pkt);
        }
    }
}

AVPacketQueue::~AVPacketQueue() {
    clear();
    for (auto& pkt : buffer_) {
        av_packet_free(&pkt);
    }
}

bool AVPacketQueue::isValid() const {
    return initialized_;
}

bool AVPacketQueue::push(AVPacket* pkt) {
    if (!initialized_ || !pkt || pkt->size <= 0) return false;

    std::unique_lock<std::mutex> lock(mutex_);
    cond_not_full_.wait(lock, [this]() {
        return (size_ < capacity_ && !paused_) || is_closed_;
    });

    if (is_closed_ || paused_) return false;

    AVPacket* dest = buffer_[tail_];
    // av_packet_unref(dest);
    if (pkt->data && pkt->size > 0) {
        av_packet_move_ref(dest, pkt);
    } else {
        return false;
    }

    tail_ = (tail_ + 1) % capacity_;
    ++size_;
    cond_not_empty_.notify_one();
    return true;
}

bool AVPacketQueue::pop(AVPacket* out_pkt) {
    std::unique_lock<std::mutex> lock(mutex_);
    cond_not_empty_.wait(lock, [this]() {
        return (size_ > 0 && !paused_) || is_closed_;
    });

    if (is_closed_ || (paused_ && size_ == 0)) return false;

    AVPacket* src = buffer_[head_];
    av_packet_move_ref(out_pkt, src);
    head_ = (head_ + 1) % capacity_;
    --size_;
    cond_not_full_.notify_one();
    return true;
}

void AVPacketQueue::pause() {
    std::lock_guard<std::mutex> lock(mutex_);
    paused_ = true;
}

void AVPacketQueue::resume() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        paused_ = false;
    }
    cond_not_full_.notify_all();
    cond_not_empty_.notify_all();
}

void AVPacketQueue::setPaused(bool pause) {
    pause ? this->pause() : this->resume();
}

bool AVPacketQueue::isPaused() {
    std::lock_guard<std::mutex> lock(mutex_);
    return paused_;
}

void AVPacketQueue::notify() {
    std::lock_guard<std::mutex> lock(mutex_);
    tail_ = (tail_ + 1) % capacity_;
    ++size_;
    cond_not_empty_.notify_one();
}

void AVPacketQueue::close() {
    if (is_closed_) return;
    std::unique_lock<std::mutex> lock(mutex_);
    is_closed_ = true;
    cond_not_empty_.notify_all();
    cond_not_full_.notify_all();
}

void AVPacketQueue::open() {
    std::unique_lock<std::mutex> lock(mutex_);
    is_closed_ = false;
}

size_t AVPacketQueue::size() {
    std::unique_lock<std::mutex> lock(mutex_);
    return size_;
}

void AVPacketQueue::clear() {
    std::unique_lock<std::mutex> lock(mutex_);
    for (size_t i = 0; i < capacity_; ++i) {
        if (buffer_[i]) av_packet_unref(buffer_[i]);
    }
    head_ = tail_ = size_ = 0;
}

