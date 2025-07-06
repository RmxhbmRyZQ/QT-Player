//
// Created by Admin on 2025/7/1.
//

#include "include/AVFrameQueue.h"

AVFrameQueue::AVFrameQueue(size_t capacity)
    : capacity_(capacity), head_(0), tail_(0), size_(0),
      initialized_(true), is_closed_(false), paused_(false) {

    buffer_.resize(capacity_);
    for (size_t i = 0; i < capacity_; ++i) {
        buffer_[i] = av_frame_alloc();
        if (!buffer_[i]) {
            initialized_ = false;
            break;
        }
    }

    if (!initialized_) {
        for (auto& f : buffer_) {
            if (f) av_frame_free(&f);
        }
    }
}

AVFrameQueue::~AVFrameQueue() {
    clear();
    for (auto& f : buffer_) {
        av_frame_free(&f);
    }
}

bool AVFrameQueue::isValid() const {
    return initialized_;
}

bool AVFrameQueue::push(AVFrame* frame) {
    if (!initialized_ || !frame) return false;

    std::unique_lock<std::mutex> lock(mutex_);
    cond_not_full_.wait(lock, [this]() {
        return (size_ < capacity_ && !paused_) || is_closed_;
    });

    if (is_closed_ || paused_) return false;

    AVFrame* dest = buffer_[tail_];
    // av_frame_unref(dest);
    av_frame_move_ref(dest, frame);

    tail_ = (tail_ + 1) % capacity_;
    ++size_;
    cond_not_empty_.notify_one();
    return true;
}

bool AVFrameQueue::pop(AVFrame* out_frame) {
    std::unique_lock<std::mutex> lock(mutex_);
    cond_not_empty_.wait(lock, [this]() {
        return (size_ > 0 && !paused_) || is_closed_;
    });

    if (is_closed_ || (paused_ && size_ == 0)) return false;

    AVFrame* src = buffer_[head_];
    av_frame_move_ref(out_frame, src);
    // out_frame = buffer_[head_];
    head_ = (head_ + 1) % capacity_;
    --size_;
    cond_not_full_.notify_one();
    return true;
}

size_t AVFrameQueue::size() {
    std::unique_lock<std::mutex> lock(mutex_);
    return size_;
}

void AVFrameQueue::clear() {
    std::unique_lock<std::mutex> lock(mutex_);
    for (auto& f : buffer_) {
        if (f) av_frame_unref(f);
    }
    head_ = tail_ = size_ = 0;
}

void AVFrameQueue::close() {
    std::unique_lock<std::mutex> lock(mutex_);
    is_closed_ = true;
    cond_not_empty_.notify_all();
    cond_not_full_.notify_all();
}

void AVFrameQueue::open() {
    std::unique_lock<std::mutex> lock(mutex_);
    is_closed_ = false;
}

void AVFrameQueue::pause() {
    std::lock_guard<std::mutex> lock(mutex_);
    paused_ = true;
}

void AVFrameQueue::resume() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        paused_ = false;
    }
    cond_not_empty_.notify_all();
    cond_not_full_.notify_all();
}

void AVFrameQueue::setPaused(bool pause) {
    pause ? this->pause() : this->resume();
}

bool AVFrameQueue::isPaused() {
    std::lock_guard<std::mutex> lock(mutex_);
    return paused_;
}

