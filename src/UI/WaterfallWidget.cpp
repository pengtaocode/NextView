#include "WaterfallWidget.h"

#include <QResizeEvent>
#include <QFutureWatcher>
#include <QtConcurrent/QtConcurrent>
#include <algorithm>

WaterfallWidget::WaterfallWidget(QWidget* parent)
    : QWidget(parent), m_filter(All)
{
    setMinimumWidth(400);
}

void WaterfallWidget::setFiles(const QList<MediaFile>& files) {
    // ── 增量更新：保留已有控件（不丢缩略图），只处理新增和删除 ──────────────

    // 新文件集合
    QSet<QString> newPaths;
    for (const MediaFile& f : files) newPaths.insert(f.path);

    // 删除已不在新列表中的控件
    for (auto it = m_widgets.begin(); it != m_widgets.end(); ) {
        if (!newPaths.contains(it.key())) {
            it.value()->stopPreview();
            it.value()->deleteLater();
            it = m_widgets.erase(it);
        } else {
            ++it;
        }
    }

    m_allFiles = files;

    // 仅为新增文件创建控件，已有控件保留原有缩略图
    QList<MediaFile> addedFiles;
    for (const MediaFile& file : files) {
        if (m_widgets.contains(file.path)) continue;

        auto* widget = new MediaItemWidget(file, this);
        widget->setDarkMode(m_darkMode);
        m_widgets.insert(file.path, widget);
        connect(widget, &MediaItemWidget::clicked,
                this, &WaterfallWidget::itemClicked);
        addedFiles.append(file);
    }

    relayout();

    // 只对新增文件异步加载缩略图（QImage 可在非 GUI 线程创建）
    for (const MediaFile& file : addedFiles) {
        if (!file.hasThumbnail()) continue;

        const QString filePath  = file.path;
        const QString thumbPath = file.thumbnailPath;

        auto* watcher = new QFutureWatcher<QImage>(this);
        connect(watcher, &QFutureWatcher<QImage>::finished, this,
                [this, filePath, watcher]() {
            QImage img = watcher->result();
            watcher->deleteLater();
            if (img.isNull() || !m_widgets.contains(filePath)) return;
            m_widgets[filePath]->setThumbnail(QPixmap::fromImage(std::move(img)));
        });
        watcher->setFuture(QtConcurrent::run([thumbPath]() -> QImage {
            return QImage(thumbPath);
        }));
    }
}

void WaterfallWidget::setFilter(FilterType type) {
    if (m_filter == type) return;
    m_filter = type;

    for (auto it = m_widgets.begin(); it != m_widgets.end(); ++it) {
        const MediaFile& file = it.value()->mediaFile();
        bool visible = true;
        if (m_filter == ImagesOnly && file.type != MediaFile::Image) visible = false;
        else if (m_filter == VideosOnly && file.type != MediaFile::Video) visible = false;
        it.value()->setVisible(visible);
    }

    relayout();
}

void WaterfallWidget::setDarkMode(bool dark) {
    m_darkMode = dark;
    for (auto* w : m_widgets.values())
        w->setDarkMode(dark);
}

void WaterfallWidget::clear() {
    for (auto* w : m_widgets.values()) {
        w->stopPreview();
        w->deleteLater();
    }
    m_widgets.clear();
    m_allFiles.clear();
    setMinimumHeight(0);
}

void WaterfallWidget::updateThumbnail(const QString& filePath, const QString& thumbnailPath) {
    if (!m_widgets.contains(filePath)) return;
    QPixmap pixmap(thumbnailPath);
    if (!pixmap.isNull()) {
        m_widgets[filePath]->setThumbnail(pixmap);
        for (MediaFile& f : m_allFiles)
            if (f.path == filePath) { f.thumbnailPath = thumbnailPath; break; }
    }
}

void WaterfallWidget::updatePreview(const QString& filePath, const QString& previewPath) {
    if (!m_widgets.contains(filePath)) return;
    m_widgets[filePath]->setPreviewPath(previewPath);
    for (MediaFile& f : m_allFiles)
        if (f.path == filePath) { f.previewPath = previewPath; break; }
}

QList<MediaFile> WaterfallWidget::filteredFiles() const {
    QList<MediaFile> result;
    for (const MediaFile& f : m_allFiles) {
        if (m_filter == All) result.append(f);
        else if (m_filter == ImagesOnly && f.type == MediaFile::Image) result.append(f);
        else if (m_filter == VideosOnly && f.type == MediaFile::Video) result.append(f);
    }
    return result;
}

void WaterfallWidget::relayout() {
    int cols     = calculateColumnCount();
    int totalW   = width();
    int spacing  = ItemSpacing;
    int available = totalW - spacing * (cols + 1);
    int itemWidth = qMax(80, available / cols);

    int leftover = available - itemWidth * cols;
    int offsetX  = spacing + leftover / 2;

    QVector<int> colHeights(cols, 0);

    for (const MediaFile& file : m_allFiles) {
        if (!m_widgets.contains(file.path)) continue;
        MediaItemWidget* widget = m_widgets[file.path];

        bool show = true;
        if (m_filter == ImagesOnly && file.type != MediaFile::Image) show = false;
        if (m_filter == VideosOnly && file.type != MediaFile::Video) show = false;
        widget->setVisible(show);
        if (!show) continue;

        int col = shortestColumn(colHeights);
        int x   = offsetX + col * (itemWidth + spacing);
        int y   = colHeights[col];
        int h   = qMax(calculateItemHeight(file, itemWidth), 80);

        widget->setGeometry(x, y, itemWidth, h);
        colHeights[col] += h + spacing;
    }

    int maxHeight = *std::max_element(colHeights.begin(), colHeights.end()) + spacing;
    setMinimumHeight(maxHeight);
}

int WaterfallWidget::calculateColumnCount() const {
    int w = width();
    if (w < 1) return 2;
    return qMax(2, w / 480);
}

int WaterfallWidget::calculateItemHeight(const MediaFile& file, int itemWidth) const {
    if (file.dimensions.isValid() && file.dimensions.width() > 0) {
        double ratio = static_cast<double>(file.dimensions.height()) / file.dimensions.width();
        ratio = qBound(0.4, ratio, 2.5);
        return static_cast<int>(itemWidth * ratio);
    }
    if (file.type == MediaFile::Video)
        return static_cast<int>(itemWidth * 9.0 / 16.0);
    return DefaultAspectH;
}

int WaterfallWidget::shortestColumn(const QVector<int>& heights) const {
    int minIdx = 0;
    for (int i = 1; i < heights.size(); ++i)
        if (heights[i] < heights[minIdx]) minIdx = i;
    return minIdx;
}

void WaterfallWidget::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    relayout();
}
