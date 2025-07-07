//
// Created by Admin on 2025/7/1.
//

#pragma once

#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QClipboard>
#include <QGuiApplication>
#include <QImage>
#include <QByteArray>
#include <QBuffer>

extern "C" {
#include <libavutil/frame.h>
#include <libavcodec/avcodec.h>
}

class VideoRenderWidget : public QOpenGLWidget, protected QOpenGLFunctions {
    Q_OBJECT
public:
    explicit VideoRenderWidget(QWidget* parent = nullptr);
    ~VideoRenderWidget();

    void setFrame(AVFrame *frame);
    int copyFrameToClipboard();

protected:
    void initializeGL() override;
    void paintGL() override;
    void resizeGL(int w, int h) override;
    void mousePressEvent(QMouseEvent* event) override;

signals:
    void clicked();

private:
    void initShader();
    void initTextures();

    QOpenGLShaderProgram program;
    GLuint textureY = 0, textureU = 0, textureV = 0;
    int videoW = 0, videoH = 0;
    uint8_t *yPlane = nullptr, *uPlane = nullptr, *vPlane = nullptr;
    AVFrame *frame = nullptr;

    std::mutex mtx;
};

