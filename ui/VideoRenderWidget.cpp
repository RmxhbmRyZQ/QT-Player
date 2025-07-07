//
// Created by Admin on 2025/7/1.
//

#include "VideoRenderWidget.h"

#include <mutex>
#include <QOpenGLWidget>
#include <QOpenGLShader>
#include <QDebug>


static const GLfloat vertices[] = {
    -1.0f, -1.0f,
     1.0f, -1.0f,
    -1.0f,  1.0f,
     1.0f,  1.0f,
};

static const GLfloat texCoords[] = {
    0.0f, 1.0f,
    1.0f, 1.0f,
    0.0f, 0.0f,
    1.0f, 0.0f,
};

VideoRenderWidget::VideoRenderWidget(QWidget* parent)
    : ::QOpenGLWidget(parent) {}

VideoRenderWidget::~VideoRenderWidget() {
    makeCurrent();
    glDeleteTextures(1, &textureY);
    glDeleteTextures(1, &textureU);
    glDeleteTextures(1, &textureV);
    doneCurrent();
    if (frame)
        av_frame_free(&frame);
}

void VideoRenderWidget::initializeGL() {
    initializeOpenGLFunctions();
    initShader();
    initTextures();
    glClearColor(0, 0, 0, 1);
}

void VideoRenderWidget::resizeGL(int w, int h) {
    glViewport(0, 0, w, h);
}

void VideoRenderWidget::mousePressEvent(QMouseEvent* event) {
    QOpenGLWidget::mousePressEvent(event);
    emit clicked();
}

void VideoRenderWidget::paintGL() {
    std::unique_lock<std::mutex> lock(mtx);  // 防止与 setFrame 对帧的使用冲突导致报错

    glClear(GL_COLOR_BUFFER_BIT);

    if (!yPlane) {
        return;
    }
    // 计算绘制区域保持视频比例
    double widgetW = width() * devicePixelRatio();
    double widgetH = height() * devicePixelRatio();
    double widgetRatio = widgetW / widgetH;
    double videoRatio = static_cast<double>(videoW) / videoH;

    double drawW, drawH, offsetX = 0, offsetY = 0;
    if (videoRatio > widgetRatio) {
        // 视频更宽，左右对齐，上下留黑
        drawW = widgetW;
        drawH = widgetW / videoRatio;
        offsetY = (widgetH - drawH) / 2;
    } else {
        // 视频更高，上下对齐，左右留黑
        drawH = widgetH;
        drawW = widgetH * videoRatio;
        offsetX = (widgetW - drawW) / 2;
    }
    // 设置绘制区域
    glViewport(static_cast<int>(offsetX), static_cast<int>(offsetY), static_cast<int>(drawW), static_cast<int>(drawH));

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textureY);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, videoW, videoH, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, yPlane);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, textureU);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, videoW / 2, videoH / 2, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, uPlane);

    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, textureV);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, videoW / 2, videoH / 2, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, vPlane);

    program.bind();
    program.enableAttributeArray(0);
    program.enableAttributeArray(1);
    program.setAttributeArray(0, GL_FLOAT, vertices, 2);
    program.setAttributeArray(1, GL_FLOAT, texCoords, 2);

    program.setUniformValue("texY", 0);
    program.setUniformValue("texU", 1);
    program.setUniformValue("texV", 2);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    program.disableAttributeArray(0);
    program.disableAttributeArray(1);
    program.release();

}

void VideoRenderWidget::setFrame(AVFrame *frame) {
    std::unique_lock<std::mutex> lock(mtx);

    if (this->frame) {
        av_frame_unref(this->frame);
    } else {
        this->frame = av_frame_alloc();
    }

    if (frame == nullptr) {
        av_frame_free(&this->frame);
        yPlane = nullptr;
        uPlane = nullptr;
        vPlane = nullptr;
        videoW = 0;
        videoH = 0;
        return;
    }

    av_frame_move_ref(this->frame, frame);
    yPlane = this->frame->data[0];
    uPlane = this->frame->data[1];
    vPlane = this->frame->data[2];
    videoW = this->frame->width;
    videoH = this->frame->height;
}

int VideoRenderWidget::copyFrameToClipboard() {
    if (frame == nullptr) return 0;
    // 1. 查找 MJPEG 编码器
    const AVCodec *jpegCodec = avcodec_find_encoder(AV_CODEC_ID_MJPEG);
    if (!jpegCodec) return -1;

    AVCodecContext *jpegCtx = avcodec_alloc_context3(jpegCodec);
    jpegCtx->pix_fmt = AV_PIX_FMT_YUVJ420P;  // 推荐
    jpegCtx->height = frame->height;
    jpegCtx->width = frame->width;
    jpegCtx->time_base = {1, 25};

    if (avcodec_open2(jpegCtx, jpegCodec, nullptr) < 0) {
        avcodec_free_context(&jpegCtx);
        return -1;
    }

    // 2. 编码为JPEG
    AVPacket pkt = {};
    // av_init_packet(&pkt);
    pkt.data = nullptr;
    pkt.size = 0;
    int ret = avcodec_send_frame(jpegCtx, frame);
    if (ret >= 0)
        ret = avcodec_receive_packet(jpegCtx, &pkt);

    int result = -1;
    if (ret == 0 && pkt.size > 0) {
        // 3. 用QImage解析JPEG数据
        QByteArray jpegData((const char*)pkt.data, pkt.size);
        QImage img = QImage::fromData(jpegData, "JPG");
        if (!img.isNull()) {
            // 4. 放入剪贴板
            QGuiApplication::clipboard()->setImage(img);
            result = 0;
        }
        av_packet_unref(&pkt);
    }

    avcodec_free_context(&jpegCtx);
    return result;
}

void VideoRenderWidget::initTextures() {
    glGenTextures(1, &textureY);
    glBindTexture(GL_TEXTURE_2D, textureY);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glGenTextures(1, &textureU);
    glBindTexture(GL_TEXTURE_2D, textureU);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glGenTextures(1, &textureV);
    glBindTexture(GL_TEXTURE_2D, textureV);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
}

void VideoRenderWidget::initShader() {
    const char* vsrc =
        "attribute vec2 vertexIn;\n"
        "attribute vec2 textureIn;\n"
        "varying vec2 textureOut;\n"
        "void main() {\n"
        "    gl_Position = vec4(vertexIn, 0.0, 1.0);\n"
        "    textureOut = textureIn;\n"
        "}\n";

    const char* fsrc =
        "varying vec2 textureOut;\n"
        "uniform sampler2D texY;\n"
        "uniform sampler2D texU;\n"
        "uniform sampler2D texV;\n"
        "void main() {\n"
        "    float y = texture2D(texY, textureOut).r;\n"
        "    float u = texture2D(texU, textureOut).r - 0.5;\n"
        "    float v = texture2D(texV, textureOut).r - 0.5;\n"
        "    float r = y + 1.402 * v;\n"
        "    float g = y - 0.344 * u - 0.714 * v;\n"
        "    float b = y + 1.772 * u;\n"
        "    gl_FragColor = vec4(r, g, b, 1.0);\n"
        "}\n";

    program.addShaderFromSourceCode(QOpenGLShader::Vertex, vsrc);
    program.addShaderFromSourceCode(QOpenGLShader::Fragment, fsrc);
    program.link();
}
