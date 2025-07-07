//
// Created by Admin on 2025/6/30.
//

#include "include/VideoDecoder.h"

static int open_sws(PlayerContext *state) {
    constexpr AVPixelFormat target = AV_PIX_FMT_YUV420P;
    // constexpr AVPixelFormat target = AV_PIX_FMT_RGB24;

    if (state->v_codec_ctx->pix_fmt == target) {
        return 0;
    }

    struct SwsContext *sws_ctx = nullptr;
    sws_ctx = sws_getCachedContext(
        nullptr,  // 已有的 AVCodecContext，如果为 nullptr 会创建个新的
        state->v_codec_ctx->width,
        state->v_codec_ctx->height,
        state->v_codec_ctx->pix_fmt,
        state->v_codec_ctx->width,
        state->v_codec_ctx->height,
        target,
        SWS_BICUBIC,
        nullptr,  // 原始过滤器
        nullptr,  // 目的过滤器
        nullptr);

    if (sws_ctx == nullptr) {
        av_log(nullptr, AV_LOG_ERROR, "Could not initialize sws!\n");
        return -1;
    }

    state->sws_ctx = sws_ctx;

    return 0;
}

VideoDecoder::VideoDecoder(PlayerContext *ctx) : player_ctx(ctx) {
}

VideoDecoder::~VideoDecoder() {
    stopDecode();
}

int VideoDecoder::decode() {
    video_decode_thread = new std::thread(&VideoDecoder::videoDecode, this);
    return 0;
}

int VideoDecoder::stopDecode() {
    if (video_decode_thread) {
        if (video_decode_thread->joinable()) {
            video_decode_thread->join();
        }
        delete video_decode_thread;
        video_decode_thread = nullptr;
    }
    return 0;
}

void VideoDecoder::videoDecode() {
    int ret = 0;
    AVPacket *packet = nullptr;
    AVFrame *frame = nullptr;
    AVFrame *frame_yuv = nullptr;

    frame = av_frame_alloc();
    if (frame == nullptr) {
        goto __ERROR;
    }

    packet = av_packet_alloc();
    if (packet == nullptr) {
        goto __ERROR;
    }


    while (player_ctx->quit == 0) {
        player_ctx->waitJump((char*)"Video Decoder");
        // 处理 EOF 情况，未 eof 或者队列里面有 packet 执行正常操作，否者发送 nullptr
        if (!player_ctx->video_demux_end || player_ctx->v_packet_queue->size() > 0) {
            if (player_ctx->v_packet_queue->pop(packet) < 0) {
                goto __ERROR;
            }

            if (packet->data == nullptr) {
                continue;
            }

            if (player_ctx->first_frame_pts == -1)
                player_ctx->first_frame_pts = packet->pts;
            // 发送一个包到解码器
            ret = avcodec_send_packet(player_ctx->v_codec_ctx, packet);
            av_packet_unref(packet);
            if (ret < 0) {
                goto __ERROR;
            }
        } else {
            avcodec_send_packet(player_ctx->v_codec_ctx, nullptr);
        }

        while (player_ctx->quit == 0) {
            // 从解码器读取一帧信息
            ret = avcodec_receive_frame(player_ctx->v_codec_ctx, frame);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                if (ret == AVERROR_EOF) {
                    player_ctx->video_decoder_end = true;
                    // 用户可能在最后跳转到前面，先不结束
                    while (player_ctx->video_decoder_end && !player_ctx->quit) {
                        std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    }
                }
                break;
            }

            if (ret < 0) {
                goto __ERROR;
            }

            if (player_ctx->sws_ctx == nullptr) {
                if (open_sws(player_ctx) < 0) {
                    goto __ERROR;
                }
            }

            if (player_ctx->sws_ctx) {
                // 仅在宽高改变时分配帧
                if (frame_yuv == nullptr || frame_yuv->width != frame->width || frame_yuv->height != frame->height) {
                    if (frame_yuv) {
                        av_frame_free(&frame_yuv);
                    }

                    frame_yuv = av_frame_alloc();
                    if (frame_yuv == nullptr) {
                        goto __ERROR;
                    }
                    frame_yuv->format = AV_PIX_FMT_YUV420P;
                    frame_yuv->width = frame->width;
                    frame_yuv->height = frame->height;

                    ret = av_frame_get_buffer(frame_yuv, 32);
                    if (ret < 0) {
                        goto __ERROR;
                    }
                }

                // 在 v_frame_queue->push 中会使用 move_ref，导致 data 要重新分配
                if (frame_yuv->data[0] == nullptr) {
                    ret = av_frame_get_buffer(frame_yuv, 32);
                    if (ret < 0) {
                        goto __ERROR;
                    }
                }

                sws_scale(player_ctx->sws_ctx,
                    (uint8_t**)frame->data,
                    frame->linesize,
                    0,
                    frame->height,
                    frame_yuv->data,
                    frame_yuv->linesize);
                player_ctx->v_frame_queue->push(frame_yuv);
                av_frame_unref(frame);
            } else {
                player_ctx->v_frame_queue->push(frame);
            }
        }
    }

    av_frame_free(&frame);
    if (frame_yuv) av_frame_free(&frame_yuv);
    av_packet_free(&packet);

    return;
    __ERROR:

    if (frame) av_frame_free(&frame);
    if (frame_yuv) av_frame_free(&frame_yuv);
    if (packet) av_packet_free(&packet);
}
