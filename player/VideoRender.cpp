//
// Created by Admin on 2025/7/1.
//

#include "include/VideoRender.h"

static int64_t calculate_wait_time(PlayerContext *state, int64_t pts, int64_t last_pts) {
    if (state->audio_stream_idx != -1) {
        // 视频等待时间，当视频比音频快时，等待一些时间在显示帧，从而音视频同步
        // 当音频比视频快时，直接快速播放帧，即等待时间为 0，从而音视频同步
        state->video_offset = (int64_t)((double)(pts) * av_q2d(state->fmt_ctx->streams[state->video_stream_idx]->time_base) * 1000000);

        int64_t wait_time = (state->video_offset - state->audio_offset) / 1000;
        if (wait_time < 0) {
            wait_time = 0;
        }

        return wait_time;
    }

    // 如果只有视频流
    if (state->base_time == 0) {
        state->base_time = av_gettime();
    }

    // 两个包间的等待事件，加入了倍速元素
    int64_t diff = (int64_t)((double)(pts - last_pts) * av_q2d(state->fmt_ctx->streams[state->video_stream_idx]->time_base) * 1000000 / state->speeds[state->speed_idx]);

    // 采用绝对值同步，以 base_time 为绝对值底
    int64_t wait_time = (state->base_time + diff + state->video_offset - av_gettime()) / 1000;
    state->video_offset += diff;

    if (wait_time < 0) {
        wait_time = 0;
    }

    return wait_time;
}

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
            double offset = (double)frame->pts * av_q2d(stream->time_base) * 1000000;
            player_ctx->video_offset = offset;
            player_ctx->base_time = av_gettime() - offset;
            last_pts = frame->pts;
            player_ctx->ready_to_jump = 1;
        }

        int64_t pts = frame->pts;  // 在 setFrame 中会 move_ref 所以要提前获取 pts

        // 更新播放界面
        player_ctx->video_render->setFrame(frame);
        emit onRefreshVideo();

        // 进行视频同步
        int64_t wait_time = calculate_wait_time(player_ctx, pts, last_pts);
        last_pts = pts;
        if (wait_time < 1000) {
            // printf("wait_time: %lld\n", wait_time);
            QThread::msleep(wait_time);
        }
    }
}
