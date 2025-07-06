//
// Created by Admin on 2025/7/1.
//

#include "include/AudioRender.h"

AudioRender::AudioRender(PlayerContext *ctx) : player_ctx(ctx) {
    for (int i = 0; i < FILTER_NUMBER; i++) {
        audio_filters[i] = new AudioFilter(ctx);
    }
}

AudioRender::~AudioRender() {
    this->wait();
    for (int i = 0; i < FILTER_NUMBER; i++) {
        delete audio_filters[i];
        audio_filters[i] = nullptr;
    }
}

bool AudioRender::readFrame(AVFrame *frame, int sample_stride) {
    if (player_ctx->audio_decoder_end) {
        // 结束时，队列还有数据，就先读取数据
        if (player_ctx->audio_buffer->size() >= frame->nb_samples * sample_stride) {
            return player_ctx->audio_buffer->readFrame(frame, sample_stride);
        }
        player_ctx->audio_render_end = true;
        // 当音视频渲染都结束了，那么就发送信号
        if (player_ctx->video_stream_idx == -1 || player_ctx->video_render_end) {
            emit finished();
            return false;
        }
        // 如果队列里面没数据，但是还没有结束，那么就放入静音，保持音视频同步
        memset(frame->data[0], 0, frame->nb_samples * sample_stride);
        return true;
    }
    return player_ctx->audio_buffer->readFrame(frame, sample_stride);
}

void AudioRender::run() {
    AVFrame* frame = av_frame_alloc();
    if (!frame) return;

    frame->format = AV_SAMPLE_FMT_S16;
    frame->nb_samples = 1024;
    frame->sample_rate = player_ctx->a_codec_ctx->sample_rate;
    frame->ch_layout = player_ctx->a_codec_ctx->ch_layout;

    if (av_frame_get_buffer(frame, 32) < 0) {
        av_frame_free(&frame);
        return;
    }

    AVFrame* frame_filtered = av_frame_alloc();
    if (!frame_filtered) return;

    const int bytes_per_sample = 2;
    const int channels = frame->ch_layout.nb_channels;
    const int sample_stride = bytes_per_sample * channels;
    const int sleep_time = 10;

    // 新增：时间相关参数
    const int max_write_ms = 20;   // 每次最多写入20ms
    const int max_cache_ms = 30;   // 缓存超过30ms就等待
    const int bytes_per_ms = frame->sample_rate * channels * bytes_per_sample / 1000;
    const int max_write_bytes = bytes_per_ms * max_write_ms;
    const int max_cached_bytes = bytes_per_ms * max_cache_ms;

    while (player_ctx->quit == 0) {
        player_ctx->waitJump((char*)"Audio Render");

        while (player_ctx->pause && player_ctx->quit == 0) {
            QThread::msleep(10);
        }

        int free_bytes = player_ctx->audio_sink->bytesFree();
        int cached_bytes = player_ctx->audio_sink->bufferSize() - free_bytes;

        // 缓存超过30ms就等一等，保证低延迟
        if (cached_bytes > max_cached_bytes) {
            QThread::msleep(sleep_time);
            continue;
        }

        if (player_ctx->speed_idx == DEFAULT_SPEED_IDX) {
            // 1 倍速播放，不需要使用滤波器
            int frame_bytes = frame->nb_samples * bytes_per_sample * channels;

            if (!readFrame(frame, sample_stride)) {
                QThread::msleep(sleep_time);
                continue;
            }
            // 更新同步时间
            player_ctx->audio_offset += frame->nb_samples * 1000000 / frame->sample_rate;

            player_ctx->audio_device->write(reinterpret_cast<char*>(frame->data[0]), frame_bytes);
        } else {
            // 使用滤波器实现倍速播放
            AudioFilter *filter = audio_filters[player_ctx->speed_idx];
            if (!filter->isValid()) {
                int ret = filter->initFilter(player_ctx->speeds[player_ctx->speed_idx]);
                if (ret < 0) {
                    qDebug() << "filter init failed";
                    return;
                }
            }

            while (player_ctx->quit == 0) {
                // 读出数据
                int ret = filter->filterOut(frame_filtered);

                // 如果读取失败，说明还需要放入数据才能滤波
                if (ret == AVERROR(EAGAIN)) {
                    if (!readFrame(frame, sample_stride)) {
                        QThread::msleep(sleep_time);
                        continue;
                    }

                    // 更新同步时间
                    player_ctx->audio_offset += frame->nb_samples * 1000000 / frame->sample_rate;
                    filter->filterIn(frame);
                    // 如果滤波完如果 frame 被 unref 就重新分配
                    if (frame->data[0] == nullptr) {
                        frame->format = AV_SAMPLE_FMT_S16;
                        frame->nb_samples = 1024;
                        frame->sample_rate = player_ctx->a_codec_ctx->sample_rate;
                        frame->ch_layout = player_ctx->a_codec_ctx->ch_layout;

                        if (av_frame_get_buffer(frame, 32) < 0) {
                            av_frame_free(&frame);
                            return;
                        }
                    }
                }

                if (ret >= 0) {
                    break;
                }
            }


            int frame_bytes = frame_filtered->nb_samples * bytes_per_sample * channels;

            player_ctx->audio_device->write(reinterpret_cast<char*>(frame_filtered->data[0]), frame_bytes);
            av_frame_unref(frame_filtered);
        }
    }

    av_frame_free(&frame);
    av_frame_free(&frame_filtered);
}
