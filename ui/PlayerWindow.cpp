#include "PlayerWindow.h"
#include "VideoRenderWidget.h"
#include "ui_PlayerWindow.h"

static QString getVolumeEmoji(int volume) {
    if (volume == 0)
        return "🔇";      // 静音
    else if (volume <= 30)
        return "🔈";      // 小音量
    else if (volume <= 70)
        return "🔉";      // 中等音量
    else
        return "🔊";      // 大音量
}

static QString timeToString(double progress) {
    int totalSeconds = progress;  // 假如 progress 为 int 秒数
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

PlayerWindow::PlayerWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::PlayerWindow)
{
    ui->setupUi(this);

    ui->comboSpeed->setCurrentIndex(DEFAULT_SPEED_IDX);
    ui->sliderProgress->setMaximum(1000);

    thumbnailPreview = new ThumbnailPreview(this);
    thumbnailPreview->setProgressSlider(ui->sliderProgress);
    thumbnailPreviewThread = new QThread();
    thumbnailWorker = new ThumbnailWorker();
    thumbnailWorker->moveToThread(thumbnailPreviewThread);
    thumbnailPreviewThread->start();

    player = new Player();
    player->setVideoRenderWidgets(ui->videoWidget);

    speedupTimer = new QTimer(this);
    speedupTimer->setSingleShot(true);
    qApp->installEventFilter(this);

    connectSignals();

    setWindowTitle("播放器");
    this->setWindowIcon(QIcon(":icon.svg"));
}

PlayerWindow::~PlayerWindow()
{
    delete player;
    speedupTimer->stop();
    delete speedupTimer;
    thumbnailPreviewThread->quit();
    thumbnailPreviewThread->wait();
    delete thumbnailPreviewThread;
    delete thumbnailWorker;
    delete thumbnailPreview;
    delete ui;
}

void PlayerWindow::connectSignals() {
    connect(player, &Player::onRefreshVideo, this, &PlayerWindow::renderFrame);
    connect(player, &Player::onProgressInit, this, &PlayerWindow::progressInitHandler);
    connect(player, &Player::onProgressChange, this, &PlayerWindow::progressChangedHandler);
    connect(ui->sliderVolume, &QSlider::valueChanged, this, &PlayerWindow::volumeSliderChanged);
    connect(ui->comboSpeed, &QComboBox::currentIndexChanged, this, &PlayerWindow::speedChanged);
    connect(ui->sliderProgress, &QSlider::sliderReleased, this, &PlayerWindow::progressSliderHandler);
    connect(ui->sliderProgress, &QSlider::sliderPressed, this, &PlayerWindow::progressSliderPressedHandler);
    connect(speedupTimer, &QTimer::timeout, this, &PlayerWindow::rightKeyLongPress);

    connect(ui->sliderProgress, &ProgressSlider::requestPreview, this, &PlayerWindow::onRequestPreview);
    connect(this, &PlayerWindow::requestPreview, thumbnailWorker, &ThumbnailWorker::onRequestPreview);
    connect(ui->sliderProgress, &ProgressSlider::hidePreview, thumbnailPreview, &PlayerWindow::hide);
    connect(thumbnailWorker, &ThumbnailWorker::thumbnailReady, thumbnailPreview, &ThumbnailPreview::onThumbnailReady);

    connect(ui->videoWidget, &VideoRenderWidget::clicked, this, &PlayerWindow::spacePressedHandler);

    connect(thumbnailPreviewThread, &QThread::finished, thumbnailPreviewThread, &QObject::deleteLater);
}

