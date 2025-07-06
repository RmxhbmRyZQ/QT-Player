//
// Created by Admin on 2025/7/5.
//

#pragma once
#include <QWidget>
#include <QLabel>
#include <QVBoxLayout>
#include <QPixmap>
#include <QWindow>

#include "ProgressSlider.h"
#include "VideoRenderWidget.h"

extern "C" {
#include <libavutil/frame.h>
}

class ThumbnailPreview : public QWidget {
    Q_OBJECT
public:
    explicit ThumbnailPreview(QWidget* parent = nullptr);

    void setFrame(const AVFrame* frame);
    void setTimeText(const QString& text);
    void setProgressSlider(ProgressSlider *progress);
    void updatePreviewSize(int mainWinW, int mainWinH);

public slots:
    void onThumbnailReady(const AVFrame *frame, int mouseX, int status, int64_t value);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    VideoRenderWidget* renderWidget = nullptr;
    QLabel* timeLabel = nullptr;
    ProgressSlider* progress = nullptr;
    std::atomic<bool> focus = false;
};

