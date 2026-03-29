#include "ProjectCard.h"
#include "AnimUtils.h"

#include <QPainter>
#include <QPainterPath>
#include <QLinearGradient>
#include <QConicalGradient>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QFont>
#include <QFontMetrics>
#include <cmath>
#include <QDateTime>

ProjectCard::ProjectCard(const Project& project, QWidget* parent)
    : QWidget(parent)
    , m_project(project)
{
    // 禁止 Qt 在 paintEvent 前用调色板白色填充背景，彻底消除白色闪烁
    setAttribute(Qt::WA_OpaquePaintEvent);
    setAttribute(Qt::WA_NoSystemBackground);
    setupUI();
    refreshDisplay();
    setMouseTracking(true);
    setCursor(Qt::PointingHandCursor);
    setMinimumWidth(120);
}

void ProjectCard::setupUI() {
    // 速度驱动进度动画，适配屏幕刷新率
    m_animTimer = new QTimer(this);
    m_animTimer->setInterval(screenAnimInterval());
    connect(m_animTimer, &QTimer::timeout, this, [this]() {
        if (m_cacheTotal <= 0) return;
        const float dt = m_animTimer->interval() / 1000.0f;

        if (m_finishing) {
            // 收尾动画：匀速走完剩余部分
            m_visualProgress += m_cacheSpeed * dt;
            m_visualProgress = qMin(m_visualProgress, float(m_cacheTotal));
            m_pbValue = qBound(0, qRound(m_visualProgress * 500.0f), m_cacheTotal * 500);
            if (m_visualProgress >= float(m_cacheTotal)) {
                m_animTimer->stop();
                m_finishing          = false;
                m_cacheTotal         = 0;
                m_visualProgress     = 0.0f;
                m_cacheSpeed         = 0.0f;
                m_animTarget         = 0;
                m_generatingStartMs  = 0;
                m_generatingStartVal = 0;
                m_pbMax              = 0;
                m_pbValue            = 0;
                m_showProgress       = false;
                m_showStatus         = false;
            }
            update();
            return;
        }

        // 累计均速 = 动画开始后新增完成数 / 经过时间
        qint64 elapsed = QDateTime::currentMSecsSinceEpoch() - m_generatingStartMs;
        int deltaProgress = m_animTarget - m_generatingStartVal;
        if (elapsed >= 200 && deltaProgress > 0)
            m_cacheSpeed = 1000.0f * deltaProgress / elapsed;

        if (m_cacheSpeed > 0.0f) {
            m_visualProgress += m_cacheSpeed * dt;
            // 超前量：max(2个视频, 2s 的解析量)，消除批次间停顿
            float lead = qMax(2.0f, m_cacheSpeed * 2.0f);
            float cap  = qMin(float(m_cacheTotal) - 0.01f, float(m_animTarget) + lead);
            m_visualProgress = qMin(m_visualProgress, cap);
        }

        m_pbValue = qBound(0, qRound(m_visualProgress * 500.0f), m_cacheTotal * 500);
        update();
    });

    // 悬停图片缩放动效（250ms 内从 1.0 缓动到 1.06）
    m_scaleTimer = new QTimer(this);
    m_scaleTimer->setInterval(screenAnimInterval());
    connect(m_scaleTimer, &QTimer::timeout, this, [this]() {
        const float target = m_hovered ? 1.06f : 1.0f;
        const float step   = 0.06f / 2.0f * m_scaleTimer->interval() / 1000.0f; // 2 秒走完 0.06 的缩放量
        m_hoverScale = (m_hoverScale < target)
                       ? qMin(m_hoverScale + step, target)
                       : qMax(m_hoverScale - step, target);
        if (m_hoverScale == target)
            m_scaleTimer->stop();
        update();
    });

    // 无缩略图旋转等待动效，适配屏幕刷新率
    m_spinTimer = new QTimer(this);
    m_spinTimer->setInterval(screenAnimInterval());
    connect(m_spinTimer, &QTimer::timeout, this, [this]() {
        m_spinAngle = std::fmod(m_spinAngle + 300.0f * m_spinTimer->interval() / 1000.0f, 360.0f);
        update();
    });
}

