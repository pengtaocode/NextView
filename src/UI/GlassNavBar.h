#pragma once

#include "GlassWidget.h"
#include "MarqueeLabel.h"
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QAbstractButton>
#include <QMouseEvent>
#include <QResizeEvent>

/**
 * @brief 实色顶部导航栏
 */
class GlassNavBar : public GlassWidget {
    Q_OBJECT

public:
    explicit GlassNavBar(QWidget* parent = nullptr);

    void addLeftWidget(QWidget* widget);
    void setCenterWidget(QWidget* widget);
    void addRightWidget(QWidget* widget);
    void setTitle(const QString& title);
    void setLeftMaxWidth(int maxW);

    MarqueeLabel* titleLabel() const { return m_titleLabel; }

    /** 是否允许拖动窗口（默认 false，仅状态栏启用） */
    void setDraggable(bool draggable);

    static constexpr int NavBarHeight = 52;

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

signals:
    void settingsClicked();

private:
    QHBoxLayout* m_mainLayout;
    QHBoxLayout* m_leftLayout;
    QHBoxLayout* m_rightLayout;
    QWidget*     m_leftContainer = nullptr;
    QWidget*     m_centerWidget;
    MarqueeLabel* m_titleLabel;
    bool         m_draggable = false;
};
