#pragma once

#include <QWidget>
#include <QList>
#include <QMap>
#include "MediaFile.h"
#include "MediaItemWidget.h"

/**
 * @brief 瀑布流布局控件（Masonry Layout）
 * 控件全量创建（保证滚动流畅），缩略图通过 QtConcurrent 后台异步加载（保证点击响应快）
 */
class WaterfallWidget : public QWidget {
    Q_OBJECT

public:
    enum FilterType { All, ImagesOnly, VideosOnly };

    explicit WaterfallWidget(QWidget* parent = nullptr);

    void setFiles(const QList<MediaFile>& files);
    void setFilter(FilterType type);
    void setDarkMode(bool dark);
    void clear();
    void updateThumbnail(const QString& filePath, const QString& thumbnailPath);
    void updatePreview(const QString& filePath, const QString& previewPath);
    QList<MediaFile> filteredFiles() const;

    // 根据当前激活状态标记超出免费配额的视频控件为锁定（悬停显示提示遮罩）
    void markLockedVideos();

signals:
    void itemClicked(const MediaFile& file);

protected:
    void resizeEvent(QResizeEvent* event) override;

private:
    void relayout();
    int  calculateColumnCount() const;
    int  calculateItemHeight(const MediaFile& file, int itemWidth) const;
    int  shortestColumn(const QVector<int>& heights) const;

    QList<MediaFile>                m_allFiles;
    QMap<QString, MediaItemWidget*> m_widgets;

    FilterType m_filter    = All;
    bool       m_darkMode  = false;

    static constexpr int ItemSpacing    = 8;
    static constexpr int MinItemWidth   = 180;
    static constexpr int DefaultAspectH = 150;
};
