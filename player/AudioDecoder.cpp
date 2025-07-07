//
// Created by Admin on 2025/6/30.
//

#include "include/AudioDecoder.h"

#include <QMediaDevices>
#include <QThread>

static int open_swr(PlayerContext *state) {
    if (state->a_codec_ctx->sample_fmt == AV_SAMPLE_FMT_S16) {
        return 0;
    }
    SwrContext *swr_ctx = nullptr;
    int ret = 0;

    AVChannelLayout input_channel_layout, output_channel_layout;
    av_channel_layout_copy(&input_channel_layout, &state->a_codec_ctx->ch_layout);
    av_channel_layout_copy(&output_channel_layout, &input_channel_layout);

    ret = swr_alloc_set_opts2(&swr_ctx,
        &output_channel_layout,
        AV_SAMPLE_FMT_S16,
        state->a_codec_ctx->sample_rate,
        &input_channel_layout,
        state->a_codec_ctx->sample_fmt,
        state->a_codec_ctx->sample_rate,
        0,
        nullptr);

    if (ret < 0) {
        av_log(nullptr, AV_LOG_ERROR, "Failed to open sample rate\n");
        return -1;
    }

    ret = swr_init(swr_ctx);
    if (ret < 0) {
        av_log(nullptr, AV_LOG_ERROR, "Failed to initialize swr\n");
        swr_free(&swr_ctx);
        return -1;
    }

    state->swr_ctx = swr_ctx;

    return 0;
}

AudioDecoder::AudioDecoder(PlayerContext *ctx) {
    player_ctx = ctx;
}

AudioDecoder::~AudioDecoder() {
    stopDecode();
}

int AudioDecoder::decode() {
    audio_decode_thread = new std::thread(&AudioDecoder::audioDecode, this);
    return 0;
}

int AudioDecoder::stopDecode() {
    if (audio_decode_thread) {
        if (audio_decode_thread->joinable()) {
            audio_decode_thread->join();
        }
        delete audio_decode_thread;
        audio_decode_thread = nullptr;
    }
    return 0;
}

void AudioDecoder::audioDecode() {
    int ret = 0;
    AVFrame *frame = nullptr;
    AVPacket *packet = nullptr;


    frame = av_frame_alloc();
    if (frame == nullptr) {
        goto __ERROR;
    }

    packet = av_packet_alloc();
    if (packet == nullptr) {
        goto __ERROR;
    }

    while (player_ctx->quit == 0) {
        player_ctx->waitJump((char*)"Audio Decoder");

        // 处理 EOF 情况，未 eof 或者队列里面有 packet 执行正常操作，否者发送 nullptr
        if (!player_ctx->audio_demux_end || player_ctx->a_packet_queue->size() > 0) {
            if (player_ctx->a_packet_queue->pop(packet) < 0) {
                goto __ERROR;
            }

            if (packet->data == nullptr) {
                continue;
            }

            ret = avcodec_send_packet(player_ctx->a_codec_ctx, packet);
            av_packet_unref(packet);
            if (ret < 0) {
                goto __ERROR;
            }
        } else {
            avcodec_send_packet(player_ctx->a_codec_ctx, nullptr);
        }

        while (player_ctx->quit == 0) {
            ret = avcodec_receive_frame(player_ctx->a_codec_ctx, frame);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                if (ret == AVERROR_EOF) {
                    player_ctx->audio_decoder_end = true;
                    // 用户可能在最后跳转到前面，先不结束
                    while (player_ctx->audio_decoder_end && !player_ctx->quit) {
                        std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    }
                }
                break;
            }

            if (ret < 0) {
                goto __ERROR;
            }

            if (player_ctx->swr_ctx == nullptr) {
                if (open_swr(player_ctx) < 0)
                    goto __ERROR;
            }

            // 处理跳转后同步
            if (!player_ctx->ready_to_jump) {
                AVStream *stream = player_ctx->fmt_ctx->streams[player_ctx->audio_stream_idx];
                player_ctx->audio_offset = ((double)frame->pts * av_q2d(stream->time_base) - (double)frame->nb_samples / frame->sample_rate) * 1000000;
                player_ctx->video_offset = player_ctx->audio_offset;
                player_ctx->ready_to_jump = 1;
            }

            if (player_ctx->swr_ctx) {
                uint8_t * const *in = frame->extended_data;  // 视频通常使用 data，而音频通常使用 extended_data
                const int in_count = frame->nb_samples;
                uint8_t **out = &player_ctx->temp_buf;
                const int out_count = frame->nb_samples + 512;

                const int out_size = av_samples_get_buffer_size(nullptr,
                    frame->ch_layout.nb_channels,
                    out_count,
                    AV_SAMPLE_FMT_S16,
                    0);

                av_fast_malloc(&player_ctx->temp_buf, (unsigned int *)&player_ctx->temp_buf_size, out_size);

                const int nb_samples = swr_convert(player_ctx->swr_ctx,
                    out, out_count,
                    in, in_count);

                const int data_size = nb_samples * frame->ch_layout.nb_channels * av_get_bytes_per_sample(AV_SAMPLE_FMT_S16);
                player_ctx->audio_buffer->writeRaw(player_ctx->temp_buf, data_size);
            } else {
                const int data_size = av_samples_get_buffer_size(nullptr,
                    frame->ch_layout.nb_channels,
                    frame->nb_samples,
                    AV_SAMPLE_FMT_S16,
                    1);
                player_ctx->audio_buffer->writeRaw(frame->data[0], data_size);
            }

            av_frame_unref(frame);
        }
    }

    __ERROR:
    // char buffer[1024];
    // av_strerror(ret, buffer, sizeof(buffer));
    // printf("the error is: %s\n", buffer);
    if (frame) av_frame_free(&frame);
    if (packet) av_packet_free(&packet);
}
