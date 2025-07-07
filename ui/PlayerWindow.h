#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QFileDialog>

#include "Player.h"
#include "ThumbnailPreview.h"
#include "ThumbnailWorker.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class PlayerWindow;
}
QT_END_NAMESPACE

class PlayerWindow : public QMainWindow
{
    Q_OBJECT

private slots:
    void on_btnForward_clicked();
    void on_btnPlay_clicked();
    void on_btnPause_clicked();
    void on_btnStop_clicked();
    void on_btnScreenshot_clicked();
    void on_btnFullscreen_clicked();
    void on_btnOpen_clicked();
    void on_btnRewind_clicked();
    void on_btnVolumeIcon_clicked();
    void renderFrame();
    void volumeSliderChanged(int value);
    void speedChanged(int ids);
    void progressInitHandler(double progress);
    void progressChangedHandler(double progress);
    void progressSliderHandler();
    void progressSliderPressedHandler();
    void rightKeyLongPress();
    void increaseVolume();
    void decreaseVolume();
    void spacePressedHandler();
    void onRequestPreview(int mouseX, int status, int64_t value);

public:
    PlayerWindow(QWidget *parent = nullptr);
    ~PlayerWindow();

private:
    void connectSignals();
    void setStyle();

protected:
    // void keyPressEvent(QKeyEvent *event) override;
    // void keyReleaseEvent(QKeyEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *event) override;
    void resizeEvent(QResizeEvent* event) override;

signals:
    void requestPreview(int mouseX, int status, int64_t value);

private:
    Ui::PlayerWindow *ui = nullptr;
    Player *player = nullptr;
    ThumbnailPreview *thumbnailPreview = nullptr;
    bool firstFrame = false;
    bool userPressed = false;
    bool rightKeyPressed = false;
    bool isSpeedup = false;
    QTimer *speedupTimer = nullptr;
    int speed_idx = DEFAULT_SPEED_IDX;
    QThread *thumbnailPreviewThread = nullptr;
    ThumbnailWorker *thumbnailWorker = nullptr;
    qint64 ms = 0;
};
#endif // MAINWINDOW_H
