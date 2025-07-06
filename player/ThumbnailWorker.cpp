//
// Created by Admin on 2025/7/5.
//

#include "ThumbnailWorker.h"

#include <QThread>

#define PLAY_FRAME_FORMAT AV_PIX_FMT_YUV420P

ThumbnailWorker::ThumbnailWorker(QObject* parent)
    : QObject(parent) {
}

ThumbnailWorker::~ThumbnailWorker() {
    initialized =false;
    running = false;
    release();
}

int ThumbnailWorker::init(const char* filename) {
    int ret = 0;
    ret = avformat_open_input(&this->pFormatCtx, filename, nullptr, nullptr);
    if (ret < 0) {
        av_log(nullptr, AV_LOG_ERROR, "Could not open file %s\n", filename);
        return -1;
    }

    if (pFormatCtx->nb_streams == 0) {
        ret = avformat_find_stream_info(pFormatCtx, nullptr);
        if (ret < 0) {
            av_log(nullptr, AV_LOG_ERROR, "Could not find stream information\n");
            return -1;
        }
    }

    for (int i = 0; i < pFormatCtx->nb_streams; i++) {
        if (pFormatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            streamIdx = i;
            break;
        }
    }

    const AVStream* stream = pFormatCtx->streams[streamIdx];
    const AVCodec *codec = nullptr;
    AVCodecContext *codec_ctx = nullptr;

    // 查找解码器
    codec = avcodec_find_decoder(stream->codecpar->codec_id);
    if (codec == nullptr) {
        av_log(nullptr, AV_LOG_ERROR, "codec %d not found!\n", stream->codecpar->codec_id);
        goto _ERROR;
    }

    // 分配解码器 context
    codec_ctx = avcodec_alloc_context3(codec);
    if (codec_ctx == nullptr) {
        av_log(nullptr, AV_LOG_ERROR, "Could not allocate video codec context!\n");
        goto _ERROR;
    }

    // 拷贝输入流的解码器参数到上下文
    avcodec_parameters_to_context(codec_ctx, stream->codecpar);

    // 打开解码器
    ret = avcodec_open2(codec_ctx, codec, nullptr);
    if (ret < 0) {
        av_log(codec_ctx, AV_LOG_ERROR, "Fail to init decoder context!\n");
        goto _ERROR;
    }

    pCodecCtx = codec_ctx;

    frame = av_frame_alloc();
    if (frame == nullptr) {
        av_log(nullptr, AV_LOG_ERROR, "Could not allocate last frame!\n");
        goto _ERROR;
    }

    read_frame = av_frame_alloc();
    if (read_frame == nullptr) {
        av_log(nullptr, AV_LOG_ERROR, "Could not allocate last frame!\n");
        goto _ERROR;
    }

    packet = av_packet_alloc();
    if (packet == nullptr) {
        av_log(nullptr, AV_LOG_ERROR, "Could not allocate packet!\n");
        goto _ERROR;
    }

    initialized = true;

    return 0;

    _ERROR:
    release();

    return -1;
}

void ThumbnailWorker::reset() {
    while(running) {
        QThread::msleep(10);
    }
    release();
}

bool ThumbnailWorker::isRunning() {
    return running;
}

void ThumbnailWorker::onRequestPreview(int mouseX, int status, int64_t value) {
    if (!initialized) return;
    if (status == 1) {
        focus = true;
    }

    if (status == -1) {
        focus = false;
        emit thumbnailReady(nullptr, mouseX, status, value);
        return;
    }

    if (!focus) return;
    if (running) return;  // 防止过于频繁消耗性能
    running = true;

    int64_t target = ((double)value) / (1000 * av_q2d(pFormatCtx->streams[streamIdx]->time_base));
    int ret = av_seek_frame(pFormatCtx, streamIdx, target, AVSEEK_FLAG_BACKWARD);

    if (ret >= 0) {
        avcodec_flush_buffers(pCodecCtx);
    }

    if (readFrame() < 0) {
        goto __END;
    }

    if (scaleFrame() < 0) {
        goto __END;
    }

    if (focus) emit thumbnailReady(frame, mouseX, status, value);
    running = false;
    return;

__END:
    emit thumbnailReady(nullptr, mouseX, status, value);
    running = false;
}

void ThumbnailWorker::release() {
    if (frame) av_frame_free(&frame);
    if (pCodecCtx) avcodec_free_context(&pCodecCtx);
    if (pFormatCtx) avformat_close_input(&pFormatCtx);
    if (frame) av_frame_free(&frame);
    if (read_frame) av_frame_free(&read_frame);
    if (rescaled_frame) av_frame_free(&rescaled_frame);
    if (packet) av_packet_free(&packet);
    lastPts = -1;
    initialized = false;
    running = false;
}

