//
// Created by Admin on 2025/6/30.
//

#include "include/PacketDemux.h"

PacketDemux::PacketDemux(PlayerContext *ctx) : player_ctx(ctx) {
}

PacketDemux::~PacketDemux() {
    stopDemux();
}

int PacketDemux::demux() {
    demux_thread = new std::thread([this] {
        this->demuxPacket();
    });
    return 0;
}

int PacketDemux::stopDemux() {
    if (demux_thread) {
        if (demux_thread->joinable()) {
            demux_thread->join();
        }
        delete demux_thread;
        demux_thread = nullptr;
    }
    return 0;
}

void PacketDemux::demuxPacket() {
    AVPacket *packet = av_packet_alloc();
    int audio_miss_count = 0;
    int video_miss_count = 0;
    const int MAX_MISS_COUNT = PACKET_QUEUE_SIZE;
    const int64_t PTS_TIMEOUT = 1 * AV_TIME_BASE;
    const int64_t DURATION_TIMEOUT = 0.5 * AV_TIME_BASE;

    int64_t last_audio_pts = 0;
    int64_t last_video_pts = 0;

    // 获取一个 packet 并进行分发
    // 由于一个流可能会先于另外一个流结束，为了不相互影响（由于流间的同步机制），进行一些流结束的预测
    // 1.当包的 pts 与流的 duration 接近时
    // 2.当不同流的包数量差别过多时，如视频读了 20 个包，而音频没有，可以认为音频结束了读不到包
    // 3.当不同流的 pts 相差较大时，一般情况下不同流的包相差不会太大，这样才会播放顺利
    // 当满足上面三点时，可以认为该流已经结束了，但是这种预测还是会有小概率失败的
    while (player_ctx->quit == 0) {
        int ret = av_read_frame(player_ctx->fmt_ctx, packet);
        AVStream *a_stream = player_ctx->fmt_ctx->streams[player_ctx->audio_stream_idx];
        AVStream *v_stream = player_ctx->fmt_ctx->streams[player_ctx->video_stream_idx];

        if (ret >= 0) {
            if (packet->pts < 0) packet->pts = 0;
            if (packet->stream_index == player_ctx->video_stream_idx) {
                last_video_pts = av_rescale_q(packet->pts, v_stream->time_base, AVRational{1, AV_TIME_BASE}); // 转为微秒
                video_miss_count = 0;
                audio_miss_count++;

                // 视频流已判定提前结束后又遇到包
                if (player_ctx->video_decoder_end) {
                    printf("[WARN] video stream judged as ended, but new packet arrived (pts=%lld)\n", packet->pts);
                    av_packet_unref(packet);
                    continue;
                }

                player_ctx->v_packet_queue->push(packet);
            } else if (packet->stream_index == player_ctx->audio_stream_idx) {
                last_audio_pts = av_rescale_q(packet->pts, a_stream->time_base, AVRational{1, AV_TIME_BASE});
                audio_miss_count = 0;
                video_miss_count++;

                // 音频流已判定提前结束后又遇到包
                if (player_ctx->audio_demux_end) {
                    printf("[WARN] audio stream judged as ended, but new packet arrived (pts=%lld)\n", packet->pts);
                    av_packet_unref(packet);
                    continue;
                }

                player_ctx->a_packet_queue->push(packet);
            } else {
                av_packet_unref(packet);
            }

            // audio流判据
            if (!player_ctx->audio_demux_end) {
                int64_t audio_duration = av_rescale_q(a_stream->duration, a_stream->time_base, AVRational{1, AV_TIME_BASE});
                int64_t audio_duration_us = (a_stream->duration > 0) ? audio_duration : 0;

                if ((audio_duration_us == 0 || audio_duration_us - last_audio_pts < DURATION_TIMEOUT) &&
                    audio_miss_count >= MAX_MISS_COUNT && last_video_pts - last_audio_pts > PTS_TIMEOUT) {
                    player_ctx->audio_demux_end = true;
                    player_ctx->a_packet_queue->notify();
                    }
            }

            // video流判据
            if (!player_ctx->video_demux_end) {
                int64_t video_duration = av_rescale_q(v_stream->duration, v_stream->time_base, AVRational{1, AV_TIME_BASE});
                int64_t video_duration_us = (v_stream->duration > 0) ? video_duration : 0;

                if ((video_duration_us == 0 || video_duration_us - last_video_pts < DURATION_TIMEOUT) &&
                    video_miss_count >= MAX_MISS_COUNT && last_audio_pts - last_video_pts > PTS_TIMEOUT) {
                    player_ctx->video_demux_end = true;
                    player_ctx->v_packet_queue->notify();
                    }
            }

            player_ctx->waitJump((char*)"Packet Demux");
        } else {
            // 流读完时的操作，保持你的原始逻辑
            player_ctx->audio_demux_end = true;
            player_ctx->video_demux_end = true;
            while (player_ctx->audio_demux_end && player_ctx->video_demux_end && !player_ctx->quit) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }
    }

    av_packet_free(&packet);
}

