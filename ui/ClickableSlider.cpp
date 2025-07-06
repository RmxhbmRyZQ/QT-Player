//
// Created by Admin on 2025/7/3.
//

#include "ClickableSlider.h"
#include <QtMath>

void ClickableSlider::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        setSliderValueFromEvent(event);
        event->accept();
    }
    QSlider::mousePressEvent(event);
}

void ClickableSlider::mouseMoveEvent(QMouseEvent *event)
{
    if (event->buttons() & Qt::LeftButton) {
        setSliderValueFromEvent(event);
        event->accept();
    }
    QSlider::mouseMoveEvent(event);
}

void ClickableSlider::setSliderValueFromEvent(QMouseEvent *event)
{
    int min = this->minimum();
    int max = this->maximum();
    int newValue;

    if (orientation() == Qt::Horizontal) {
        double ratio = double(event->pos().x()) / width();
        ratio = qBound(0.0, ratio, 1.0);
        newValue = min + (max - min) * ratio;
    } else {
        double ratio = 1.0 - double(event->pos().y()) / height();
        ratio = qBound(0.0, ratio, 1.0);
        newValue = min + (max - min) * ratio;
    }
    setValue(newValue);
}
