//
// Created by Admin on 2025/6/30.
//

#include "include/Player.h"

#include <libavutil/time.h>

static int open_file(PlayerContext *state) {
    int ret = 0;
    ret = avformat_open_input(&state->fmt_ctx, state->filename, nullptr, nullptr);
    if (ret < 0) {
        av_log(nullptr, AV_LOG_ERROR, "Could not open file %s\n", state->filename);
        return -1;
    }
    if (state->fmt_ctx->nb_streams == 0) {
        ret = avformat_find_stream_info(state->fmt_ctx, nullptr);
        if (ret < 0) {
            av_log(nullptr, AV_LOG_ERROR, "Could not find stream information\n");
            return -1;
        }
    }
    return 0;
}

static int find_stream(PlayerContext *state) {
    state->audio_stream_idx = -1;
    state->video_stream_idx = -1;

    for (int i = 0; i < state->fmt_ctx->nb_streams; i++) {
        if (state->fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO && state->video_stream_idx == -1) {
            state->video_stream_idx = i;
        }
        if (state->fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO && state->audio_stream_idx == -1) {
            state->audio_stream_idx = i;
        }
        if (state->audio_stream_idx != -1 && state->video_stream_idx != -1) {
            state->jump_target_number = 5;
            return 0;
        }
    }

    if (state->audio_stream_idx == -1 && state->video_stream_idx == -1) {
        return -1;
    }

    state->jump_target_number = 3;

    return 0;
}

static AVCodecContext *open_codec(const AVStream *stream) {
    const AVCodec *codec = nullptr;
    AVCodecContext *codec_ctx = nullptr;
    int ret = 0;

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

    return codec_ctx;

    _ERROR:
    if (codec_ctx)
        avcodec_free_context(&codec_ctx);

    return nullptr;
}

static int open_video(PlayerContext *state, VideoDecoder *decoder, VideoRender *render) {
    if (state->video_stream_idx == -1) {
        return 0;
    }

    state->v_codec_ctx = open_codec(state->fmt_ctx->streams[state->video_stream_idx]);
    if (state->v_codec_ctx == nullptr) {
        return -1;
    }

    decoder->decode();
    render->start();

    return 0;
}

static void initAudioDevice(PlayerContext *player_ctx) {
    if (player_ctx->audio_stream_idx == -1) return;
    QAudioFormat format;
    format.setSampleRate(player_ctx->a_codec_ctx->sample_rate);
    format.setChannelCount(player_ctx->a_codec_ctx->ch_layout.nb_channels);
    format.setSampleFormat(QAudioFormat::Int16);

    QAudioDevice device = QMediaDevices::defaultAudioOutput();
    if (!device.isFormatSupported(format)) {
        format = device.preferredFormat();
    }

    player_ctx->audio_sink = new QAudioSink(device, format);
    player_ctx->audio_sink->setVolume(player_ctx->volume);
    player_ctx->audio_device = player_ctx->audio_sink->start();
}

static int open_audio(PlayerContext *state, AudioDecoder *decoder, AudioRender *render) {
    if (state->audio_stream_idx == -1) {
        return 0;
    }

    state->a_codec_ctx = open_codec(state->fmt_ctx->streams[state->audio_stream_idx]);
    if (state->a_codec_ctx == nullptr) {
        return -1;
    }

    initAudioDevice(state);

    decoder->decode();
    render->start();

    return 0;
}

