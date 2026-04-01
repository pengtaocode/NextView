#pragma once
#include <QGuiApplication>
#include <QScreen>
#include <QWidget>
#include <QTimer>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <functional>

#ifdef Q_OS_MAC
#include <QStyleFactory>
#endif

/**
 * @brief macOS 下 QMacStyle 会接管 QPushButton 原生渲染并忽略 stylesheet 中的
 *        border-radius，此函数在 macOS 下为该按钮单独设置 Fusion 风格，使自定义
 *        样式（圆角、背景色等）正常生效；其他平台为空操作。
 */
inline void setupCustomStyledButton(QPushButton* btn) {
#ifdef Q_OS_MAC
    btn->setStyle(QStyleFactory::create("Fusion"));
#else
    Q_UNUSED(btn)
#endif
}

/// 根据主屏刷新率返回动画帧间隔（ms），范围限定在 60–120 Hz
inline int screenAnimInterval() {
    double hz = QGuiApplication::primaryScreen()
                    ? QGuiApplication::primaryScreen()->refreshRate()
                    : 60.0;
    hz = qBound(60.0, hz, 120.0);
    return qRound(1000.0 / hz);
}

// ──────────────────────────────────────────────────────────────
// ZoomTransitionOverlay：页面/内容切换过渡动画层
// 原理：截取当前画面叠在目标背景色之上，快速淡出
// WA_OpaquePaintEvent + WA_NoSystemBackground 保证不预填白色
// ──────────────────────────────────────────────────────────────
class ZoomTransitionOverlay : public QWidget {
public:
    ZoomTransitionOverlay(QWidget* parent,
                          const QPixmap& snapshot,
                          const QColor&  bgColor,
                          const QRect&   contentRect,
                          std::function<void()> onDone)
        : QWidget(parent)
        , m_snap(snapshot)
        , m_bgColor(bgColor)
        , m_onDone(std::move(onDone))
    {
        setAttribute(Qt::WA_OpaquePaintEvent);
        setAttribute(Qt::WA_NoSystemBackground);
        setGeometry(contentRect);
        show();
        raise();

        m_timer = new QTimer(this);
        m_timer->setInterval(16);
        connect(m_timer, &QTimer::timeout, this, [this]() {
            m_elapsed += m_timer->interval();
            if (m_elapsed >= m_duration) {
                m_timer->stop();
                if (m_onDone) m_onDone();
                return;
            }
            update();
        });
        m_timer->start();
    }

protected:
    void paintEvent(QPaintEvent*) override {
        float t = qMin(1.0f, float(m_elapsed) / float(m_duration));
        float alpha = (1.0f - t) * (1.0f - t);   // 二次缓入

        QPainter p(this);
        p.fillRect(rect(), m_bgColor);
        if (!m_snap.isNull() && alpha > 0.0f) {
            p.setOpacity(alpha);
            p.drawPixmap(0, 0, m_snap);
        }
    }

private:
    QPixmap               m_snap;
    QColor                m_bgColor;
    std::function<void()> m_onDone;
    QTimer*               m_timer    = nullptr;
    int                   m_elapsed  = 0;
    static constexpr int  m_duration = 260;
};
