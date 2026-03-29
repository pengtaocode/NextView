#include "VideoPlayerDialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QCloseEvent>
#include <QFont>
#include <QDebug>

VideoPlayerDialog::VideoPlayerDialog(QWidget* parent)
    : QDialog(parent, Qt::Window | Qt::FramelessWindowHint)
    , m_isFullscreen(false)
    , m_userSeeking(false)
    , m_controlsVisible(true)
{
    setStyleSheet("QDialog { background: black; }");
    setAttribute(Qt::WA_DeleteOnClose, false);
    setMouseTracking(true);

    // 媒体播放器
    m_player = new QMediaPlayer(this);
    m_audioOutput = new QAudioOutput(this);
    m_audioOutput->setVolume(1.0f); // 默认音量100%
    m_player->setAudioOutput(m_audioOutput);

    // 视频显示控件（全屏）
    m_videoWidget = new QVideoWidget(this);
    m_videoWidget->setStyleSheet("background: black;");
    m_player->setVideoOutput(m_videoWidget);

    // 创建控制栏
    m_controlBar = createControlBar();

    // 连接播放器信号
    connect(m_player, &QMediaPlayer::playbackStateChanged,
            this, &VideoPlayerDialog::onPlaybackStateChanged);
    connect(m_player, &QMediaPlayer::durationChanged,
            this, &VideoPlayerDialog::onDurationChanged);
    connect(m_player, &QMediaPlayer::positionChanged,
            this, &VideoPlayerDialog::onPositionChanged);
    connect(m_player, &QMediaPlayer::mediaStatusChanged,
            this, &VideoPlayerDialog::onMediaStatusChanged);

    // 控制栏自动隐藏计时器（3秒后隐藏）
    m_controlBarTimer = new QTimer(this);
    m_controlBarTimer->setSingleShot(true);
    m_controlBarTimer->setInterval(3000);
    connect(m_controlBarTimer, &QTimer::timeout, this, &VideoPlayerDialog::onControlBarTimer);
}

VideoPlayerDialog::~VideoPlayerDialog() {
    m_player->stop();
}

QWidget* VideoPlayerDialog::createControlBar() {
    QWidget* bar = new QWidget(this);
    bar->setStyleSheet(
        "QWidget { background: rgba(0,0,0,0.75); }"
        "QPushButton { background: transparent; border: none; color: white; "
        "font-size: 18px; font-weight: bold; padding: 6px; border-radius: 4px; }"
        "QPushButton:hover { background: rgba(255,255,255,0.15); }"
        "QSlider::groove:horizontal { height: 4px; background: rgba(255,255,255,0.3); "
        "border-radius: 2px; }"
        "QSlider::handle:horizontal { background: white; width: 14px; height: 14px; "
        "border-radius: 7px; margin: -5px 0; }"
        "QSlider::sub-page:horizontal { background: #007AFF; border-radius: 2px; }"
        "QLabel { color: rgba(255,255,255,0.85); font-size: 12px; }"
    );

    QVBoxLayout* barLayout = new QVBoxLayout(bar);
    barLayout->setContentsMargins(16, 8, 16, 12);
    barLayout->setSpacing(6);

    // 进度条行
    m_progressSlider = new QSlider(Qt::Horizontal, bar);
    m_progressSlider->setRange(0, 1000);
    m_progressSlider->setValue(0);
    m_progressSlider->setCursor(Qt::PointingHandCursor);

    connect(m_progressSlider, &QSlider::sliderPressed, this, [this]() {
        m_userSeeking = true;
    });
    connect(m_progressSlider, &QSlider::sliderReleased, this, [this]() {
        seekTo(m_progressSlider->value());
        m_userSeeking = false;
    });

    barLayout->addWidget(m_progressSlider);

    // 按钮行
    QHBoxLayout* btnRow = new QHBoxLayout();
    btnRow->setSpacing(4);

    m_playPauseBtn = new QPushButton("⏸", bar);
    m_playPauseBtn->setFixedSize(40, 36);
    connect(m_playPauseBtn, &QPushButton::clicked, this, &VideoPlayerDialog::togglePlayPause);

    m_timeLabel = new QLabel("0:00 / 0:00", bar);
    m_timeLabel->setMinimumWidth(100);

    // 音量控件
    m_volumeBtn = new QPushButton("🔊", bar);
    m_volumeBtn->setFixedSize(36, 36);
    connect(m_volumeBtn, &QPushButton::clicked, this, [this]() {
        bool muted = m_audioOutput->volume() > 0;
        m_audioOutput->setVolume(muted ? 0.0f : 1.0f);
        m_volumeBtn->setText(muted ? "🔇" : "🔊");
        m_volumeSlider->setValue(muted ? 0 : 100);
    });

    m_volumeSlider = new QSlider(Qt::Horizontal, bar);
    m_volumeSlider->setRange(0, 100);
    m_volumeSlider->setValue(100);
    m_volumeSlider->setFixedWidth(80);
    m_volumeSlider->setCursor(Qt::PointingHandCursor);
    connect(m_volumeSlider, &QSlider::valueChanged, this, [this](int value) {
        m_audioOutput->setVolume(value / 100.0f);
        m_volumeBtn->setText(value == 0 ? "🔇" : "🔊");
    });

    m_fullscreenBtn = new QPushButton("⛶", bar);
    m_fullscreenBtn->setFixedSize(36, 36);
    m_fullscreenBtn->setToolTip("全屏");
    connect(m_fullscreenBtn, &QPushButton::clicked, this, &VideoPlayerDialog::toggleFullscreen);

    btnRow->addWidget(m_playPauseBtn);
    btnRow->addWidget(m_timeLabel);
    btnRow->addStretch();
    btnRow->addWidget(m_volumeBtn);
    btnRow->addWidget(m_volumeSlider);
    btnRow->addSpacing(8);
    btnRow->addWidget(m_fullscreenBtn);

    barLayout->addLayout(btnRow);

    return bar;
}

