//
// Created by Admin on 2025/7/5.
//

#include "ThumbnailPreview.h"

#include <QGuiApplication>
#include <QPainter>
#include <qscreen.h>
#include <QStyleOption>
#include <ui_PlayerWindow.h>

static QString timeToString(double progress) {
    int totalSeconds = progress;
    int hours = totalSeconds / 3600;
    int minutes = (totalSeconds % 3600) / 60;
    int seconds = totalSeconds % 60;

    // 格式化为 00:00:00 字符串
    QString timeStr;
    if (hours > 0)
        timeStr = QString("%1:%2:%3")
                    .arg(hours, 2, 10, QChar('0'))
                    .arg(minutes, 2, 10, QChar('0'))
                    .arg(seconds, 2, 10, QChar('0'));
    else
        timeStr = QString("%1:%2")
                    .arg(minutes, 2, 10, QChar('0'))
                    .arg(seconds, 2, 10, QChar('0'));
    return timeStr;
}

ThumbnailPreview::ThumbnailPreview(QWidget* parent)
    : QWidget(parent)
{
    setWindowFlags(Qt::ToolTip | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground, true);

    renderWidget = new VideoRenderWidget(this);
    renderWidget ->setFixedSize(120, 68); // 缩略图尺寸

    timeLabel = new QLabel(this);
    timeLabel->setAlignment(Qt::AlignCenter);
    timeLabel->setStyleSheet("color: white; background: rgba(0,0,0,180); border-radius: 5px; padding:2px;");

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setSpacing(2);
    layout->setContentsMargins(3,3,3,3);
    layout->addWidget(renderWidget, 0, Qt::AlignCenter);
    layout->addWidget(timeLabel, 0, Qt::AlignCenter);
    setLayout(layout);

    setFixedSize(126, 92); // 总尺寸略大于图片
}

void ThumbnailPreview::setFrame(const AVFrame* frame) {
    if (renderWidget && frame) {
        // 复制一份，防止外部 frame 释放（如果有线程安全需求可以再处理）
        AVFrame* frameCopy = av_frame_alloc();
        av_frame_ref(frameCopy, frame); // 深拷贝指针及像素
        renderWidget->setFrame(frameCopy); // VideoRenderWidget 内部管理释放
        renderWidget->update(); // 强制重绘
    }
}

void ThumbnailPreview::setTimeText(const QString& text) {
    timeLabel->setText(text);
}

void ThumbnailPreview::setProgressSlider(ProgressSlider* progress) {
    this->progress = progress;
}

void ThumbnailPreview::updatePreviewSize(int mainWinW, int mainWinH) {
    // 你可以根据主窗口宽高，设置自己的尺寸
    int thumbW = mainWinW / 10;
    int thumbH = mainWinH / 10;
    renderWidget->setFixedSize(thumbW, thumbH);
    setFixedSize(thumbW + 6, thumbH + 24); // 适当加padding/label高度
}

void ThumbnailPreview::onThumbnailReady(const AVFrame* frame, int mouseX, int status, int64_t value) {
    if (status == 1) {
        focus = true;
    }

    if (status == -1) {
        focus = false;
        if (isVisible()) hide();
        return;
    }

    if (!focus) return;

    setFrame(frame);
    QString timeText = timeToString(value / 1000); // 你需要实现
    setTimeText(timeText);

    // 计算弹窗显示位置
    QPoint globalPos = progress->mapToGlobal(QPoint(mouseX, 0));
    int popupW = width();
    int popupH = height();
    int x = globalPos.x() - popupW / 2;
    int y = progress->mapToGlobal(QPoint(0, 0)).y() - popupH - 10; // 显示在进度条上方

    // 获取当前屏幕的可用区域
    QScreen* screen = nullptr;
    if (QWidget* w = this->window()) {
        if (QWindow* win = w->windowHandle())
            screen = win->screen();
    }
    if (!screen) screen = QGuiApplication::primaryScreen();
    QRect deskRect = screen->availableGeometry();

    // 保证缩略图弹窗不超出屏幕边界
    x = std::max(deskRect.left(), std::min(x, deskRect.right() - popupW));
    if (y < deskRect.top()) y = deskRect.top() + 10; // 避免太顶

    move(x, y);
    show();
    raise();  // 放到最上面，确保不会被遮蔽
}

void ThumbnailPreview::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setBrush(QColor(20,20,20,210));
    p.setPen(Qt::NoPen);
    p.drawRoundedRect(rect(), 10, 10);
}

