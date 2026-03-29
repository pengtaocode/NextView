#include "SplashOverlay.h"
#include "AnimUtils.h"

#include <QPainter>
#include <QPainterPath>
#include <QTimer>
#include <QVariantAnimation>
#include <QEasingCurve>
#include <QResizeEvent>
#include <QFont>
#include <QFontMetrics>
#include <QGraphicsOpacityEffect>
#include <QDateTime>

SplashOverlay::SplashOverlay(QWidget* parent)
    : QWidget(parent)
{
    resize(parent->size());
    raise();
    // 跟随父窗口大小变化
    parent->installEventFilter(this);

    // QGraphicsOpacityEffect 负责整体淡出合成
    m_opacityEffect = new QGraphicsOpacityEffect(this);
    m_opacityEffect->setOpacity(1.0);
    setGraphicsEffect(m_opacityEffect);

    // 扫光动画定时器：用墙上时钟驱动，与帧率/定时器精度完全无关
    // phase = elapsed_ms / 2000 → 恰好 2 秒从 0 扫到 1
    m_shimmerStartMs = QDateTime::currentMSecsSinceEpoch();
    m_shimmerTimer = new QTimer(this);
    m_shimmerTimer->setInterval(screenAnimInterval());
    connect(m_shimmerTimer, &QTimer::timeout, this, [this]() {
        qint64 elapsed = QDateTime::currentMSecsSinceEpoch() - m_shimmerStartMs;
        m_shimmerPhase = qMin(float(elapsed) / 2000.0f, 1.0f);
        update();
    });
    m_shimmerTimer->start();

    // 2 秒后开始消散（恰好完成一次完整扫光循环，约 62fps × 0.008/帧 = 0.5/秒 → 2 秒一圈）
    QTimer::singleShot(2000, this, &SplashOverlay::startDismiss);
}

bool SplashOverlay::eventFilter(QObject* obj, QEvent* event) {
    if (obj == parent() && event->type() == QEvent::Resize) {
        resize(static_cast<QResizeEvent*>(event)->size());
    }
    return false;
}

void SplashOverlay::startDismiss() {
    m_shimmerTimer->stop();

    m_dismissAnim = new QVariantAnimation(this);
    m_dismissAnim->setStartValue(0.0f);
    m_dismissAnim->setEndValue(1.0f);
    m_dismissAnim->setDuration(500);
    m_dismissAnim->setEasingCurve(QEasingCurve::OutQuart);

    connect(m_dismissAnim, &QVariantAnimation::valueChanged,
            this, [this](const QVariant& v) {
        float t = v.toFloat();
        m_opacityEffect->setOpacity(1.0 - t);   // 1 → 0
        m_scale = 1.0f + t * 0.22f;             // 1 → 1.22（向外扩散）
        update();
    });
    connect(m_dismissAnim, &QVariantAnimation::finished,
            this, &QObject::deleteLater);

    m_dismissAnim->start();
}

void SplashOverlay::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);

    const double cx = width()  / 2.0;
    const double cy = height() / 2.0;

    // 放大变换（以窗口中心为基点），制造内容向外扩散的视觉效果
    p.translate(cx, cy);
    p.scale(m_scale, m_scale);
    p.translate(-cx, -cy);

    // ── 背景 ────────────────────────────────────────────────────
    p.fillRect(rect(), QColor(8, 8, 18));

    // ── 字体 ────────────────────────────────────────────────────
    QFont font("Segoe UI", -1, QFont::Bold);
    font.setPixelSize(96);
    font.setLetterSpacing(QFont::AbsoluteSpacing, 16);
    p.setFont(font);

    const QString    text = "NEXTVIEW";
    const QFontMetrics fm(font);
    const int        tw   = fm.horizontalAdvance(text);
    // 基线位置：垂直居中（ascent 在基线上方，descent 在下方）
    const double     tx   = cx - tw / 2.0;
    const double     ty   = cy + (fm.ascent() - fm.descent()) / 2.0;

    // ── 文字路径 ─────────────────────────────────────────────────
    QPainterPath path;
    path.addText(tx, ty, font, text);

    // 第一层：基础蓝色渐变
    QLinearGradient base(tx, 0, tx + tw, 0);
    base.setColorAt(0.0, QColor(0,  65, 200));
    base.setColorAt(0.5, QColor(20, 110, 255));
    base.setColorAt(1.0, QColor(0,  65, 200));
    p.fillPath(path, QBrush(base));

    // 第二层：扫光高亮（裁剪到文字轮廓内）
    // shimmerPhase 0→1 对应高亮中心从文字左侧扫到右侧
    const double shimCenter = tx - 80.0 + m_shimmerPhase * (tw + 160.0);
    QLinearGradient shimmer(shimCenter - 80, 0, shimCenter + 80, 0);
    shimmer.setColorAt(0.0, QColor(160, 225, 255,   0));
    shimmer.setColorAt(0.5, QColor(160, 225, 255, 215));
    shimmer.setColorAt(1.0, QColor(160, 225, 255,   0));

    p.save();
    p.setClipPath(path);
    const QRectF pb = path.boundingRect();
    p.fillRect(QRectF(shimCenter - 80, pb.top(), 160, pb.height()), shimmer);
    p.restore();
}