void VideoPlayerDialog::open(const MediaFile& file) {
    showMaximized();

    m_player->setSource(QUrl::fromLocalFile(file.path));
    m_player->play();

    // 重置进度条
    m_progressSlider->setValue(0);
    m_timeLabel->setText("0:00 / 0:00");

    // 启动控制栏自动隐藏
    m_controlBarTimer->start();

    qDebug() << "视频播放器已打开：" << file.path;
}

void VideoPlayerDialog::togglePlayPause() {
    if (m_player->playbackState() == QMediaPlayer::PlayingState) {
        m_player->pause();
    } else {
        m_player->play();
    }
}

void VideoPlayerDialog::onPlaybackStateChanged(QMediaPlayer::PlaybackState state) {
    if (state == QMediaPlayer::PlayingState) {
        m_playPauseBtn->setText("⏸");
    } else {
        m_playPauseBtn->setText("▶");
    }
}

void VideoPlayerDialog::onDurationChanged(qint64 duration) {
    Q_UNUSED(duration)
    // 时长改变后更新时间标签
    qint64 pos = m_player->position();
    qint64 dur = m_player->duration();
    m_timeLabel->setText(formatTime(pos) + " / " + formatTime(dur));
}

void VideoPlayerDialog::onPositionChanged(qint64 position) {
    if (!m_userSeeking) {
        qint64 duration = m_player->duration();
        if (duration > 0) {
            int sliderValue = static_cast<int>(position * 1000 / duration);
            m_progressSlider->setValue(sliderValue);
        }
    }

    qint64 dur = m_player->duration();
    m_timeLabel->setText(formatTime(position) + " / " + formatTime(dur));
}

void VideoPlayerDialog::seekTo(int sliderValue) {
    qint64 duration = m_player->duration();
    if (duration > 0) {
        qint64 pos = static_cast<qint64>(sliderValue) * duration / 1000;
        m_player->setPosition(pos);
    }
}

void VideoPlayerDialog::toggleFullscreen() {
    if (m_isFullscreen) {
        showMaximized();
        m_isFullscreen = false;
        m_fullscreenBtn->setText("⛶");
    } else {
        showFullScreen();
        m_isFullscreen = true;
        m_fullscreenBtn->setText("⊡");
    }
    updateControlBarGeometry();
}

void VideoPlayerDialog::onControlBarTimer() {
    if (m_player->playbackState() == QMediaPlayer::PlayingState) {
        hideControls();
    }
}

void VideoPlayerDialog::onMediaStatusChanged(QMediaPlayer::MediaStatus status) {
    // 播放结束停在最后一帧
    if (status == QMediaPlayer::EndOfMedia) {
        m_player->pause();
        m_player->setPosition(m_player->duration());
        showControls();
    }
}

void VideoPlayerDialog::showControls() {
    m_controlBar->show();
    m_controlsVisible = true;
    setCursor(Qt::ArrowCursor);
    m_controlBarTimer->start();
}

void VideoPlayerDialog::hideControls() {
    if (m_player->playbackState() == QMediaPlayer::PlayingState) {
        m_controlBar->hide();
        m_controlsVisible = false;
        setCursor(Qt::BlankCursor);
    }
}

void VideoPlayerDialog::updateControlBarGeometry() {
    m_videoWidget->setGeometry(0, 0, width(), height());
    m_videoWidget->raise();
    m_controlBar->setGeometry(0, height() - ControlBarHeight, width(), ControlBarHeight);
    m_controlBar->raise();
}

QString VideoPlayerDialog::formatTime(qint64 ms) {
    int totalSec = static_cast<int>(ms / 1000);
    int minutes = totalSec / 60;
    int seconds = totalSec % 60;
    return QString("%1:%2").arg(minutes).arg(seconds, 2, 10, QChar('0'));
}

void VideoPlayerDialog::keyPressEvent(QKeyEvent* event) {
    switch (event->key()) {
    case Qt::Key_Escape:
        if (m_isFullscreen) {
            toggleFullscreen(); // 先退出全屏
        } else {
            m_player->stop();
            hide();
        }
        break;
    case Qt::Key_Space:
        togglePlayPause();
        showControls();
        break;
    case Qt::Key_Left:
        m_player->setPosition(qMax(0LL, m_player->position() - 5000)); // 后退5秒
        showControls();
        break;
    case Qt::Key_Right:
        m_player->setPosition(qMin(m_player->duration(), m_player->position() + 5000)); // 前进5秒
        showControls();
        break;
    default:
        QDialog::keyPressEvent(event);
    }
}

void VideoPlayerDialog::mouseMoveEvent(QMouseEvent* event) {
    Q_UNUSED(event)
    showControls();
}

void VideoPlayerDialog::resizeEvent(QResizeEvent* event) {
    QDialog::resizeEvent(event);
    updateControlBarGeometry();
}

void VideoPlayerDialog::closeEvent(QCloseEvent* event) {
    m_player->stop();
    QDialog::closeEvent(event);
}