int ThumbnailWorker::readFrame() {
    int ret = 0;
    while(true) {
        ret = av_read_frame(pFormatCtx, packet);
        if (ret < 0) {
            av_log(nullptr, AV_LOG_ERROR, "Could not read frame!\n");
            goto __END;
        }
        if (packet->stream_index != streamIdx) {
            av_packet_unref(packet);
            continue;
        }

        // 当包的 pts 与上一个 pts 相同，说明没跳到下一个关键帧，播放之前的
        if (packet->pts == lastPts) {
            av_packet_unref(packet);
            goto __END;
        }
        lastPts = packet->pts;

        ret = avcodec_send_packet(pCodecCtx, packet);
        av_packet_unref(packet);
        if (ret < 0) {
            av_log(nullptr, AV_LOG_ERROR, "Could not send packet!\n");
            goto __END;
        }

        ret = avcodec_receive_frame(pCodecCtx, read_frame);
        if (ret == AVERROR(EAGAIN)) {
            continue;
        }

        if (ret < 0) {
            goto __END;
        }
        break;
    }

    return 0;

    __END:

    return -1;
}

static int open_sws(AVCodecContext *codec_ctx, SwsContext *&ctx) {
    constexpr AVPixelFormat target = PLAY_FRAME_FORMAT;

    if (codec_ctx->pix_fmt == target) {
        return 0;
    }

    SwsContext *sws_ctx = nullptr;
    sws_ctx = sws_getCachedContext(
        nullptr,  // 已有的 AVCodecContext，如果为 nullptr 会创建个新的
        codec_ctx->width,
        codec_ctx->height,
        codec_ctx->pix_fmt,
        codec_ctx->width,
        codec_ctx->height,
        target,
        SWS_BICUBIC,
        nullptr,  // 原始过滤器
        nullptr,  // 目的过滤器
        nullptr);

    if (sws_ctx == nullptr) {
        av_log(nullptr, AV_LOG_ERROR, "Could not initialize sws!\n");
        return -1;
    }

    ctx = sws_ctx;

    return 0;
}

static int create_frame(AVFrame *&dst, AVFrame *&src) {
    int ret = 0;
    if (dst == nullptr || dst->width != src->width || dst->height != src->height) {
        if (dst) {
            av_frame_free(&dst);
        }

        dst = av_frame_alloc();
        if (dst == nullptr) {
            goto __ERROR;
        }
        dst->format = PLAY_FRAME_FORMAT;
        dst->width = src->width;
        dst->height = src->height;

        ret = av_frame_get_buffer(dst, 32);
        if (ret < 0) {
            goto __ERROR;
        }
    }
    return 0;
    __ERROR:
    return -1;
}

static void copy_frame_data(AVFrame* dst, const AVFrame* src) {
    if (!dst || !src) return;

    // 检查像素格式
    if (src->format == AV_PIX_FMT_RGB24 || src->format == AV_PIX_FMT_BGR24) {
        // 只拷贝 data[0]
        int h = src->height;
        int bytes = src->width * 3;
        for (int row = 0; row < h; ++row) {
            memcpy(dst->data[0] + row * dst->linesize[0],
                   src->data[0] + row * src->linesize[0],
                   bytes);
        }
    } else if (src->format == AV_PIX_FMT_YUV420P) {
        // plane 0: Y
        int hY = src->height;
        int wY = src->width;
        for (int row = 0; row < hY; ++row) {
            memcpy(dst->data[0] + row * dst->linesize[0],
                   src->data[0] + row * src->linesize[0],
                   wY);
        }
        // plane 1: U
        int hU = src->height / 2;
        int wU = src->width / 2;
        for (int row = 0; row < hU; ++row) {
            memcpy(dst->data[1] + row * dst->linesize[1],
                   src->data[1] + row * src->linesize[1],
                   wU);
        }
        // plane 2: V
        for (int row = 0; row < hU; ++row) {
            memcpy(dst->data[2] + row * dst->linesize[2],
                   src->data[2] + row * src->linesize[2],
                   wU);
        }
    } else {
        // 其它格式，直接 copy linesize[plane] 字节（不保证最优，但可以支持部分场景）
        int plane = 0;
        while (src->data[plane] && dst->data[plane]) {
            int h = src->height;
            for (int row = 0; row < h; ++row) {
                memcpy(dst->data[plane] + row * dst->linesize[plane],
                       src->data[plane] + row * src->linesize[plane],
                       src->linesize[plane]);
            }
            ++plane;
        }
    }
}

int ThumbnailWorker::scaleFrame() {
    if (swsCtx == nullptr) {
        if (open_sws(pCodecCtx, swsCtx) < 0) {
            goto __ERROR;
        }
    }

    create_frame(frame, read_frame);

    if (swsCtx) {
        // 仅在宽高改变时分配帧
        create_frame(rescaled_frame, read_frame);

        sws_scale(swsCtx,
            (uint8_t**)read_frame->data,
            read_frame->linesize,
            0,
            read_frame->height,
            rescaled_frame->data,
            rescaled_frame->linesize);
        copy_frame_data(frame, rescaled_frame);
        // av_frame_copy(frame, rescaled_frame);  // 闪退了，还是自己写吧
    } else {
        copy_frame_data(frame, read_frame);
        // av_frame_copy(frame, rescaled_frame);
    }
    av_frame_unref(read_frame);

    return 0;
    __ERROR:
    return -1;
}
