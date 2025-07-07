//
// Created by Admin on 2025/7/1.
//

#include "include/VideoRender.h"

VideoRender::VideoRender(PlayerContext *ctx) : player_ctx(ctx) {
}

VideoRender::~VideoRender() {
    this->wait();
}

void VideoRender::run() {
    AVFrame *frame = av_frame_alloc();
    if (!frame) {
        return;
    }

    while (player_ctx->audio_stream_idx != -1 && player_ctx->video_offset - player_ctx->audio_offset > 10 && player_ctx->quit == 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    while (player_ctx->quit == 0) {
        player_ctx->waitJump((char*)"Video Render");

        // 暂停逻辑
        if (player_ctx->pause) {
            while (player_ctx->pause && player_ctx->quit == 0) {
                QThread::msleep(10);
            }

            // 如果没有音频流，就需要调整视频的同步数值（因为视频以音频为同步点）
            if (player_ctx->audio_stream_idx == -1) {
                player_ctx->ready_to_jump = 0;
            }
        }

        // 获取帧，并添加结束逻辑，当解码器结束了且队列也用完时结束
        if (!player_ctx->video_decoder_end || player_ctx->v_frame_queue->size() > 0) {
            if (!player_ctx->v_frame_queue->pop(frame))
                continue;
        } else {
            player_ctx->video_render_end = true;
            while (player_ctx->video_render_end && !player_ctx->quit) {
                // 当音视频渲染都结束了，那么就发送信号
                if (player_ctx->audio_stream_idx == -1 || player_ctx->audio_render_end) {
                    emit finished();
                    break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            continue;
        }

        // 只有视频流时的跳转逻辑，重新定义一下视频等待逻辑
        if (!player_ctx->ready_to_jump && player_ctx->audio_stream_idx == -1) {
            AVStream *stream = player_ctx->fmt_ctx->streams[player_ctx->video_stream_idx];
            double offset = 0.0;
            if (frame->pts >= 0)
                offset = (double)frame->pts * av_q2d(stream->time_base) * 1000000;
            else
                offset = player_ctx->first_frame_pts * av_q2d(stream->time_base) * 1000000;
            player_ctx->video_offset = offset;
            speed_offset = offset;
            player_ctx->base_time = av_gettime() - offset;
            last_pts = frame->pts;
            player_ctx->ready_to_jump = 1;
        }

        int64_t pts = frame->pts;  // 在 setFrame 中会 move_ref 所以要提前获取 pts

        // 更新播放界面
        player_ctx->video_render->setFrame(frame);
        emit onRefreshVideo();

        // 进行视频同步
        int64_t wait_time = calculateWaitTime(pts);

        last_pts = pts;
        if (wait_time < 1000) {
            // printf("wait_time: %lld\n", wait_time);
            QThread::msleep(wait_time);
        }
    }
}

int64_t VideoRender::calculateWaitTime(int64_t pts) {
    PlayerContext *state = player_ctx;
    if (state->audio_stream_idx != -1) {
        // 视频等待时间，当视频比音频快时，等待一些时间在显示帧，从而音视频同步
        // 当音频比视频快时，直接快速播放帧，即等待时间为 0，从而音视频同步
        if (pts >= 0)
            state->video_offset = (int64_t)((double)(pts) * av_q2d(state->fmt_ctx->streams[state->video_stream_idx]->time_base) * 1000000);
        else
            state->video_offset += state->frame_duration * 1000;

        // 可能不是非常精准，但播放的效果也足够了
        int64_t wait_time = (state->video_offset - state->audio_offset) / (1000 * state->speeds[state->speed_idx]);
        if (wait_time < 0) {
            wait_time = 0;
        }

        return wait_time;
    }

    // 如果只有视频流
    if (state->base_time == 0) {
        state->base_time = av_gettime();
    }

    // 两个包间的等待事件，加入了倍速元素，并处理视频帧没有pts的情况
    int64_t diff = 0;
    if (pts >= 0)
        diff = (int64_t)((double)(pts - last_pts) * av_q2d(state->fmt_ctx->streams[state->video_stream_idx]->time_base) * 1000000);
    else
        diff = state->frame_duration * 1000;

    int diff_speed = diff / state->speeds[state->speed_idx];

    // 采用绝对值同步，以 base_time 为绝对值底
    int64_t wait_time = (state->base_time + diff_speed + speed_offset - av_gettime()) / 1000;
    state->video_offset += diff;
    speed_offset += diff_speed;

    if (wait_time < 0) {
        wait_time = 0;
    }

    return wait_time;
}