void ProjectCard::refreshDisplay() {
    if (m_project.imageCount > 0 || m_project.videoCount > 0) {
        m_statsText = QString("图片 %1  ·  视频 %2").arg(m_project.imageCount).arg(m_project.videoCount);
    } else if (m_project.scanState == Project::Scanned) {
        m_statsText = "无媒体文件";
    } else {
        m_statsText = "未扫描";
    }

    bool busy = false;
    if (m_project.scanState == Project::Scanning) {
        m_statusText   = QString("扫描中... %1 个").arg(m_project.scanFound);
        m_showStatus   = true;
        m_pbMax        = 0;
        m_pbValue      = 0;
        m_showProgress = true;
        busy           = true;
    } else if (m_project.cacheState == Project::Generating) {
        m_statusText = QString("生成预览 %1/%2").arg(m_project.cacheProgress).arg(m_project.cacheTotal);
        m_showStatus = true;
        m_cacheTotal = m_project.cacheTotal;
        // range × 500：每个视频被分成 500 格，视觉上完全连续
        if (m_pbMax != m_cacheTotal * 500) {
            m_pbMax   = m_cacheTotal * 500;
            m_pbValue = 0;
        }
        m_animTarget = m_project.cacheProgress;
        if (!m_animTimer->isActive()) {
            m_visualProgress     = float(m_animTarget);
            m_generatingStartMs  = QDateTime::currentMSecsSinceEpoch();
            m_generatingStartVal = m_animTarget;
            m_animTimer->start();
        }
        m_showProgress = true;
        busy           = true;
    } else {
        if (m_animTimer->isActive() && !m_finishing && m_cacheTotal > 0) {
            // 刚完成：启动收尾动画，匀速走完剩余部分
            m_finishing  = true;
            float remaining = float(m_cacheTotal) - m_visualProgress;
            m_cacheSpeed = remaining > 0.0f ? remaining : 1.0f;
            m_showStatus = false;
        } else if (!m_animTimer->isActive()) {
            m_animTarget         = 0;
            m_cacheTotal         = 0;
            m_visualProgress     = 0.0f;
            m_cacheSpeed         = 0.0f;
            m_generatingStartMs  = 0;
            m_generatingStartVal = 0;
            m_finishing          = false;
            m_pbMax              = 0;
            m_pbValue            = 0;
            m_showStatus         = false;
            m_showProgress       = false;
        }
    }

    if (m_thumbnail.isNull() && busy) {
        if (!m_spinTimer->isActive()) m_spinTimer->start();
    } else {
        m_spinTimer->stop();
    }

    update();
}

void ProjectCard::updateProject(const Project& project) {
    m_project = project;
    refreshDisplay();
}

