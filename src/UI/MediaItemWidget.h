#pragma once

#include <QWidget>
#include <QLabel>
#include <QTimer>
#include <QPixmap>
#include <QMediaPlayer>
#include <QVideoSink>
#include <QVideoFrame>
#include <QAudioOutput>
#include "MediaFile.h"
#include "LoadingOverlay.h"
#include "AnimUtils.h"

/**
 * @brief 单个媒体文件展示控件
 * 在瀑布流中展示图片或视频
 * 视频支持鼠标悬停0.2秒后自动播放预览（通过 QVideoSink 帧绘制，避免 native 窗口问题）
 */
class MediaItemWidget : public QWidget {
    Q_OBJECT

public:
    explicit MediaItemWidget(const MediaFile& file, QWidget* parent = nullptr);
    ~MediaItemWidget();

    void setThumbnail(const QPixmap& pixmap);
    void setPreviewPath(const QString& path);
    void setDarkMode(bool dark);

    const MediaFile& mediaFile() const { return m_file; }

    void stopPreview();

    static MediaItemWidget* s_currentPreview;

signals:
    void clicked(const MediaFile& file);

protected:
    void paintEvent(QPaintEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;

private slots:
    void startPreview();
    void onPlaybackStateChanged(QMediaPlayer::PlaybackState state);
    void onVideoFrameChanged(const QVideoFrame& frame);

private:
    MediaFile m_file;
    QPixmap   m_thumbnail;
    bool      m_hasThumbnail;
    bool      m_isPlaying;

    QTimer*       m_hoverTimer;
    QMediaPlayer* m_player;
    QVideoSink*   m_videoSink;
    QAudioOutput* m_audioOutput;

    QPixmap m_currentFrame;   // 当前视频帧（paintEvent 直接绘制）

    bool            m_darkMode   = false;
    LoadingOverlay* m_loadingOverlay = nullptr;
    bool            m_isLoading  = false;

    // 底部信息行
    QString   m_addedDateStr;     // 文件添加日期，格式 "yyyy-MM-dd"
    QTimer*   m_scrollTimer      = nullptr;
    float     m_scrollOffset     = 0.0f;  // 文件名水平滚动偏移（px）
    bool      m_nameHovered      = false;
    bool      m_nameOverflows    = false; // 文件名是否超出可用宽度
};
