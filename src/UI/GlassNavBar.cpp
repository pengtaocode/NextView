#include "GlassNavBar.h"

#include <QWidget>
#include <QHBoxLayout>
#include <QLabel>
#include <QFont>
#include <QWindow>

GlassNavBar::GlassNavBar(QWidget* parent)
    : GlassWidget(parent)
    , m_centerWidget(nullptr)
{
    setFixedHeight(NavBarHeight);
    GlassWidget::setShowBottomLine(false);  // 无底部分隔线

    m_mainLayout = new QHBoxLayout(this);
    m_mainLayout->setContentsMargins(16, 0, 16, 0);
    m_mainLayout->setSpacing(8);

    // 左侧容器
    m_leftContainer = new QWidget(this);
    m_leftLayout = new QHBoxLayout(m_leftContainer);
    m_leftLayout->setContentsMargins(0, 0, 0, 0);
    m_leftLayout->setSpacing(4);
    m_leftContainer->setMinimumWidth(60);
    m_leftContainer->setMaximumWidth(200);
    m_leftContainer->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);

    // 中央标题（stretch=1，超长截断+悬停滚动）
    m_titleLabel = new MarqueeLabel(this);
    QFont titleFont = m_titleLabel->font();
    titleFont.setPixelSize(17);
    titleFont.setWeight(QFont::DemiBold);
    m_titleLabel->setLabelFont(titleFont);
    m_titleLabel->setMinimumWidth(0);

    // 右侧容器（不压缩）
    QWidget* rightContainer = new QWidget(this);
    m_rightLayout = new QHBoxLayout(rightContainer);
    m_rightLayout->setContentsMargins(0, 0, 0, 0);
    m_rightLayout->setSpacing(6);
    m_rightLayout->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    rightContainer->setMinimumWidth(60);
    rightContainer->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);

    m_mainLayout->addWidget(m_leftContainer, 0);
    m_mainLayout->addWidget(m_titleLabel, 1);
    m_mainLayout->addWidget(rightContainer, 0);
}

void GlassNavBar::setDraggable(bool draggable) {
    m_draggable = draggable;
}

void GlassNavBar::addLeftWidget(QWidget* widget) {
    m_leftLayout->addWidget(widget);
}

void GlassNavBar::setLeftMaxWidth(int maxW) {
    m_leftContainer->setMaximumWidth(maxW);
}

void GlassNavBar::setCenterWidget(QWidget* widget) {
    if (m_centerWidget) {
        m_mainLayout->removeWidget(m_centerWidget);
        m_centerWidget->hide();
    }
    m_centerWidget = widget;
    if (widget) {
        int idx = m_mainLayout->indexOf(m_titleLabel);
        m_titleLabel->hide();
        m_mainLayout->insertWidget(idx, widget, 1);
    }
}

void GlassNavBar::addRightWidget(QWidget* widget) {
    m_rightLayout->addWidget(widget);
}

void GlassNavBar::setTitle(const QString& title) {
    m_titleLabel->setFullText(title);
    m_titleLabel->show();
}

void GlassNavBar::resizeEvent(QResizeEvent* event) {
    GlassWidget::resizeEvent(event);
}

void GlassNavBar::mousePressEvent(QMouseEvent* event) {
    if (m_draggable && event->button() == Qt::LeftButton) {
        // 点在按钮上时交给按钮处理；其余区域启动系统拖拽
        if (!qobject_cast<QAbstractButton*>(childAt(event->pos()))) {
            if (QWindow* win = window()->windowHandle())
                win->startSystemMove();
            return;
        }
    }
    GlassWidget::mousePressEvent(event);
}

void GlassNavBar::mouseDoubleClickEvent(QMouseEvent* event) {
    if (m_draggable && event->button() == Qt::LeftButton) {
        if (!qobject_cast<QAbstractButton*>(childAt(event->pos()))) {
            window()->isMaximized() ? window()->showNormal() : window()->showMaximized();
            return;
        }
    }
    GlassWidget::mouseDoubleClickEvent(event);
}
