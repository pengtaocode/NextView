#pragma once

#include <QWidget>
#include <QScrollArea>
#include <QPushButton>
#include <QLabel>
#include "GlassNavBar.h"
#include "WaterfallWidget.h"
#include "Project.h"
#include "MediaFile.h"

/**
 * @brief 项目浏览页面
 * 以瀑布流展示项目内的所有媒体文件
 * 顶部有返回按钮、项目名称、过滤标签和设置按钮
 */
class BrowsePage : public QWidget {
    Q_OBJECT

public:
    explicit BrowsePage(QWidget* parent = nullptr);

    /**
     * @brief 加载项目和文件列表
     */
    void loadProject(const Project& project, const QList<MediaFile>& files);

    /**
     * @brief 更新缩略图
     */
    void updateThumbnail(const QString& filePath, const QString& thumbnailPath);

    /**
     * @brief 更新预览视频路径
     */
    void updatePreview(const QString& filePath, const QString& previewPath);

    /**
     * @brief 设置深色模式
     */
    void setDarkMode(bool dark);

    /**
     * @brief 获取瀑布流控件
     */
    WaterfallWidget* waterfallWidget() const { return m_waterfallWidget; }

    /**
     * @brief 获取导航栏
     */
    GlassNavBar* navBar() const { return m_navBar; }

signals:
    /**
     * @brief 返回按钮被点击
     */
    void backRequested();

    /**
     * @brief 设置按钮被点击
     */
    void settingsRequested();

    /**
     * @brief 媒体项被点击（请求打开查看器）
     */
    void mediaItemClicked(const MediaFile& file, const QList<MediaFile>& allFiles);

protected:
    void resizeEvent(QResizeEvent* event) override;

private:
    /**
     * @brief 创建过滤标签按钮组
     */
    QWidget* createFilterButtons();

    GlassNavBar* m_navBar;              // 导航栏
    QScrollArea* m_scrollArea;          // 滚动区域
    WaterfallWidget* m_waterfallWidget; // 瀑布流控件

    QPushButton* m_filterAll;           // 全部过滤按钮
    QPushButton* m_filterImages;        // 图片过滤按钮
    QPushButton* m_filterVideos;        // 视频过滤按钮

    Project m_currentProject;           // 当前项目
    QList<MediaFile> m_currentFiles;    // 当前文件列表

    bool m_darkMode;

    void updateFilterButtonStyles(WaterfallWidget::FilterType active);
};
