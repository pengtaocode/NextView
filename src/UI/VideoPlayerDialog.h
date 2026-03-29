#pragma once

#include <QDialog>
#include <QMediaPlayer>
#include <QVideoWidget>
#include <QAudioOutput>
#include <QPushButton>
#include <QSlider>
#include <QLabel>
#include <QTimer>
#include <QWidget>
#include "MediaFile.h"

/**
 * @brief 内置视频播放器对话框
 * 最大化显示，黑色背景，底部浮层控制栏
 * 支持播放/暂停、进度跳转、全屏、音量调节
 */
class VideoPlayerDialog : public QDialog {
    Q_OBJECT

public:
    explicit VideoPlayerDialog(QWidget* parent = nullptr);
    ~VideoPlayerDialog();

    /**
     * @brief 打开并播放视频
     */
    void open(const MediaFile& file);

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void closeEvent(QCloseEvent* event) override;

private slots:
    /**
     * @brief 切换播放/暂停状态
     */
    void togglePlayPause();

    /**
     * @brief 播放状态变化时更新按钮显示
     */
    void onPlaybackStateChanged(QMediaPlayer::PlaybackState state);

    /**
     * @brief 媒体时长获取后更新进度条范围
     */
    void onDurationChanged(qint64 duration);

    /**
     * @brief 播放位置变化时更新进度条
     */
    void onPositionChanged(qint64 position);

    /**
     * @brief 拖动进度条跳转
     */
    void seekTo(int position);

    /**
     * @brief 切换全屏状态
     */
    void toggleFullscreen();

    /**
     * @brief 控制栏自动隐藏计时器触发
     */
    void onControlBarTimer();

    /**
     * @brief 播放结束处理
     */
    void onMediaStatusChanged(QMediaPlayer::MediaStatus status);

private:
    /**
     * @brief 创建底部控制栏
     */
    QWidget* createControlBar();

    /**
     * @brief 更新控制栏位置
     */
    void updateControlBarGeometry();

    /**
     * @brief 格式化时间为 mm:ss 字符串
     */
    static QString formatTime(qint64 ms);

    /**
     * @brief 显示/隐藏控制栏
     */
    void showControls();
    void hideControls();

    QMediaPlayer* m_player;         // 媒体播放器
    QVideoWidget* m_videoWidget;    // 视频显示控件
    QAudioOutput* m_audioOutput;    // 音频输出

    // 控制栏控件
    QWidget* m_controlBar;          // 控制栏容器
    QPushButton* m_playPauseBtn;    // 播放/暂停按钮
    QSlider* m_progressSlider;      // 进度条
    QLabel* m_timeLabel;            // 当前时间/总时长标签
    QPushButton* m_fullscreenBtn;   // 全屏按钮
    QPushButton* m_volumeBtn;       // 音量按钮
    QSlider* m_volumeSlider;        // 音量滑块

    QTimer* m_controlBarTimer;      // 控制栏自动隐藏计时器
    bool m_isFullscreen;            // 是否全屏
    bool m_userSeeking;             // 用户是否正在拖动进度条
    bool m_controlsVisible;         // 控制栏是否可见

    static constexpr int ControlBarHeight = 72;  // 控制栏高度
};
