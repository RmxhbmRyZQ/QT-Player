//
// Created by Admin on 2025/7/5.
//

#pragma once
#include <QSlider>
#include <QPainter>
#include <QMouseEvent>
#include <QStyleOptionSlider>
#include <QEnterEvent>

class ProgressSlider : public QSlider {
    Q_OBJECT
public:
    explicit ProgressSlider(Qt::Orientation orientation, QWidget *parent = nullptr);
    explicit ProgressSlider(QWidget *parent = nullptr);

signals:
    // 鼠标悬浮在进度条上时发出，参数分别为 鼠标x像素、状态、slider value
    void requestPreview(int mouseX, int status, int value);
    void hidePreview(); // 鼠标离开时隐藏

protected:
    void paintEvent(QPaintEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;

private:
    bool hovered = false;
    int hoverPos = -1; // 鼠标在进度条上的x坐标
};




