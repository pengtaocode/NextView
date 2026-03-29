#include "GlassWidget.h"

#include <QPainter>
#include <QPaintEvent>

GlassWidget::GlassWidget(QWidget* parent)
    : QWidget(parent)
    , m_contentSource(nullptr)
    , m_darkMode(false)
    , m_blurLevel(4)
{
}

void GlassWidget::setContentSource(QWidget* w) {
    m_contentSource = w;
}

void GlassWidget::setDarkMode(bool dark) {
    m_darkMode = dark;
    update();
}

void GlassWidget::setBlurLevel(int level) {
    m_blurLevel = qBound(1, level, 8);
    update();
}

void GlassWidget::setShowBottomLine(bool show) {
    m_showBottomLine = show;
    update();
}

void GlassWidget::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event)
    QPainter painter(this);

    // 实色背景，不透明
    if (m_darkMode) {
        painter.fillRect(rect(), QColor(28, 28, 30));
    } else {
        painter.fillRect(rect(), QColor(242, 242, 247));
    }

    // 底部细线分隔（可通过 setShowBottomLine 控制）
    if (m_showBottomLine) {
        if (m_darkMode) {
            painter.setPen(QPen(QColor(255, 255, 255, 30), 1));
        } else {
            painter.setPen(QPen(QColor(0, 0, 0, 25), 1));
        }
        painter.drawLine(0, height() - 1, width(), height() - 1);
    }
}