Player::Player(QObject *parent) : QObject(parent) {
    initialized_ = true;
    progress_timer = new QTimer(this);
    player_ctx = new PlayerContext();
    packet_demux = new PacketDemux(player_ctx);
    audio_decoder = new AudioDecoder(player_ctx);
    video_decoder = new VideoDecoder(player_ctx);

    // if (!player_ctx || !packet_demux || !audio_decoder || !video_decoder) {
    //     initialized_ = false;
    //     return;
    // }

    video_render = new VideoRender(player_ctx);
    audio_render = new AudioRender(player_ctx);

    // if (!video_render || !audio_render) {
    //     initialized_ = false;
    //     return;
    // }

    connect(video_render, &VideoRender::onRefreshVideo, this, &Player::refreshVideoHandler);
    connect(progress_timer, &QTimer::timeout, this, &Player::sendProgressChanged);
    connect(video_render, &VideoRender::finished, this, &Player::finishedHandler);
    connect(audio_render, &AudioRender::finished, this, &Player::finishedHandler);
    progress_timer->setInterval(100);

    if (!player_ctx->initQueues() || !player_ctx->initPacketAndFrame()) {
        initialized_ = false;
        return;
    }
}

Player::~Player() {
    player_ctx->quit = 1;
    player_ctx->closeQueues();
    player_ctx->closeAudioDevice();
    progress_timer->stop();
    delete progress_timer;
    delete packet_demux;
    delete audio_decoder;
    delete video_decoder;
    delete video_render;
    delete audio_render;
    delete player_ctx;
}

int Player::setFile(const char *filename) const {
    if (!initialized_ || !filename) return -1;

    player_ctx->filename = av_strdup(filename);
    if (open_file(player_ctx) < 0) {
        return -1;
    }

    if (find_stream(player_ctx) < 0) {
        return -1;
    }

    return 0;
}

static void get_duration(PlayerContext *player_ctx) {
    double total_duration = 0.0;

    // 1. 优先用整体duration
    if (player_ctx->fmt_ctx->duration > 0) {
        total_duration = player_ctx->fmt_ctx->duration / (double)AV_TIME_BASE;
    } else {
        // 2. 否则用两个流里最长的那个
        double v_duration = 0.0, a_duration = 0.0;

        // 视频流存在
        if (player_ctx->video_stream_idx >= 0) {
            AVStream* v_stream = player_ctx->fmt_ctx->streams[player_ctx->video_stream_idx];
            if (v_stream && v_stream->duration > 0)
                v_duration = v_stream->duration * av_q2d(v_stream->time_base);
        }
        // 音频流存在
        if (player_ctx->audio_stream_idx >= 0) {
            AVStream* a_stream = player_ctx->fmt_ctx->streams[player_ctx->audio_stream_idx];
            if (a_stream && a_stream->duration > 0)
                a_duration = a_stream->duration * av_q2d(a_stream->time_base);
        }

        total_duration = std::max(v_duration, a_duration);
    }
    player_ctx->duration = total_duration;
}

int Player::start() {
    if (!initialized_) return -1;
    if (open_audio(player_ctx, audio_decoder, audio_render) < 0) {
        return -1;
    }

    if (open_video(player_ctx, video_decoder, video_render) < 0) {
        return -1;
    }

    get_duration(player_ctx);
    emit onProgressInit(player_ctx->duration);

    progress_timer->start();

    packet_demux->demux();

    return 0;
}

int Player::pause(bool b) const {
    if (!initialized_) return -1;
    player_ctx->pause = b;
    return 0;
}

int Player::isPlaying() const {
    if (!initialized_) return -1;
    return !player_ctx->pause;
}

int Player::reset() {
    player_ctx->quit = 1;

    progress_timer->stop();
    player_ctx->closeQueues();

    packet_demux->stopDemux();
    video_decoder->stopDecode();
    audio_decoder->stopDecode();

    video_render->wait();
    audio_render->wait();

    player_ctx->video_render->setFrame(nullptr);  // 这个要置空，不然小概率闪退
    player_ctx->closeAudioDevice();
    player_ctx->closeFile();
    player_ctx->openQueues();
    player_ctx->resetValue();
    return 0;
}

int Player::stop() {
    if (!initialized_) return -1;
    reset();
    return 0;
}