void ProjectCard::setThumbnail(const QPixmap& pixmap) {
    m_thumbnail = pixmap;
    m_scaledThumb = pixmap.isNull() ? QPixmap()
        : pixmap.scaled(size(), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
    m_spinTimer->stop();
    update();
}

QRect ProjectCard::deleteBtnRect() const {
    QFont btnFont;
    btnFont.setPixelSize(13);
    btnFont.setWeight(QFont::Bold);
    QFontMetrics bfm(btnFont);
    const int hPad = 14, vPad = 5;
    int btnW = bfm.horizontalAdvance("删除") + 2 * hPad;
    int btnH = bfm.height() + 2 * vPad;
    return QRect(width() - btnW - 8, 8, btnW, btnH);
}

void ProjectCard::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event)
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    const int w = width();
    const int h = height();
    const int pad = 10;
    const int lineGap = 5;

    // 先用窗口背景色填满整个 rect（含圆角外区域），避免 WA_OpaquePaintEvent 下角落变黑
    painter.fillRect(rect(), palette().color(QPalette::Window));

    // 圆角裁剪（仅对卡片内容生效）
    QPainterPath clipPath;
    clipPath.addRoundedRect(rect(), 12, 12);
    painter.setClipPath(clipPath);

    // ── 背景 ─────────────────────────────────────────────────────────
    if (!m_scaledThumb.isNull()) {
        // 用 painter 变换做缩放（避免每帧重新插值），超出卡片的部分被圆角 clipPath 裁掉
        painter.save();
        painter.translate(w / 2.0, h / 2.0);
        painter.scale(m_hoverScale, m_hoverScale);
        painter.translate(-w / 2.0, -h / 2.0);
        int xOff = (m_scaledThumb.width()  - w) / 2;
        int yOff = (m_scaledThumb.height() - h) / 2;
        painter.drawPixmap(-xOff, -yOff, m_scaledThumb);
        painter.restore();
    } else {
        painter.fillRect(rect(), m_darkMode ? QColor(0, 0, 0) : QColor(60, 60, 65));

        if (m_spinTimer->isActive()) {
            const double cx = w / 2.0, cy = h / 2.0;
            const int r = 26;
            painter.save();
            painter.translate(cx, cy);
            painter.rotate(m_spinAngle);
            QConicalGradient grad(0, 0, 0);
            grad.setColorAt(0.00, QColor(80,  100, 255, 255));
            grad.setColorAt(0.40, QColor(160,  80, 255, 255));
            grad.setColorAt(0.85, QColor(80,  100, 255,  50));
            grad.setColorAt(1.00, QColor(80,  100, 255,   0));
            painter.setPen(QPen(QBrush(grad), 3.5, Qt::SolidLine, Qt::RoundCap));
            painter.drawArc(-r, -r, r * 2, r * 2, 0, 300 * 16);
            painter.restore();
        } else {
            painter.setPen(QColor(200, 200, 205));
            QFont iconFont = painter.font();
            iconFont.setPixelSize(32);
            painter.setFont(iconFont);
            painter.drawText(rect().adjusted(0, -16, 0, 0), Qt::AlignCenter, "🗂");
        }
    }

    // ── 底部渐变 ──────────────────────────────────────────────────────
    int gradH = qMax(80, h * 45 / 100);
    QRect gradRect(0, h - gradH, w, gradH);
    QLinearGradient grad(gradRect.topLeft(), gradRect.bottomLeft());
    grad.setColorAt(0.0, QColor(0, 0, 0, 0));
    grad.setColorAt(1.0, QColor(0, 0, 0, 190));
    painter.fillRect(gradRect, grad);

    // ── 底部文字（从下往上布局）──────────────────────────────────────
    int y = h - pad;

    // 统计行
    {
        QFont sf; sf.setPixelSize(11);
        painter.setFont(sf);
        painter.setPen(QColor(255, 255, 255, 200));
        painter.drawText(QRect(pad, y - 14, w - 2*pad, 14),
                         Qt::AlignLeft | Qt::AlignVCenter, m_statsText);
        y -= (14 + lineGap);
    }

    // 项目名（超长右侧省略）
    {
        QFont nf; nf.setPixelSize(14); nf.setWeight(QFont::DemiBold);
        painter.setFont(nf);
        painter.setPen(Qt::white);
        QFontMetrics fm(nf);
        QString elided = fm.elidedText(m_project.name, Qt::ElideRight, w - 2*pad);
        painter.drawText(QRect(pad, y - 18, w - 2*pad, 18),
                         Qt::AlignLeft | Qt::AlignVCenter, elided);
        y -= (18 + lineGap);
    }

    // 进度条
    if (m_showProgress) {
        QRect pbRect(pad, y - 4, w - 2*pad, 4);
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(255, 255, 255, 77));
        painter.drawRoundedRect(pbRect, 2, 2);
        if (m_pbMax > 0 && m_pbValue > 0) {
            int chunkW = qRound(float(pbRect.width()) * m_pbValue / m_pbMax);
            if (chunkW > 0) {
                painter.setBrush(Qt::white);
                painter.drawRoundedRect(QRectF(pbRect.x(), pbRect.y(), chunkW, pbRect.height()), 2, 2);
            }
        }
        y -= (4 + lineGap);
    }

    // 状态行
    if (m_showStatus) {
        QFont stf; stf.setPixelSize(11);
        painter.setFont(stf);
        painter.setPen(QColor(255, 255, 255, 200));
        painter.drawText(QRect(pad, y - 16, w - 2*pad, 16),
                         Qt::AlignLeft | Qt::AlignVCenter, m_statusText);
    }

    // ── 悬停效果 ──────────────────────────────────────────────────────
    if (m_hovered) {
        // 蓝色边框
        painter.setPen(QPen(QColor(0, 122, 255, 180), 2));
        painter.setBrush(Qt::NoBrush);
        painter.drawRoundedRect(rect().adjusted(1, 1, -1, -1), 11, 11);

        // 删除按钮
        QRect btnR = deleteBtnRect();
        QColor btnBg = m_deleteBtnHovered ? QColor(255, 100, 90, 255) : QColor(255, 69, 58, 230);
        painter.setPen(Qt::NoPen);
        painter.setBrush(btnBg);
        painter.drawRoundedRect(btnR, btnR.height() / 2.0, btnR.height() / 2.0);
        QFont btnFont; btnFont.setPixelSize(13); btnFont.setWeight(QFont::Bold);
        painter.setFont(btnFont);
        painter.setPen(Qt::white);
        painter.drawText(btnR, Qt::AlignCenter, "删除");
    }
}

void ProjectCard::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        if (m_hovered && deleteBtnRect().contains(event->pos()))
            emit deleteRequested(m_project.id);
        else
            emit clicked(m_project.id);
    }
}

void ProjectCard::mouseMoveEvent(QMouseEvent* event) {
    if (m_hovered) {
        bool onBtn = deleteBtnRect().contains(event->pos());
        if (onBtn != m_deleteBtnHovered) {
            m_deleteBtnHovered = onBtn;
            update();
        }
    }
}

void ProjectCard::enterEvent(QEnterEvent* event) {
    Q_UNUSED(event)
    m_hovered = true;
    m_scaleTimer->start();
    update();
}

void ProjectCard::leaveEvent(QEvent* event) {
    Q_UNUSED(event)
    m_hovered = false;
    m_deleteBtnHovered = false;
    m_scaleTimer->start();
    update();
}

void ProjectCard::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    if (!m_thumbnail.isNull())
        m_scaledThumb = m_thumbnail.scaled(size(), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
}

QString ProjectCard::formatSize(qint64 bytes) {
    if (bytes < 1024) return QString("%1 B").arg(bytes);
    if (bytes < 1024 * 1024) return QString("%1 KB").arg(bytes / 1024);
    if (bytes < 1024LL * 1024 * 1024)
        return QString("%1 MB").arg(bytes / (1024.0 * 1024.0), 0, 'f', 1);
    return QString("%1 GB").arg(bytes / (1024.0 * 1024.0 * 1024.0), 0, 'f', 2);
}
