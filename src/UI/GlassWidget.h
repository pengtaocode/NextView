#pragma once

#include <QWidget>

/**
 * @brief 毛玻璃效果基类
 * 实现 iOS 风格的半透明毛玻璃效果
 * 通过截取背景、缩小再放大来模拟模糊效果
 */
class GlassWidget : public QWidget {
    Q_OBJECT

public:
    explicit GlassWidget(QWidget* parent = nullptr);

    /**
     * @brief 设置内容源控件（要被透过看到的内容）
     * @param w 内容源控件，通常是滚动区域或主内容控件
     */
    void setContentSource(QWidget* w);

    /**
     * @brief 设置深色模式
     */
    void setDarkMode(bool dark);

    /**
     * @brief 获取当前是否深色模式
     */
    bool isDarkMode() const { return m_darkMode; }

    /**
     * @brief 设置模糊强度（1-8，数值越大越模糊）
     */
    void setBlurLevel(int level);

    /**
     * @brief 是否显示底部分隔线（默认显示）
     */
    void setShowBottomLine(bool show);

protected:
    void paintEvent(QPaintEvent* event) override;

    QWidget* m_contentSource;   // 内容源控件
    bool m_darkMode;            // 是否深色模式
    int m_blurLevel;            // 模糊等级（缩小倍数）
    bool m_showBottomLine = true; // 是否显示底部分隔线
};
