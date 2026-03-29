#pragma once

#include <QDialog>
#include <QLabel>
#include <QPushButton>
#include <QList>
#include <QPixmap>
#include <QPoint>
#include <QTransform>
#include "MediaFile.h"

/**
 * @brief 全屏图片查看器
 * 支持拖拽平移、滚轮缩放、左右切换、双击还原、ESC关闭
 */
class ImageViewer : public QDialog {
    Q_OBJECT

public:
    explicit ImageViewer(QWidget* parent = nullptr);

    /**
     * @brief 打开图片查看器
     * @param files 图片文件列表（仅图片）
     * @param startIndex 起始显示的图片索引
     */
    void open(const QList<MediaFile>& files, int startIndex);

protected:
    void paintEvent(QPaintEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private slots:
    /**
     * @brief 切换到上一张图片
     */
    void showPrevious();

    /**
     * @brief 切换到下一张图片
     */
    void showNext();

private:
    /**
     * @brief 加载指定索引的图片
     */
    void loadImage(int index);

    /**
     * @brief 还原缩放和平移到适应窗口大小
     */
    void fitToWindow();

    /**
     * @brief 更新顶部信息栏
     */
    void updateInfoBar();

    /**
     * @brief 更新导航按钮位置
     */
    void updateNavButtons();

    /**
     * @brief 计算绘制图片的矩形区域
     */
    QRectF imageRect() const;

    QList<MediaFile> m_files;   // 图片文件列表
    int m_currentIndex;          // 当前图片索引
    QPixmap m_currentPixmap;     // 当前图片像素图
    bool m_loading;              // 是否正在加载

    // 变换状态
    double m_scale;              // 缩放比例
    QPointF m_offset;            // 平移偏移

    // 拖拽状态
    bool m_dragging;             // 是否正在拖拽
    QPoint m_dragStart;          // 拖拽起始点
    QPointF m_offsetAtDragStart; // 拖拽开始时的偏移

    // UI 控件
    QWidget* m_infoBar;          // 顶部信息栏
    QLabel* m_filenameLabel;     // 文件名标签
    QLabel* m_indexLabel;        // 序号标签
    QPushButton* m_prevBtn;      // 上一张按钮
    QPushButton* m_nextBtn;      // 下一张按钮
    QPushButton* m_closeBtn;     // 关闭按钮

    static constexpr double MinScale = 0.1;   // 最小缩放比例
    static constexpr double MaxScale = 10.0;  // 最大缩放比例
};
