//
// Created by Admin on 2025/7/1.
//

#include "include/AudioFilter.h"

AudioFilter::AudioFilter(PlayerContext *ctx) : player_ctx(ctx) {
}

AudioFilter::~AudioFilter() {
    if (graph) {
        avfilter_graph_free(&graph);
        graph = nullptr;
        buf_ctx = nullptr;
        sink_ctx = nullptr;
    }
}

int AudioFilter::initFilter(double speed) {
    AVFilterInOut *inputs = nullptr;
    AVFilterInOut *outputs = nullptr;
    AVFilterGraph *graph = nullptr;
    AVFilterContext *buf_ctx = nullptr;
    AVFilterContext *sink_ctx = nullptr;
    const AVFilter *buf_filter = nullptr;
    const AVFilter *sink_filter = nullptr;
    char args[512] = {0};
    char filter_desc[64];
    int ret = 0;
    AVCodecContext *dec_ctx = player_ctx->a_codec_ctx;
    char ch_layout_str[64] = {0};
    enum AVSampleFormat sample_fmts[2];

    // 分配滤镜图
    graph = avfilter_graph_alloc();
    if (!graph) {
        goto __ERROR;
    }

    // 分配输入/输出点
    inputs = avfilter_inout_alloc();
    outputs = avfilter_inout_alloc();
    if (!inputs || !outputs) {
        goto __ERROR;
    }

    // 获取滤镜定义
    buf_filter = avfilter_get_by_name("abuffer");
    sink_filter = avfilter_get_by_name("abuffersink");
    if (!buf_filter || !sink_filter) {
        goto __ERROR;
    }

    // 构造 abuffer 参数
    av_channel_layout_describe(&dec_ctx->ch_layout, ch_layout_str, sizeof(ch_layout_str));

    snprintf(args, sizeof(args),
        "sample_rate=%d:sample_fmt=%s:channels=%d:channel_layout=0x%" PRIx64,
        dec_ctx->sample_rate,
        av_get_sample_fmt_name(AV_SAMPLE_FMT_S16),
        dec_ctx->ch_layout.nb_channels,
        dec_ctx->ch_layout.u.mask);


    // 创建输入滤镜（abuffer）
    ret = avfilter_graph_create_filter(&buf_ctx, buf_filter, "in", args, nullptr, graph);
    if (ret < 0) {
        goto __ERROR;
    }

    // 创建输出滤镜（abuffersink）
    ret = avfilter_graph_create_filter(&sink_ctx, sink_filter, "out", nullptr, nullptr, graph);
    if (ret < 0) {
        goto __ERROR;
    }

    // 设置允许的采样格式
    sample_fmts[0] = AV_SAMPLE_FMT_S16;
    sample_fmts[1] = AV_SAMPLE_FMT_NONE;
    av_opt_set_int_list(sink_ctx, "sample_fmts", sample_fmts, AV_SAMPLE_FMT_NONE, AV_OPT_SEARCH_CHILDREN);

    // 配置连接关系
    inputs->name = av_strdup("out");
    inputs->filter_ctx = sink_ctx;
    inputs->pad_idx = 0;
    inputs->next = nullptr;

    outputs->name = av_strdup("in");
    outputs->filter_ctx = buf_ctx;
    outputs->pad_idx = 0;
    outputs->next = nullptr;

    // 构建 atempo 滤镜描述
    if (speed >= 0.5 && speed <= 2.0) {
        snprintf(filter_desc, sizeof(filter_desc), "atempo=%f", speed);
    } else if (speed > 2.0 && speed <= 4.0) {
        snprintf(filter_desc, sizeof(filter_desc), "atempo=2.0,atempo=%f", speed/2.0);
    } else if (speed < 0.5 && speed >= 0.25) {
        snprintf(filter_desc, sizeof(filter_desc), "atempo=0.5,atempo=%f", speed/0.5);
    } else {
        goto __ERROR;
    }

    // 解析滤镜链路
    ret = avfilter_graph_parse_ptr(graph, filter_desc, &inputs, &outputs, nullptr);
    if (ret < 0) {
        goto __ERROR;
    }

    // 配置滤镜图
    ret = avfilter_graph_config(graph, nullptr);
    if (ret < 0) {
        goto __ERROR;
    }

    // 成功后保存上下文
    this->buf_ctx = buf_ctx;
    this->sink_ctx = sink_ctx;
    this->graph = graph;
    initialized = true;
    return 0;

__ERROR:
    avfilter_graph_free(&graph);
    avfilter_inout_free(&inputs);
    avfilter_inout_free(&outputs);
    return -1;
}

int AudioFilter::filterIn(AVFrame *inFrame) {
    int ret = 0;
    // 输入帧送入滤镜
    ret = av_buffersrc_add_frame(buf_ctx, inFrame);
    if (ret < 0) {
        char buf[128];
        av_strerror(ret, buf, sizeof(buf));
        printf("av_buffersrc_add_frame failed %s\n", buf);
        return -1;
    }
    return 0;
}

int AudioFilter::filterOut(AVFrame *outFrame) {
    int ret = 0;
    ret = av_buffersink_get_frame(sink_ctx, outFrame);
    if (ret == AVERROR_EOF || ret == AVERROR(EAGAIN)) {
        return ret;
    }
    if (ret < 0) {
        return -1;
    }
    return 0;
}

int AudioFilter::isValid() {
    return initialized;
}