bool PlayerWindow::eventFilter(QObject *obj, QEvent *event) {
    if (event->type() == QEvent::KeyPress) {
        const auto *keyEvent = dynamic_cast<QKeyEvent*>(event);
        switch (keyEvent->key()) {
            case Qt::Key_Left:
                on_btnRewind_clicked();
                return true;
            case Qt::Key_Right:
                if (!rightKeyPressed) {
                    rightKeyPressed = true;
                    isSpeedup = false;
                    speedupTimer->start(500); // 500ms内不跳转，等待长按判定
                }
                return true;
            case Qt::Key_Up:
                increaseVolume();
                return true;
            case Qt::Key_Down:
                decreaseVolume();
                return true;
            case Qt::Key_Space:
                spacePressedHandler();
                return true;
            case Qt::Key_Escape:
                if (this->isFullScreen()) {
                    on_btnFullscreen_clicked();
                }
                return true;
            default:
                break;
        }
    } else if (event->type() == QEvent::KeyRelease) {
        const auto *keyEvent = dynamic_cast<QKeyEvent*>(event);

        switch (keyEvent->key()) {
            case Qt::Key_Right:
                if (keyEvent->isAutoRepeat())
                    return true;
                speedupTimer->stop();
                if (rightKeyPressed && !isSpeedup) {
                    // 短按，正常seek
                    on_btnForward_clicked();
                }
                if (isSpeedup) {
                    player->setSpeed(speed_idx); // 恢复正常速度
                    ui->comboSpeed->setCurrentIndex(speed_idx);
                    isSpeedup = false;
                }
                rightKeyPressed = false;
                return true;
            default:
                break;
        }
    } else if (event->type() == QEvent::MouseMove) {
        QWidget *w = QApplication::widgetAt(QCursor::pos());
        if (w != ui->sliderProgress && thumbnailPreview->isVisible()) {
            // 鼠标离开进度条，但 leaveEvent 没发出，这里兜底关闭缩放图
            thumbnailPreview->hide();
        }
    }
    return QMainWindow::eventFilter(obj, event);
}

void PlayerWindow::resizeEvent(QResizeEvent* event) {
    QMainWindow::resizeEvent(event);
    // 通知缩略图弹窗调整大小
    if (thumbnailPreview) {
        thumbnailPreview->updatePreviewSize(width(), height());
    }
}

void PlayerWindow::on_btnForward_clicked() {
    player->forward();
    ui->statusbar->showMessage("向前跳转...", STATUS_BAR_TIME);
}

void PlayerWindow::on_btnPlay_clicked() {
    if (!player->isPlaying())
        player->pause(false);
    ui->statusbar->showMessage(player->isPlaying() ? "播放..." : "暂停...", STATUS_BAR_TIME);
}

void PlayerWindow::on_btnPause_clicked() {
    if (player->isPlaying())
        player->pause(true);
    ui->statusbar->showMessage(player->isPlaying() ? "播放..." : "暂停...", STATUS_BAR_TIME);
}

void PlayerWindow::on_btnStop_clicked() {
    player->stop();
    ui->videoWidget->setFrame(nullptr);
    ui->videoWidget->update();
    ui->sliderProgress->setValue(0);
    ui->labelCurrentTime->setText("00:00");
    ui->labelTotalTime->setText("00:00");
    ui->statusbar->showMessage("停止播放...", STATUS_BAR_TIME);
}

void PlayerWindow::on_btnScreenshot_clicked() {
    if (ui->videoWidget->copyFrameToClipboard() < 0) {
        ui->statusbar->showMessage("截图失败...", STATUS_BAR_TIME);
    } else {
        ui->statusbar->showMessage("截图成功...", STATUS_BAR_TIME);
    }
}

void PlayerWindow::on_btnFullscreen_clicked() {
    if (this->isFullScreen()) {
        this->showNormal();
        ui->statusbar->showMessage("正常播放...", STATUS_BAR_TIME);
    } else {
        this->showFullScreen();
        ui->statusbar->showMessage("全屏播放...", STATUS_BAR_TIME);
    }
}

