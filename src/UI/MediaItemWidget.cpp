#include "MediaItemWidget.h"

#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QFontMetrics>
#include <QLinearGradient>

// 静态成员：全局当前播放预览的控件
MediaItemWidget* MediaItemWidget::s_currentPreview = nullptr;

MediaItemWidget::MediaItemWidget(const MediaFile& file, QWidget* parent)
    : QWidget(parent)
    , m_file(file)
    , m_hasThumbnail(false)
    , m_isPlaying(false)
    , m_player(nullptr)
    , m_videoSink(nullptr)
    , m_audioOutput(nullptr)
{
    setMouseTracking(true);
    setCursor(Qt::PointingHandCursor);

    // 悬停延迟计时器
    m_hoverTimer = new QTimer(this);
    m_hoverTimer->setSingleShot(true);
    m_hoverTimer->setInterval(200);
    connect(m_hoverTimer, &QTimer::timeout, this, &MediaItemWidget::startPreview);

    // 只为视频创建播放器
    if (file.type == MediaFile::Video) {
        m_player = new QMediaPlayer(this);
        m_audioOutput = new QAudioOutput(this);
        m_audioOutput->setVolume(0.0f); // 静音
        m_player->setAudioOutput(m_audioOutput);

        // 用 QVideoSink 接收帧数据，在 paintEvent 里绘制
        // 彻底避免 QVideoWidget native HWND 在 Windows 上的坐标初始化问题
        m_videoSink = new QVideoSink(this);
        m_player->setVideoSink(m_videoSink);

        connect(m_videoSink, &QVideoSink::videoFrameChanged,
                this, &MediaItemWidget::onVideoFrameChanged,
                Qt::QueuedConnection);

        connect(m_player, &QMediaPlayer::playbackStateChanged,
                this, &MediaItemWidget::onPlaybackStateChanged);
    }

    setMinimumHeight(80);

    // 文件添加日期（优先 birthTime，降级 lastModified）
    QFileInfo fi(file.path);
    QDateTime dt = fi.birthTime();
    if (!dt.isValid()) dt = fi.lastModified();
    m_addedDateStr = dt.isValid() ? dt.date().toString("yyyy-MM-dd") : QString();

    // 锁定遮罩缩略图缩放动效（250ms 内从 1.0 缓动到 1.06）
    m_scaleTimer = new QTimer(this);
    m_scaleTimer->setInterval(screenAnimInterval());
    connect(m_scaleTimer, &QTimer::timeout, this, [this]() {
        const float target = (m_previewLocked && m_hovered) ? 1.06f : 1.0f;
        const float step   = 0.06f / 2.0f * m_scaleTimer->interval() / 1000.0f;
        if (m_hoverScale < target)
            m_hoverScale = qMin(m_hoverScale + step, target);
        else
            m_hoverScale = qMax(m_hoverScale - step, target);
        if (qFuzzyCompare(m_hoverScale, target))
            m_scaleTimer->stop();
        update();
    });

    // 文件名横向滚动定时器（hover 时按 40px/s 速度滚动，到头后重置循环）
    m_scrollTimer = new QTimer(this);
    m_scrollTimer->setInterval(screenAnimInterval());
    connect(m_scrollTimer, &QTimer::timeout, this, [this]() {
        const int pad = 6;
        QFont fn; fn.setPixelSize(11);
        QFontMetrics fm(fn);
        const int dateW = m_addedDateStr.isEmpty() ? 0 : fm.horizontalAdvance(m_addedDateStr);
        const int gap   = m_addedDateStr.isEmpty() ? 0 : 6;
        int nameAreaW = width() - pad * 2 - dateW - gap;
        float maxOff = float(qMax(0, fm.horizontalAdvance(m_file.name) - nameAreaW));
        if (maxOff <= 0) { m_scrollOffset = 0.0f; m_scrollTimer->stop(); return; }
        m_scrollOffset += 40.0f * m_scrollTimer->interval() / 1000.0f;
        if (m_scrollOffset > maxOff + 20.0f) m_scrollOffset = 0.0f;  // 到头后重置循环
        update();
    });
}

MediaItemWidget::~MediaItemWidget() {
    if (s_currentPreview == this)
        s_currentPreview = nullptr;
}

void MediaItemWidget::setThumbnail(const QPixmap& pixmap) {
    m_thumbnail = pixmap;
    m_hasThumbnail = !pixmap.isNull();
    update();
}

void MediaItemWidget::setPreviewPath(const QString& path) {
    m_file.previewPath = path;
}

void MediaItemWidget::setDarkMode(bool dark) {
    m_darkMode = dark;
    update();
}

void MediaItemWidget::setPreviewLocked(bool locked) {
    m_previewLocked = locked;
    update();
}

void MediaItemWidget::stopPreview() {
    m_hoverTimer->stop();
    if (m_player && m_isPlaying) {
        m_player->stop();
        m_isPlaying = false;
    }
    m_currentFrame = QPixmap();
    update();
}

