//
// Created by Admin on 2025/7/5.
//

#include "ProgressSlider.h"
#include <algorithm>

ProgressSlider::ProgressSlider(Qt::Orientation orientation, QWidget *parent)
    : QSlider(orientation, parent)
{
    setMouseTracking(true);
    setCursor(Qt::PointingHandCursor);
}

ProgressSlider::ProgressSlider(QWidget *parent)
    : ProgressSlider(Qt::Horizontal, parent)
{}

void ProgressSlider::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    int sliderHeight = height();
    int barHeight = 8;
    int dotRadius = std::min(9, sliderHeight / 2 - 2);
    int margin = dotRadius + 2;
    int barY = sliderHeight / 2 - barHeight / 2;
    int w = width() - 2 * margin;

    // 进度百分比
    double percent = (maximum() > minimum()) ? double(value() - minimum()) / (maximum() - minimum()) : 0.0;
    int playedW = int(w * percent);

    // 颜色
    QColor playedColor = hovered ? QColor(255, 255, 255, 120) : QColor(255, 255, 255, 80);    // 已播放，更加透明
    QColor bgColor     = hovered ? QColor(180, 180, 180, 60)  : QColor(120, 120, 120, 40);    // 未播放，偏暗且更透明
    QColor dotColor    = hovered ? QColor(240, 240, 240, 255) : QColor(200, 200, 200, 255);   // 圆点，柔和不刺眼

    // 未播放
    QRect bgRect(margin, barY, w, barHeight);
    p.setPen(Qt::NoPen);
    p.setBrush(bgColor);
    p.drawRoundedRect(bgRect, 4, 4);

    // 已播放
    QRect playedRect(margin, barY, playedW, barHeight);
    p.setBrush(playedColor);
    p.drawRoundedRect(playedRect, 4, 4);

    // 拖动圆点
    int dotX = std::max(margin / 2 + playedW, margin);
    p.setBrush(dotColor);
    p.drawEllipse(QPoint(dotX, sliderHeight / 2), dotRadius, dotRadius);

    if (hovered) {
        p.setBrush(QColor(255,255,255,90));
        p.drawEllipse(QPoint(dotX, sliderHeight / 2), dotRadius + 1, dotRadius + 1);
    }

    // 悬浮竖线（跟随鼠标，条形区域高，宽度2）
    if (hovered && hoverPos >= margin && hoverPos <= width() - margin) {
        int lineWidth = 2;
        int y1 = barY + 1;
        int y2 = barY + barHeight - 1;
        QPen pen(QColor(255, 255, 255, 220), lineWidth);
        p.setPen(pen);
        p.drawLine(hoverPos, y1, hoverPos, y2);
    }
}

void ProgressSlider::enterEvent(QEnterEvent *event) {
    hovered = true;
    update();

    QPointF posF = event->position(); // Qt6+ 推荐
    int x = int(posF.x());

    double percent = std::clamp(double(x) / width(), 0.0, 1.0);
    int sliderValue = minimum() + percent * (maximum() - minimum());

    emit requestPreview(x, 1, sliderValue);

    QSlider::enterEvent(event);
}

void ProgressSlider::leaveEvent(QEvent *event) {
    hovered = false;
    hoverPos = -1;
    update();
    emit requestPreview(-1, -1, -1);
    emit hidePreview();
    QSlider::leaveEvent(event);
}

void ProgressSlider::mouseMoveEvent(QMouseEvent *event) {
    hoverPos = event->pos().x();
    hovered = true;
    update();

    // 只要鼠标在slider范围内移动就一直发信号
    int x = event->pos().x();

    // 注意：这里最好用你的进度条“有效区域”来算百分比
    int sliderWidth = width();
    double percent = std::clamp(double(x) / sliderWidth, 0.0, 1.0);
    int sliderValue = minimum() + percent * (maximum() - minimum());

    emit requestPreview(x, 0, sliderValue);

    QSlider::mouseMoveEvent(event);
}

void ProgressSlider::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        // 用官方推荐方式实现点击跳转
        QStyleOptionSlider opt;
        this->initStyleOption(&opt);

        int sliderMin = style()->subControlRect(QStyle::CC_Slider, &opt, QStyle::SC_SliderGroove, this).left();
        int sliderMax = style()->subControlRect(QStyle::CC_Slider, &opt, QStyle::SC_SliderGroove, this).right();
        int sliderLength = style()->pixelMetric(QStyle::PM_SliderLength, &opt, this);

        int clickPos = event->pos().x();
        int minPos = sliderMin;
        int maxPos = sliderMax - sliderLength + 1;
        int pos = std::clamp(clickPos, minPos, maxPos);

        int newValue = QStyle::sliderValueFromPosition(
            minimum(), maximum(),
            pos - minPos,
            maxPos - minPos,
            opt.upsideDown
        );
        setValue(newValue);
        emit sliderMoved(newValue);
        emit valueChanged(newValue);
        update();
    }
    QSlider::mousePressEvent(event);
}
