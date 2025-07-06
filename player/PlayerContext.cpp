//
// Created by Admin on 2025/7/1.
//

#include "include/PlayerContext.h"

#include <cstring>  // for memset

PlayerContext::PlayerContext() = default;

PlayerContext::~PlayerContext() {
    freePacketAndFrame();
    freeQueues();
    closeAudioDevice();
    closeFile();
}

bool PlayerContext::initPacketAndFrame() {
    v_packet = av_packet_alloc();
    a_packet = av_packet_alloc();
    v_frame  = av_frame_alloc();
    a_frame  = av_frame_alloc();

    return v_packet && a_packet && v_frame && a_frame;
}

bool PlayerContext::initQueues(size_t pkt_queue_size, size_t frame_queue_size, size_t audio_buffer_size) {
    v_packet_queue = new AVPacketQueue(pkt_queue_size);
    a_packet_queue = new AVPacketQueue(pkt_queue_size);
    v_frame_queue  = new AVFrameQueue(frame_queue_size);
    audio_buffer = new AudioBuffer(audio_buffer_size);

    if (!v_packet_queue->isValid() || !a_packet_queue->isValid()
        || !v_frame_queue->isValid() || !audio_buffer->isValid()) {
        return false;
    }

    return true;
}

void PlayerContext::closeFile() {
    if (swr_ctx) {
        swr_free(&swr_ctx);
    }

    if (sws_ctx) {
        sws_freeContext(sws_ctx);
    }

    if (temp_buf) {
        av_freep(&temp_buf);
        temp_buf_size = 0;
    }

    if (v_codec_ctx) {
        avcodec_free_context(&v_codec_ctx);
    }

    if (a_codec_ctx) {
        avcodec_free_context(&a_codec_ctx);
    }

    if (fmt_ctx) {
        avformat_close_input(&fmt_ctx);
    }

    if (filename) {
        av_free(filename);
        filename = nullptr;
    }
}

void PlayerContext::freeQueues() {
    if (v_packet_queue) {
        v_packet_queue->close();
        delete v_packet_queue;
        v_packet_queue = nullptr;
    }
    if (a_packet_queue) {
        a_packet_queue->close();
        delete a_packet_queue;
        a_packet_queue = nullptr;
    }
    if (v_frame_queue) {
        v_frame_queue->close();
        delete v_frame_queue;
        v_frame_queue = nullptr;
    }
    if (audio_buffer) {
        audio_buffer->close();
        delete audio_buffer;
        audio_buffer = nullptr;
    }
}

void PlayerContext::closeQueues() {
    if (v_packet_queue) {
        v_packet_queue->close();
        v_packet_queue->clear();
    }
    if (a_packet_queue) {
        a_packet_queue->close();
        a_packet_queue->clear();
    }
    if (v_frame_queue) {
        v_frame_queue->close();
        v_frame_queue->clear();
    }
    if (audio_buffer) {
        audio_buffer->close();
        audio_buffer->clear();
    }
}

void PlayerContext::openQueues() {
    if (v_packet_queue) {
        v_packet_queue->open();
    }
    if (a_packet_queue) {
        a_packet_queue->open();
    }
    if (v_frame_queue) {
        v_frame_queue->open();
    }
    if (audio_buffer) {
        audio_buffer->open();
    }
}

void PlayerContext::freePacketAndFrame() {
    if (v_packet) {
        av_packet_free(&v_packet);
    }
    if (a_packet) {
        av_packet_free(&a_packet);
    }
    if (v_frame) {
        av_frame_free(&v_frame);
    }
    if (a_frame) {
        av_frame_free(&a_frame);
    }
}

void PlayerContext::closeAudioDevice() {
    if (audio_sink) {
        audio_sink->stop();
        delete audio_sink;
        audio_sink = nullptr;
        audio_device = nullptr;
    }
}

void PlayerContext::resetJump() {
    jump = 0;
    jump_busy = 0;
    jump_free = 0;
    ready_to_jump = 0;
    jump_target_number = 0;
}

void PlayerContext::resetEnd() {
    demux_end = false;
    audio_demux_end = false;
    video_demux_end = false;
    audio_render_end = false;
    video_render_end = false;
    audio_decoder_end = false;
    video_decoder_end = false;
}

void PlayerContext::resetValue() {
    resetJump();
    resetEnd();

    audio_stream_idx = -1;
    video_stream_idx = -1;

    base_time = 0;
    audio_offset = 0;
    video_offset = 0;
    pause = 0;
    quit = 0;
}

void PlayerContext::waitJump(char *str) {
    if (jump) {
        // printf("%The invoked thread is s\n", str);
        jump_busy += 1;  // 增加等待的线程数
        while (jump && !quit) {  // 进入等待
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        jump_free += 1;
    }
}

