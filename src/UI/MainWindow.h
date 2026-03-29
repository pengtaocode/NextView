#pragma once

#include <QMainWindow>
#include <QStackedWidget>
#include <QList>
#include <QMap>
#include <QPushButton>
#include <QFrame>
#include <functional>
#include "ProjectListPage.h"
#include "BrowsePage.h"
#include "SettingsPanel.h"
#include "ImageViewer.h"
#include "VideoPlayerDialog.h"
#include "ScanWorker.h"
#include "CacheWorker.h"
#include "Project.h"
#include "MediaFile.h"

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

protected:
    void showEvent(QShowEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void changeEvent(QEvent* event) override;
    void closeEvent(QCloseEvent* event) override;
#ifdef Q_OS_WIN
    bool nativeEvent(const QByteArray& eventType, void* message, qintptr* result) override;
#endif

private slots:
    void onAddProjectRequested();
    void onProjectClicked(const QString& projectId);
    void onBackRequested();
    void onSettingsRequested();
    void onMediaItemClicked(const MediaFile& file, const QList<MediaFile>& allFiles);

    void onScanFileFound(const MediaFile& file);
    void onScanProgress(int found);
    void onScanComplete(const QList<MediaFile>& files);

    void onThumbnailReady(const QString& filePath, const QString& thumbnailPath);
    void onPreviewReady(const QString& filePath, const QString& previewPath);
    void onCacheProgress(int completed, int total);
    void onCacheComplete();

    void onAppearanceChanged(bool isDark);
    void onQualityChanged(const QString& quality);
    void onProjectRemoved(const QString& projectId);

private:
    void applyTheme(bool dark);
    void startScan(const QString& projectId);
    void startZoomTransition(std::function<void()> onDone);  // 截图淡出过渡
    void startCaching(const QString& projectId, const QList<MediaFile>& files);
    QString currentQuality() const;

    void applyRoundedCorners();
    // 非阻塞地取消并销毁当前 CacheWorker（清理移至后台线程）
    void asyncStopCacheWorker();

    // 永久状态栏（始终显示 NextView + 窗口控制按钮）
    GlassNavBar* m_statusBar;
    QFrame*      m_titleLine = nullptr;   // NextView 标题下方装饰线
    QWidget*     m_resizeHandles[8] = {}; // 8 方向缩放手柄

    // 页面
    QStackedWidget*  m_stackedWidget;
    ProjectListPage* m_projectListPage;  // index 0
    BrowsePage*      m_browsePage;       // index 1
    SettingsPanel*   m_settingsPanel;    // index 2

    // 媒体查看器
    ImageViewer*       m_imageViewer;
    VideoPlayerDialog* m_videoPlayer;

    // 后台工作
    ScanWorker*  m_scanWorker;
    QString      m_scanningProjectId;
    CacheWorker* m_cacheWorker;
    QString      m_cachingProjectId;
    QThread*     m_cacheThread;

    // 启动时后台索引（为已扫描项目填充搜索索引，不影响 UI）
    ScanWorker*   m_indexWorker   = nullptr;
    QStringList   m_indexQueue;
    void          indexNextProject();
    void          onIndexComplete(const QList<MediaFile>& files, const QString& projectId);

    // 页面切换动画层
    QWidget* m_zoomOverlay = nullptr;

    // 状态
    QString m_currentProjectId;
    int     m_prevPageIndex;
    QMap<QString, QList<MediaFile>> m_projectFiles;
    bool    m_darkMode;
    QString m_previewQuality;
};