void PlayerWindow::on_btnOpen_clicked() {
    QString file = QFileDialog::getOpenFileName(this, "选择文件", QDir::homePath(), "视频文件 (*.mp4 *.avi *.flv);;所有文件 (*.*)");
    if (!file.isNull()) {
        ui->sliderProgress->setValue(0);
        player->reset();
        thumbnailWorker->reset();
        ui->videoWidget->setFrame(nullptr);
        if (player->setFile(file.toUtf8().data()) < 0 || player->start() < 0) {
            ui->statusbar->showMessage("播放失败...", STATUS_BAR_TIME);
        } else {
            if (player->haveVideo()) {
                thumbnailWorker->init(file.toUtf8().data());
            }
            ui->statusbar->showMessage("播放成功...", STATUS_BAR_TIME);
        }
    }
}

void PlayerWindow::on_btnRewind_clicked() {
    player->backward();
    ui->statusbar->showMessage("向后跳转...", STATUS_BAR_TIME);
}

void PlayerWindow::on_btnVolumeIcon_clicked() {

    player->setMute(!player->getMute());
    if (player->getMute()) {
        ui->statusbar->showMessage("静音播放...", STATUS_BAR_TIME);
        ui->btnVolumeIcon->setText(getVolumeEmoji(0));
    } else {
        ui->statusbar->showMessage("取消静音...", STATUS_BAR_TIME);
        ui->btnVolumeIcon->setText(getVolumeEmoji(ui->sliderVolume->value()));
    }
}

void PlayerWindow::renderFrame() {
    VideoRenderWidget *renderWidget = ui->videoWidget;
    renderWidget->update();
}

void PlayerWindow::volumeSliderChanged(int value) {
    player->setVolume(value);
    ui->labelVolumeValue->setText(QString::number(value));
    ui->btnVolumeIcon->setText(getVolumeEmoji(ui->sliderVolume->value()));
}

void PlayerWindow::speedChanged(int ids) {
    if (rightKeyPressed && ids == THREE_SPEED_INDEX) return;
    player->setSpeed(ids);
    speed_idx = ids;
    ui->statusbar->showMessage("播放速度改变...", STATUS_BAR_TIME);
}

void PlayerWindow::progressInitHandler(double progress) {
    ui->labelTotalTime->setText(timeToString(progress));
    ui->sliderProgress->setMaximum(progress * 1000);
}

void PlayerWindow::progressChangedHandler(double progress) {
    // 在用户跳转后，防止进度条由于信号延迟的问题，跳转回去
    if (userPressed || QDateTime::currentMSecsSinceEpoch() - ms <= 200) {
        return;
    }

    ui->labelCurrentTime->setText(timeToString(progress));
    ui->sliderProgress->setValue(progress * 1000);
}

void PlayerWindow::progressSliderHandler() {
    ms = QDateTime::currentMSecsSinceEpoch();
    userPressed = false;
    player->seek(ui->sliderProgress->value());
    ui->statusbar->showMessage("跳转成功...", STATUS_BAR_TIME);
}

void PlayerWindow::progressSliderPressedHandler() {
    userPressed = true;
}

void PlayerWindow::rightKeyLongPress() {
    isSpeedup = true;
    speed_idx = ui->comboSpeed->currentIndex();
    ui->comboSpeed->setCurrentIndex(THREE_SPEED_INDEX);
    player->setSpeed(THREE_SPEED_INDEX);
    ui->statusbar->showMessage("3 倍速播放...", STATUS_BAR_TIME);
}

void PlayerWindow::increaseVolume() {
    int volume = std::min(ui->sliderVolume->value() + 10, 100);
    ui->sliderVolume->setValue(volume);
    ui->statusbar->showMessage("音量加 10...", STATUS_BAR_TIME);
}

void PlayerWindow::decreaseVolume() {
    int volume = std::max(ui->sliderVolume->value() - 10, 0);
    ui->sliderVolume->setValue(volume);
    ui->statusbar->showMessage("音量减 10...", STATUS_BAR_TIME);
}

void PlayerWindow::spacePressedHandler() {
    if (player->isPlaying()) {
        on_btnPause_clicked();
    } else {
        on_btnPlay_clicked();
    }
}

void PlayerWindow::onRequestPreview(int mouseX, int status, int64_t value) {
    if (!thumbnailWorker->isRunning())
        emit requestPreview(mouseX, status, value);
}