static int seek_position(int n, PlayerContext *player_ctx) {
    double target_seconds = n / 1000.0;
    int stream_idx = 0;

    // 设置跳转的流
    if (player_ctx->video_stream_idx != -1) {
        stream_idx = player_ctx->video_stream_idx;
    } else {
        stream_idx = player_ctx->audio_stream_idx;
    }

    AVFormatContext* fmt_ctx = player_ctx->fmt_ctx;
    AVStream* stream = fmt_ctx->streams[stream_idx];

    // 计算目标时间戳
    int64_t seek_target = target_seconds / av_q2d(stream->time_base);

    // 跳转
    int ret = av_seek_frame(fmt_ctx, stream_idx, seek_target, AVSEEK_FLAG_BACKWARD);

    // 跳转成功后，刷新所有解码器缓存
    if (ret >= 0) {
        if (player_ctx->video_stream_idx != -1) {
            avcodec_flush_buffers(player_ctx->v_codec_ctx);
        }
        if (player_ctx->audio_stream_idx != -1) {
            avcodec_flush_buffers(player_ctx->a_codec_ctx);
        }
    }
    return ret;
}

int Player::seek(int n) {
    // 检查是否能跳转
    if (!initialized_) return -1;
    if (!player_ctx->ready_to_jump || player_ctx->quit) return 0;

    // 设置跳转前参数
    player_ctx->jump = 1;
    player_ctx->jump_free = 0;
    if (!isPlaying()) pause(false);
    player_ctx->closeQueues();

    // 等全部线程都到跳转等待区（线程同步），防止有些帧还没放入队列中
    while (player_ctx->jump_busy != player_ctx->jump_target_number) {
        if (player_ctx->quit) return 0;
        player_ctx->resetEnd();
        // printf("%d\n", (int)player_ctx->jump_busy);
        QThread::msleep(5);
    }

    // 跳转到具体位置
    seek_position(n, player_ctx);

    // 恢复参数
    player_ctx->openQueues();
    player_ctx->jump_busy = 0;
    player_ctx->jump = 0;
    player_ctx->ready_to_jump = 0;

    // while (player_ctx->jump_free != player_ctx->jump_target_number) {
    //     printf("%d\n", (int)player_ctx->jump_free);
    //     QThread::msleep(5);
    // }

    // QThread::msleep(20);

    return 0;
}

int Player::forward() {
    if (!initialized_) return -1;
    seek(std::min((player_ctx->audio_offset + 10 * 1000 * 1000) / 1000, (int64_t)(player_ctx->duration * 1000)));
    return 0;
}

int Player::backward() {
    if (!initialized_) return -1;
    seek(std::max((player_ctx->audio_offset - 2 * 1000 * 1000) / 1000, 0ll));
    return 0;
}

int Player::setVolume(int volume) {
    if (!initialized_) return -1;
    float vol = volume / 100.0;
    if (player_ctx->audio_sink)
        player_ctx->audio_sink->setVolume(vol);

    player_ctx->volume = vol;
    player_ctx->mute = false;
    return 0;
}

float Player::getVolume() const {
    return player_ctx->volume;
}

int Player::setMute(bool mute) {
    if (!initialized_) return -1;
    if (!player_ctx->filename) return 0;
    player_ctx->mute = mute;
    if (player_ctx->mute) {
        player_ctx->audio_sink->setVolume(0);
    } else {
        player_ctx->audio_sink->setVolume(player_ctx->volume);
    }
    return 0;
}

int Player::getMute() const {
    return player_ctx->mute;
}

int Player::setSpeed(int idx) {
    if (!initialized_) return -1;
    player_ctx->speed_idx = idx;
    return 0;
}

int Player::haveVideo() {
    return player_ctx->video_stream_idx != -1;
}

int Player::setVideoRenderWidgets(VideoRenderWidget *renderWidgets) {
    player_ctx->video_render = renderWidgets;
    return 0;
}

void Player::refreshVideoHandler() {
    emit onRefreshVideo();
}

void Player::sendProgressChanged() {
    if (player_ctx->audio_stream_idx != -1)
        emit onProgressChange((double)player_ctx->audio_offset / 1000000);
    else
        emit onProgressChange((double)player_ctx->video_offset / 1000000);
}

void Player::finishedHandler() {
    reset();
}
