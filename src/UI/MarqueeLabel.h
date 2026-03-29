#pragma once

#include <QWidget>
#include <QTimer>
#include <QPainter>
#include <QFontMetrics>
#include <QEnterEvent>
#include "AnimUtils.h"

/**
 * @brief 超长文字滚动标签
 * - 正常显示：超出 maxDisplayWidth 时截断为 "..."
 * - 鼠标悬停：延迟后向左平滑滚动，展示完整内容，结束后复位
 */
class MarqueeLabel : public QWidget {
    Q_OBJECT

public:
    explicit MarqueeLabel(QWidget* parent = nullptr)
        : QWidget(parent)
        , m_offset(0.0f)
        , m_scrolling(false)
    {
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        setMouseTracking(true);

        m_scrollTimer = new QTimer(this);
        m_scrollTimer->setInterval(screenAnimInterval());
        connect(m_scrollTimer, &QTimer::timeout, this, [this]() {
            QFontMetrics fm(m_font);
            int fullW  = fm.horizontalAdvance(m_full);
            int dispW  = effectiveMaxW();
            // 保持 60px/秒固定滚动速度，亚像素精度
            float step = 60.0f * m_scrollTimer->interval() / 1000.0f;
            if (m_offset < float(fullW - dispW + 32)) {
                m_offset += step;
            } else {
                m_scrollTimer->stop();
                // 停留后复位
                QTimer::singleShot(700, this, [this]() {
                    m_offset = 0;
                    update();
                    if (underMouse()) {
                        QTimer::singleShot(500, this, [this]() {
                            if (underMouse()) m_scrollTimer->start();
                        });
                    }
                });
            }
            update();
        });
    }

    void setFullText(const QString& text) {
        m_full    = text;
        m_offset  = 0;
        m_scrolling = false;
        m_scrollTimer->stop();
        update();
    }

    void setLabelFont(const QFont& font) {
        m_font = font;
        setFont(font);
        update();
    }

    QSize sizeHint() const override {
        return QSize(effectiveMaxW(), QFontMetrics(m_font).height() + 4);
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::TextAntialiasing);
        p.setPen(palette().windowText().color());

        QFontMetrics fm(m_font);
        int fullW = fm.horizontalAdvance(m_full);
        int dispW = effectiveMaxW();

        if (!m_scrolling || fullW <= dispW) {
            // 静止：居中显示（消除随窗口变宽产生的单侧空白）
            QString display = fullW > dispW
                ? fm.elidedText(m_full, Qt::ElideRight, dispW)
                : m_full;
            p.setFont(m_font);
            p.drawText(rect(), Qt::AlignLeft | Qt::AlignVCenter, display);
        } else {
            // 滚动：从 offset 开始，左对齐绘制完整文字
            p.setClipRect(rect());
            p.setFont(m_font);
            int x = -int(m_offset);
            p.drawText(x, 0, fullW + 40, height(),
                       Qt::AlignVCenter | Qt::AlignLeft, m_full);
        }
    }

    void enterEvent(QEnterEvent* e) override {
        QWidget::enterEvent(e);
        QFontMetrics fm(m_font);
        int fullW = fm.horizontalAdvance(m_full);
        int dispW = effectiveMaxW();
        if (fullW > dispW) {
            m_scrolling = true;
            m_offset    = 0;
            update();
            QTimer::singleShot(500, this, [this]() {
                if (underMouse()) m_scrollTimer->start();
            });
        }
    }

    void leaveEvent(QEvent* e) override {
        QWidget::leaveEvent(e);
        m_scrollTimer->stop();
        m_scrolling = false;
        m_offset    = 0;
        update();
    }

private:
    // 使用控件实际宽度，不用人为的 maxW 限制，避免与布局分配宽度不一致导致误判溢出
    int effectiveMaxW() const {
        return qMax(width(), 40);
    }

    QString m_full;
    QFont   m_font;
    float   m_offset;
    bool    m_scrolling;
    QTimer* m_scrollTimer;
};