void MediaItemWidget::startPreview() {
    if (m_file.type != MediaFile::Video) return;
    if (m_file.previewPath.isEmpty()) return;
    if (!QFile::exists(m_file.previewPath)) return;

    // 停止其他正在播放的预览
    if (s_currentPreview && s_currentPreview != this)
        s_currentPreview->stopPreview();
    s_currentPreview = this;

    m_player->setSource(QUrl::fromLocalFile(m_file.previewPath));
    m_player->play();
    m_isPlaying = true;
}

void MediaItemWidget::onPlaybackStateChanged(QMediaPlayer::PlaybackState state) {
    // 播放结束时循环
    if (state == QMediaPlayer::StoppedState && m_isPlaying) {
        m_player->setPosition(0);
        m_player->play();
    }
}

void MediaItemWidget::onVideoFrameChanged(const QVideoFrame& frame) {
    if (!m_isPlaying || !frame.isValid()) return;
    QImage img = frame.toImage();
    if (!img.isNull()) {
        m_currentFrame = QPixmap::fromImage(std::move(img));
        update();
    }
}

void MediaItemWidget::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event)
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    QRect r = rect();

    // 圆角裁剪
    QPainterPath clipPath;
    clipPath.addRoundedRect(r, 8, 8);
    painter.setClipPath(clipPath);

    if (m_isPlaying && !m_currentFrame.isNull()) {
        // 绘制当前视频帧（填充整个控件，保持比例）
        QPixmap scaled = m_currentFrame.scaled(
            r.size(), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
        int x = (r.width()  - scaled.width())  / 2;
        int y = (r.height() - scaled.height()) / 2;
        painter.drawPixmap(x, y, scaled);
    } else if (m_hasThumbnail && !m_thumbnail.isNull()) {
        // 绘制缩略图（锁定悬停时应用与 ProjectCard 一致的放大动效）
        QPixmap scaled = m_thumbnail.scaled(
            r.size(), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
        int x = (r.width()  - scaled.width())  / 2;
        int y = (r.height() - scaled.height()) / 2;
        if (m_hoverScale > 1.0f) {
            painter.save();
            painter.translate(r.width() / 2.0, r.height() / 2.0);
            painter.scale(m_hoverScale, m_hoverScale);
            painter.translate(-r.width() / 2.0, -r.height() / 2.0);
            painter.drawPixmap(x, y, scaled);
            painter.restore();
        } else {
            painter.drawPixmap(x, y, scaled);
        }
    } else {
        // 占位符：深色模式黑色，浅色模式白色
        painter.fillRect(r, m_darkMode ? QColor(0, 0, 0) : QColor(255, 255, 255));
        painter.setPen(QColor(150, 150, 150));
        painter.setFont(QFont("", 24));
        QString icon = (m_file.type == MediaFile::Video) ? "▶" : "🖼";
        painter.drawText(r, Qt::AlignCenter, icon);
    }

    // 未播放时：叠加视频信息
    if (m_file.type == MediaFile::Video && !m_isPlaying) {
        // 底部渐变遮罩
        QLinearGradient gradient(0, r.height() - 40, 0, r.height());
        gradient.setColorAt(0, QColor(0, 0, 0, 0));
        gradient.setColorAt(1, QColor(0, 0, 0, 140));
        painter.fillRect(r, gradient);

        // 播放图标
        painter.setPen(QColor(255, 255, 255, 180));
        QFont iconFont = painter.font();
        iconFont.setPixelSize(28);
        painter.setFont(iconFont);
        painter.drawText(r, Qt::AlignCenter, "▶");

        // 时长标签
        QString duration = m_file.formattedDuration();
        if (!duration.isEmpty()) {
            QFont durFont = painter.font();
            durFont.setPixelSize(11);
            durFont.setWeight(QFont::Medium);
            painter.setFont(durFont);

            QFontMetrics fm(durFont);
            QRect textRect = fm.boundingRect(duration);
            QRect bgRect = textRect.adjusted(-5, -3, 5, 3);
            bgRect.moveBottomRight(r.bottomRight() + QPoint(-6, -6));

            painter.setPen(Qt::NoPen);
            painter.setBrush(QColor(0, 0, 0, 160));
            painter.drawRoundedRect(bgRect, 3, 3);

            painter.setPen(Qt::white);
            painter.drawText(bgRect, Qt::AlignCenter, duration);
        }
    }

    // 未激活超出预览配额：鼠标悬停时显示锁定提示遮罩
    if (m_previewLocked && m_hovered && !m_isPlaying) {
        painter.fillRect(r, QColor(0, 0, 0, 165));

        QFont hintFont;
        hintFont.setPixelSize(11);
        hintFont.setWeight(QFont::Medium);
        painter.setFont(hintFont);
        painter.setPen(Qt::white);
        painter.drawText(r.adjusted(8, 0, -8, 0),
                         Qt::AlignCenter | Qt::TextWordWrap,
                         "前往设置激活\n解锁视频预览");
    }

    // 底部单行信息：文件名（左）+ 日期（右），同色
    if (!m_isPlaying) {
        // 图片补渐变遮罩（视频已有自己的渐变）
        if (m_file.type == MediaFile::Image) {
            QLinearGradient fnGrad(0, r.height() - 30, 0, r.height());
            fnGrad.setColorAt(0, QColor(0, 0, 0, 0));
            fnGrad.setColorAt(1, QColor(0, 0, 0, 140));
            painter.fillRect(QRect(0, r.height() - 30, r.width(), 30), fnGrad);
        }

        QFont fnFont;
        fnFont.setPixelSize(11);
        painter.setFont(fnFont);
        QFontMetrics fnFm(fnFont);
        const int pad = 6;
        const int rowH = 16;
        const int rowY = r.height() - rowH - 4;

        painter.setPen(QColor(255, 255, 255, 200));

        // 日期固定在右侧
        const int dateW = m_addedDateStr.isEmpty() ? 0
                          : fnFm.horizontalAdvance(m_addedDateStr);
        const int gap   = m_addedDateStr.isEmpty() ? 0 : 6;
        if (!m_addedDateStr.isEmpty()) {
            painter.drawText(QRect(r.width() - pad - dateW, rowY, dateW, rowH),
                             Qt::AlignLeft | Qt::AlignVCenter, m_addedDateStr);
        }

        // 文件名占剩余宽度，左侧省略或 hover 时滚动
        const int nameAreaW = r.width() - pad * 2 - dateW - gap;
        const int nameFullW = fnFm.horizontalAdvance(m_file.name);

        if (m_nameHovered && m_nameOverflows) {
            float maxOff = float(qMax(0, nameFullW - nameAreaW));
            float off = qMin(m_scrollOffset, maxOff);
            painter.save();
            painter.setClipRect(pad, rowY, nameAreaW, rowH);
            float textX = float(pad + nameAreaW - nameFullW) + off;
            painter.drawText(QPointF(textX, rowY + fnFm.ascent() + (rowH - fnFm.height()) / 2.0f), m_file.name);
            painter.restore();
        } else {
            QString elided = fnFm.elidedText(m_file.name, Qt::ElideLeft, nameAreaW);
            bool wasElided = (elided != m_file.name);
            Qt::Alignment align = wasElided
                ? (Qt::AlignRight | Qt::AlignVCenter)   // 省略时右对齐，紧贴日期，显示文件名尾部
                : (Qt::AlignLeft  | Qt::AlignVCenter);  // 完整显示时左对齐，避免右侧大片空白
            painter.drawText(QRect(pad, rowY, nameAreaW, rowH), align, elided);
        }
    }
}

void MediaItemWidget::enterEvent(QEnterEvent* event) {
    Q_UNUSED(event)
    m_hovered = true;
    if (m_previewLocked)
        m_scaleTimer->start();
    if (m_file.type == MediaFile::Video && !m_previewLocked)
        m_hoverTimer->start();

    m_nameHovered = true;
    // 只有图片才在 hover 时滚动文件名；视频 hover 时显示预览，无需滚动
    if (m_file.type == MediaFile::Image) {
        QFont fn; fn.setPixelSize(11);
        QFontMetrics fm(fn);
        const int dateW = m_addedDateStr.isEmpty() ? 0 : fm.horizontalAdvance(m_addedDateStr);
        const int gap   = m_addedDateStr.isEmpty() ? 0 : 6;
        int nameAreaW = width() - 6 * 2 - dateW - gap;
        m_nameOverflows = (fm.elidedText(m_file.name, Qt::ElideLeft, nameAreaW) != m_file.name);
        if (m_nameOverflows) m_scrollTimer->start();
    }
    update();
}

void MediaItemWidget::leaveEvent(QEvent* event) {
    Q_UNUSED(event)
    m_hovered       = false;
    if (m_previewLocked)
        m_scaleTimer->start();
    m_nameHovered   = false;
    m_nameOverflows = false;
    m_scrollOffset  = 0.0f;
    m_scrollTimer->stop();

    m_hoverTimer->stop();
    if (m_isPlaying)
        stopPreview();
    update();
}

void MediaItemWidget::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton && !m_isLoading) {
        m_isLoading = true;
        m_hoverTimer->stop();
        if (m_isPlaying) stopPreview();

        m_loadingOverlay = new LoadingOverlay(this);
        m_loadingOverlay->resize(size());
        m_loadingOverlay->show();
        m_loadingOverlay->raise();

        QTimer::singleShot(700, this, [this]() {
            if (m_loadingOverlay) {
                m_loadingOverlay->fadeOut();
                m_loadingOverlay = nullptr;
            }
            m_isLoading = false;
            emit clicked(m_file);
        });
    }
}
