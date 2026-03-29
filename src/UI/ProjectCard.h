#pragma once

#include <QWidget>
#include <QTimer>
#include <QPixmap>
#include "Project.h"

/**
 * @brief 项目卡片控件（3:4比例，图片填满，文字叠加在底部渐变上）
 * 无任何子 QWidget，所有内容在 paintEvent 中手动绘制，彻底避免子控件白色背景渗透问题
 */
class ProjectCard : public QWidget {
    Q_OBJECT

public:
    explicit ProjectCard(const Project& project, QWidget* parent = nullptr);

    void updateProject(const Project& project);
    void setThumbnail(const QPixmap& pixmap);
    void setDarkMode(bool dark) { m_darkMode = dark; update(); }

    QString projectId() const { return m_project.id; }

signals:
    void clicked(const QString& projectId);
    void deleteRequested(const QString& projectId);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    void setupUI();
    void refreshDisplay();
    QRect deleteBtnRect() const;
    static QString formatSize(qint64 bytes);

    Project  m_project;
    bool     m_hovered           = false;
    bool     m_darkMode          = false;
    bool     m_deleteBtnHovered  = false;
    QPixmap  m_thumbnail;
    QPixmap  m_scaledThumb;   // 缓存：按卡片尺寸预缩放，避免每帧重新插值

    // 文字内容（paintEvent 直接绘制，无 QLabel 子控件）
    QString  m_statsText;
    QString  m_statusText;
    bool     m_showStatus        = false;

    // 进度条（paintEvent 手动绘制，无 QProgressBar 子控件）
    bool     m_showProgress      = false;
    int      m_pbValue           = 0;
    int      m_pbMax             = 0;

    // 进度动画
    QTimer*  m_animTimer         = nullptr;
    int      m_animTarget        = 0;
    int      m_cacheTotal        = 0;
    float    m_visualProgress    = 0.0f;
    float    m_cacheSpeed        = 0.0f;
    qint64   m_generatingStartMs = 0;
    int      m_generatingStartVal= 0;
    bool     m_finishing         = false;

    // 旋转等待动效
    QTimer*  m_spinTimer         = nullptr;
    float    m_spinAngle         = 0.0f;

    // 悬停图片缩放动效
    QTimer*  m_scaleTimer        = nullptr;
    float    m_hoverScale        = 1.0f;  // 1.0（普通）→ 1.06（悬停）
};
