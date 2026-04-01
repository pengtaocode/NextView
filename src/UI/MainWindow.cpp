#include "MainWindow.h"
#include "ProjectManager.h"
#include "CacheManager.h"
#include "FFmpegHelper.h"
#include "GlassNavBar.h"
#include "SplashOverlay.h"

#include <QFileDialog>
#include <QMessageBox>
#include <QResizeEvent>
#include <QShowEvent>
#include <QCloseEvent>
#include <QSettings>
#include <QThread>
#include <QTimer>
#include <QApplication>
#include <QScreen>
#include <QFile>
#include <QDir>
#include <QPixmap>
#include <QPainter>
#include <QPainterPath>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QAbstractButton>
#include <QDebug>
#include <QMouseEvent>
#include <QWindow>
#include <QDesktopServices>
#include <QUrl>
#include <cmath>

#ifdef Q_OS_WIN
#  include <windows.h>
#  include <windowsx.h>
#endif

#include "AnimUtils.h"

// ──────────────────────────────────────────────────────────────
// ResizeHandle：覆盖在窗口边缘的透明缩放区域，触发 startSystemResize
// ──────────────────────────────────────────────────────────────
class ResizeHandle : public QWidget {
    Qt::Edges m_edges;
public:
    explicit ResizeHandle(Qt::Edges edges, QWidget* parent)
        : QWidget(parent), m_edges(edges)
    {
        if (edges == Qt::TopEdge || edges == Qt::BottomEdge)
            setCursor(Qt::SizeVerCursor);
        else if (edges == Qt::LeftEdge || edges == Qt::RightEdge)
            setCursor(Qt::SizeHorCursor);
        else if (edges == (Qt::TopEdge | Qt::LeftEdge) || edges == (Qt::BottomEdge | Qt::RightEdge))
            setCursor(Qt::SizeFDiagCursor);
        else
            setCursor(Qt::SizeBDiagCursor);
    }
protected:
    void mousePressEvent(QMouseEvent* event) override {
        if (event->button() == Qt::LeftButton && !window()->isMaximized())
            window()->windowHandle()->startSystemResize(m_edges);
    }
};

// ──────────────────────────────────────────────────────────────
// WinButton：Windows 11 风格窗口控制按钮（─  □  ✕）
// ──────────────────────────────────────────────────────────────
class WinButton : public QAbstractButton {
public:
    enum Type { Minimize, Maximize, Close };

    explicit WinButton(Type type, QWidget* parent = nullptr)
        : QAbstractButton(parent), m_type(type)
    {
        setFixedSize(46, 34);
        setCursor(Qt::ArrowCursor);
        setAttribute(Qt::WA_Hover);
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);

        // 图标颜色取自 stylesheet 设置的文字色（自动适配深浅色主题）
        QColor ic = palette().windowText().color();
        bool isDark = ic.lightness() > 128;  // 文字浅 → 背景深

        bool hov = underMouse();
        if (hov) {
            if (m_type == Close)
                p.fillRect(rect(), QColor(196, 43, 28));        // Windows 11 关闭红
            else
                p.fillRect(rect(), isDark
                    ? QColor(255, 255, 255, 28)
                    : QColor(0, 0, 0, 16));
        }

        QColor iconColor = (hov && m_type == Close) ? Qt::white : ic;
        p.setPen(QPen(iconColor, 1.1, Qt::SolidLine, Qt::RoundCap));

        qreal cx = width() / 2.0, cy = height() / 2.0;
        switch (m_type) {
        case Minimize:
            p.drawLine(QPointF(cx - 5, cy + 1), QPointF(cx + 5, cy + 1));
            break;
        case Maximize:
            if (window() && window()->isMaximized()) {
                // 还原：两个叠加小方块
                p.drawRect(QRectF(cx - 3, cy - 1,  7, 7));
                p.drawRect(QRectF(cx - 1, cy - 3,  7, 7));
            } else {
                p.drawRect(QRectF(cx - 5, cy - 5, 10, 10));
            }
            break;
        case Close:
            p.setPen(QPen(iconColor, 1.2, Qt::SolidLine, Qt::RoundCap));
            p.drawLine(QPointF(cx - 5, cy - 5), QPointF(cx + 5, cy + 5));
            p.drawLine(QPointF(cx + 5, cy - 5), QPointF(cx - 5, cy + 5));
            break;
        }
    }

    void enterEvent(QEnterEvent*) override { update(); }
    void leaveEvent(QEvent*)       override { update(); }

private:
    Type m_type;
};

// ──────────────────────────────────────────────────────────────
// GradientLabel：渐变色文字标签（通过 QPainterPath 填充渐变）
// ──────────────────────────────────────────────────────────────
class GradientLabel : public QWidget {
    QString m_text;
    QFont   m_font;
    QColor  m_colorLeft;
    QColor  m_colorRight;
public:
    GradientLabel(const QString& text, QColor left, QColor right, QWidget* parent = nullptr)
        : QWidget(parent), m_text(text), m_colorLeft(left), m_colorRight(right)
    {
        setAttribute(Qt::WA_TranslucentBackground);
        setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    }
    void setLabelFont(const QFont& f) { m_font = f; updateGeometry(); }

    QSize sizeHint() const override {
        return QFontMetrics(m_font).boundingRect(m_text).adjusted(-2, -2, 2, 2).size();
    }
protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);

        QFontMetrics fm(m_font);
        // 垂直居中基线
        int baseline = (height() + fm.ascent() - fm.descent()) / 2;

        QPainterPath path;
        path.addText(0, baseline, m_font, m_text);

        QLinearGradient grad(path.boundingRect().left(),  0,
                             path.boundingRect().right(), 0);
        grad.setColorAt(0.0, m_colorLeft);
        grad.setColorAt(1.0, m_colorRight);

        p.fillPath(path, grad);
    }
};

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , m_scanWorker(nullptr)
    , m_scanningProjectId("")
    , m_cacheWorker(nullptr)
    , m_cachingProjectId("")
    , m_cacheThread(nullptr)
    , m_darkMode(false)
    , m_previewQuality("medium")
    , m_prevPageIndex(0)
{
    // 无边框窗口（方形，Windows 系统下不用圆角）
    setWindowFlag(Qt::FramelessWindowHint);
    setWindowTitle("");
    setMinimumSize(800, 600);
    // 初始宽度 = 3 列瀑布流(3×480) + 4×间距(8) + 滚动条估算(17px) ≈ 1460
    // 初始高度 = 宽度 × 0.618（黄金比例），但不超过屏幕可用区域
    {
        const QRect screen = QApplication::primaryScreen()->availableGeometry();
        int w = qMin(1460, screen.width());
        int h = qMin(qRound(w * 0.618) + 7, screen.height());
        // 若屏幕较窄，高度也按实际宽度重新算一遍
        if (w < 1460) h = qMin(qRound(w * 0.618) + 7, screen.height());
        resize(w, h);
    }

    // ---- 中央容器：状态栏 + 堆叠页面 ----
    auto* centralContainer = new QWidget(this);
    auto* centralLayout    = new QVBoxLayout(centralContainer);
    centralLayout->setContentsMargins(0, 0, 0, 0);
    centralLayout->setSpacing(0);

    // 永久状态栏（始终可见）
    m_statusBar = new GlassNavBar(centralContainer);

    // NextView 标题：低饱和深蓝→蓝渐变，粗体
    auto* appTitleLabel = new GradientLabel(
        "NextView",
        QColor(0, 112, 224),   // #0070E0
        QColor(10, 132, 255),  // #0A84FF
        m_statusBar);
    {
        QFont f = appTitleLabel->font();
        f.setPixelSize(20);
        f.setWeight(QFont::Bold);
        appTitleLabel->setLabelFont(f);
    }
    m_statusBar->addLeftWidget(appTitleLabel);

    // NextView 下方装饰线：直接挂在 m_statusBar 上，不在布局内，宽度随窗口拉伸
    m_titleLine = new QFrame(m_statusBar);
    m_titleLine->setFrameShape(QFrame::HLine);
    m_titleLine->setFixedHeight(1);
    m_titleLine->setStyleSheet("background: rgba(0,0,0,0.1); border: none;");
    m_titleLine->setAttribute(Qt::WA_TransparentForMouseEvents);
    // 初始位置；showEvent/resizeEvent 中会更新宽度
    m_titleLine->setGeometry(0, GlassNavBar::NavBarHeight - 1, width(), 1);

    auto* winBtnContainer = new QWidget(m_statusBar);
    winBtnContainer->setAttribute(Qt::WA_NoSystemBackground);
    auto* winBtnLayout = new QHBoxLayout(winBtnContainer);
    winBtnLayout->setContentsMargins(0, 0, 0, 0);
    winBtnLayout->setSpacing(0);
    auto* wbMin   = new WinButton(WinButton::Minimize, winBtnContainer);
    auto* wbMax   = new WinButton(WinButton::Maximize, winBtnContainer);
    auto* wbClose = new WinButton(WinButton::Close,    winBtnContainer);
    winBtnLayout->addWidget(wbMin);
    winBtnLayout->addWidget(wbMax);
    winBtnLayout->addWidget(wbClose);
    connect(wbClose, &WinButton::clicked, this, &MainWindow::close);
    connect(wbMin,   &WinButton::clicked, this, &MainWindow::showMinimized);
    connect(wbMax,   &WinButton::clicked, this, [this]() {
        isMaximized() ? showNormal() : showMaximized();
    });
    m_statusBar->addRightWidget(winBtnContainer);
    m_statusBar->setDraggable(true);  // 仅状态栏允许拖动窗口
    centralLayout->addWidget(m_statusBar);

    m_stackedWidget = new QStackedWidget(centralContainer);
    centralLayout->addWidget(m_stackedWidget);

    setCentralWidget(centralContainer);

    // ---- 窗口边缘缩放手柄（8 方向，覆盖在最上层） ----
    using E = Qt::Edges;
    m_resizeHandles[0] = new ResizeHandle(E(Qt::TopEdge),                       centralContainer);
    m_resizeHandles[1] = new ResizeHandle(E(Qt::BottomEdge),                    centralContainer);
    m_resizeHandles[2] = new ResizeHandle(E(Qt::LeftEdge),                      centralContainer);
    m_resizeHandles[3] = new ResizeHandle(E(Qt::RightEdge),                     centralContainer);
    m_resizeHandles[4] = new ResizeHandle(E(Qt::TopEdge)    | Qt::LeftEdge,     centralContainer);
    m_resizeHandles[5] = new ResizeHandle(E(Qt::TopEdge)    | Qt::RightEdge,    centralContainer);
    m_resizeHandles[6] = new ResizeHandle(E(Qt::BottomEdge) | Qt::LeftEdge,     centralContainer);
    m_resizeHandles[7] = new ResizeHandle(E(Qt::BottomEdge) | Qt::RightEdge,    centralContainer);
    for (auto* h : m_resizeHandles) h->raise();

    m_projectListPage = new ProjectListPage(this);  // index 0
    m_browsePage      = new BrowsePage(this);        // index 1
    m_settingsPanel   = new SettingsPanel(this);     // index 2
    m_stackedWidget->addWidget(m_projectListPage);
    m_stackedWidget->addWidget(m_browsePage);
    m_stackedWidget->addWidget(m_settingsPanel);
    m_stackedWidget->setCurrentIndex(0);

    // ---- 媒体查看器 ----
    m_imageViewer = new ImageViewer(this);
    m_videoPlayer = new VideoPlayerDialog(this);

    // ---- 信号连接 ----
    connect(m_projectListPage, &ProjectListPage::addProjectRequested,
            this, &MainWindow::onAddProjectRequested);
    connect(m_projectListPage, &ProjectListPage::projectClicked,
            this, &MainWindow::onProjectClicked);
    connect(m_projectListPage, &ProjectListPage::settingsRequested,
            this, &MainWindow::onSettingsRequested);

    connect(m_browsePage, &BrowsePage::backRequested,
            this, &MainWindow::onBackRequested);
    connect(m_browsePage, &BrowsePage::settingsRequested,
            this, &MainWindow::onSettingsRequested);
    connect(m_browsePage, &BrowsePage::mediaItemClicked,
            this, &MainWindow::onMediaItemClicked);

    connect(m_settingsPanel, &SettingsPanel::appearanceChanged,
            this, &MainWindow::onAppearanceChanged);
    connect(m_settingsPanel, &SettingsPanel::qualityChanged,
            this, &MainWindow::onQualityChanged);
    connect(m_settingsPanel, &SettingsPanel::backRequested, this, [this]() {
        m_stackedWidget->setCurrentIndex(m_prevPageIndex);
    });
    connect(m_settingsPanel, &SettingsPanel::activationChanged, this, [this]() {
        // 解除所有视频的锁定遮罩
        m_browsePage->waterfallWidget()->markLockedVideos();
        // 重启当前项目缓存，补全此前因限额跳过的视频预览
        if (!m_cachingProjectId.isEmpty() && m_projectFiles.contains(m_cachingProjectId)) {
            asyncStopCacheWorker();
            startCaching(m_cachingProjectId, m_projectFiles[m_cachingProjectId]);
        }
    });

    connect(ProjectManager::instance(), &ProjectManager::projectAdded,
            m_projectListPage, &ProjectListPage::addProjectCard);
    connect(ProjectManager::instance(), &ProjectManager::projectUpdated,
            m_projectListPage, &ProjectListPage::updateProjectCard);
    connect(ProjectManager::instance(), &ProjectManager::projectRemoved,
            m_projectListPage, &ProjectListPage::removeProjectCard);
    // 删除项目时停止该项目的所有后台任务并清理缓存
    connect(ProjectManager::instance(), &ProjectManager::projectRemoved,
            this, &MainWindow::onProjectRemoved);

    // 加载设置
    QSettings settings("NextView", "NextView");
    m_previewQuality = settings.value("preview/quality", "high").toString();
    QString appear = settings.value("appearance/mode", "dark").toString();
    if (appear == "dark") {
        m_darkMode = true;
        applyTheme(true);
    } else {
        applyTheme(false);
    }

    if (!FFmpegHelper::isAvailable())
        qWarning() << "FFmpeg 未找到，视频功能将受限";

    // 窗口控制按钮已统一放入 m_statusBar，各页面导航栏不再重复添加

    // 启动时加载项目卡片封面缩略图
    for (const Project& p : ProjectManager::instance()->projects()) {
        QString thumbPath = p.cardThumbnailPath;

        // 兜底：旧数据没有 cardThumbnailPath 时，从缓存目录取第一张
        if (thumbPath.isEmpty() || !QFile::exists(thumbPath)) {
            QDir cacheDir(CacheManager::projectCacheDir(p.id));
            QStringList thumbs = cacheDir.entryList({"thumb_*.jpg"}, QDir::Files);
            if (!thumbs.isEmpty())
                thumbPath = cacheDir.filePath(thumbs.first());
        }

        if (!thumbPath.isEmpty()) {
            QPixmap px(thumbPath);
            if (!px.isNull())
                m_projectListPage->setProjectThumbnail(p.id, px);
        }
    }

    // 启动遮罩层（最后创建，确保层叠在最上方）
    new SplashOverlay(this);
}

MainWindow::~MainWindow() {
    if (m_scanWorker && m_scanWorker->isRunning()) {
        m_scanWorker->requestStop();
        m_scanWorker->wait(2000);
    }
    if (m_cacheWorker) {
        m_cacheWorker->cancel();
        m_cacheWorker = nullptr;
    }
    if (m_cacheThread && m_cacheThread->isRunning()) {
        m_cacheThread->quit();
        m_cacheThread->wait(2000);
    }
}

void MainWindow::applyTheme(bool dark) {
    m_darkMode = dark;

    QString btnBase = dark
        ? "QPushButton { border-radius: 10px; padding: 6px 14px; font-size: 13px; font-weight: bold; "
          "color: #F2F2F7; background: rgba(255,255,255,0.1); border: 1px solid #636366; }"
          "QPushButton:hover { background: rgba(255,255,255,0.18); }"
          "QPushButton:pressed { background: rgba(255,255,255,0.08); }"
        : "QPushButton { border-radius: 10px; padding: 6px 14px; font-size: 13px; font-weight: bold; "
          "color: #1C1C1E; background: rgba(0,0,0,0.04); border: 1px solid rgba(0,0,0,0.12); }"
          "QPushButton:hover { background: rgba(0,0,0,0.08); }"
          "QPushButton:pressed { background: rgba(0,0,0,0.04); }";

    m_statusBar->setDarkMode(dark);
    if (m_titleLine)
        m_titleLine->setStyleSheet(dark
            ? "background: rgba(255,255,255,0.1); border: none;"
            : "background: rgba(0,0,0,0.1); border: none;");

    if (dark) {
        qApp->setStyleSheet(QString(R"(
            QMainWindow { background: transparent; }
            QWidget { background: #1C1C1E; color: #F2F2F7; }
            QScrollArea { background: transparent; border: none; }
            QScrollArea > QWidget > QWidget { background: transparent; }
            QScrollBar:vertical { background: transparent; width: 6px; }
            QScrollBar::handle:vertical { background: rgba(255,255,255,0.25); border-radius: 3px; }
            QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
            QLabel { color: #F2F2F7; background: transparent; }
            QRadioButton { color: #F2F2F7; background: transparent; }
            QGroupBox { color: #F2F2F7; }
            QProgressBar { background: rgba(255,255,255,0.15); border-radius: 3px; border: none; }
            QProgressBar::chunk { background: #0A84FF; border-radius: 3px; }
        )") + btnBase);
    } else {
        qApp->setStyleSheet(QString(R"(
            QMainWindow { background: transparent; }
            QWidget { background: #F2F2F7; color: #1C1C1E; }
            QScrollArea { border: none; background: transparent; }
            QScrollArea > QWidget > QWidget { background: transparent; }
            QScrollBar:vertical { background: transparent; width: 6px; }
            QScrollBar::handle:vertical { background: rgba(0,0,0,0.18); border-radius: 3px; }
            QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
            QLabel { background: transparent; }
            QProgressBar { background: rgba(0,0,0,0.1); border-radius: 3px; border: none; }
            QProgressBar::chunk { background: #007AFF; border-radius: 3px; }
        )") + btnBase);
    }

    m_projectListPage->setDarkMode(dark);
    m_browsePage->setDarkMode(dark);
    m_settingsPanel->setDarkMode(dark);
}

void MainWindow::onAddProjectRequested() {
    QString folderPath = QFileDialog::getExistingDirectory(
        this, "选择项目文件夹", QDir::homePath(),
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks
    );
    if (folderPath.isEmpty()) return;

    QString projectId = ProjectManager::instance()->addProject(folderPath);
    if (projectId.isEmpty()) {
        QMessageBox::warning(this, "添加失败", "无法添加该文件夹，可能已存在相同路径的项目。");
        return;
    }
    startScan(projectId);
}

void MainWindow::onProjectClicked(const QString& projectId) {
    Project* project = ProjectManager::instance()->projectById(projectId);
    if (!project) return;

    m_currentProjectId = projectId;
    QList<MediaFile> files = m_projectFiles.value(projectId);

    // loadProject 不影响卡片 UI，提前执行让数据在动画期间就绪
    m_browsePage->loadProject(*project, files);

    // 先截图（卡片仍处于正常状态），再启动扫描，避免"扫描中"被截进快照
    startZoomTransition([this, projectId]() {
        m_stackedWidget->setCurrentIndex(1);
        startScan(projectId);
    });
}

void MainWindow::startZoomTransition(std::function<void()> onDone) {
    if (m_zoomOverlay) {
        m_zoomOverlay->deleteLater();
        m_zoomOverlay = nullptr;
    }

    // 内容区矩形（stacked widget 在 MainWindow 坐标系内的位置）
    QRect contentRect(m_stackedWidget->mapTo(this, QPoint(0, 0)), m_stackedWidget->size());

    // 实时截取内容区当前画面（从 DWM 缓冲区 BitBlt，无重绘开销）
    QPixmap snapshot = QGuiApplication::primaryScreen()->grabWindow(
        winId(), contentRect.x(), contentRect.y(),
        contentRect.width(), contentRect.height());

    // 目标背景色取 browsePage 调色板，自动适配深色/浅色模式
    QColor bgColor = m_browsePage->palette().color(QPalette::Window);

    m_zoomOverlay = new ZoomTransitionOverlay(this, snapshot, bgColor, contentRect,
        [this, onDone]() {
            onDone();
            if (m_zoomOverlay) {
                m_zoomOverlay->deleteLater();
                m_zoomOverlay = nullptr;
            }
        }
    );
}

void MainWindow::onBackRequested() {
    m_stackedWidget->setCurrentIndex(0);
}

void MainWindow::onSettingsRequested() {
    m_prevPageIndex = m_stackedWidget->currentIndex();
    m_settingsPanel->refreshCacheInfo();
    m_stackedWidget->setCurrentIndex(2);
}

void MainWindow::onMediaItemClicked(const MediaFile& file, const QList<MediaFile>&) {
    QDesktopServices::openUrl(QUrl::fromLocalFile(file.path));
}

void MainWindow::startScan(const QString& projectId) {
    Project* project = ProjectManager::instance()->projectById(projectId);
    if (!project) return;

    if (m_scanWorker && m_scanWorker->isRunning()) {
        m_scanWorker->requestStop();
        m_scanWorker->wait(2000);
        delete m_scanWorker;
        m_scanWorker = nullptr;
    }

    project->scanState = Project::Scanning;
    project->scanFound = 0;
    ProjectManager::instance()->updateProject(*project);
    m_scanningProjectId = projectId;

    m_scanWorker = new ScanWorker(this);
    m_scanWorker->setProject(projectId, project->folderPath);
    connect(m_scanWorker, &ScanWorker::fileFound,       this, &MainWindow::onScanFileFound);
    connect(m_scanWorker, &ScanWorker::progressUpdated, this, &MainWindow::onScanProgress);
    connect(m_scanWorker, &ScanWorker::scanComplete,    this, &MainWindow::onScanComplete);
    connect(m_scanWorker, &ScanWorker::scanError, this, [this](const QString& msg) {
        qWarning() << "扫描失败：" << msg;
        if (m_scanningProjectId.isEmpty()) return;
        Project* p = ProjectManager::instance()->projectById(m_scanningProjectId);
        if (p) {
            p->scanState = Project::NotScanned;
            p->scanFound = 0;
            ProjectManager::instance()->updateProject(*p);
        }
        m_scanningProjectId.clear();
    });
    m_scanWorker->start(QThread::LowPriority);
}

void MainWindow::onScanFileFound(const MediaFile& file) { Q_UNUSED(file) }

void MainWindow::onScanProgress(int found) {
    if (m_scanningProjectId.isEmpty()) return;
    Project* p = ProjectManager::instance()->projectById(m_scanningProjectId);
    if (p && p->scanState == Project::Scanning) {
        p->scanFound = found;
        ProjectManager::instance()->updateProject(*p);
    }
}

void MainWindow::onScanComplete(const QList<MediaFile>& files) {
    QString projectId = m_scanningProjectId;
    if (projectId.isEmpty()) return;

    QList<MediaFile> enrichedFiles = files;
    for (MediaFile& f : enrichedFiles) {
        QString thumbPath = CacheManager::thumbnailPath(projectId, f.path);
        if (QFile::exists(thumbPath)) f.thumbnailPath = thumbPath;
        if (f.type == MediaFile::Video) {
            QString prevPath = CacheManager::previewPath(projectId, f.path);
            if (QFile::exists(prevPath)) f.previewPath = prevPath;
        }
    }

    m_projectFiles[projectId] = enrichedFiles;

    // 更新首页搜索索引；同时从后台索引队列移除（避免重复）
    m_indexQueue.removeAll(projectId);
    if (Project* pr = ProjectManager::instance()->projectById(projectId))
        m_projectListPage->addSearchableFiles(pr->folderPath, enrichedFiles);

    // 找第一张有效缩略图作为卡片封面
    QString firstThumbPath;
    for (const MediaFile& f : enrichedFiles) {
        if (!f.thumbnailPath.isEmpty()) {
            QPixmap px(f.thumbnailPath);
            if (!px.isNull()) {
                firstThumbPath = f.thumbnailPath;
                m_projectListPage->setProjectThumbnail(projectId, px);
                break;
            }
        }
    }

    int imageCount = 0, videoCount = 0;
    qint64 totalSize = 0;
    // 同时统计需要缓存的文件，仅将未缓存文件交给 startCaching
    QList<MediaFile> toCache;
    for (const MediaFile& f : enrichedFiles) {
        if (f.type == MediaFile::Image) imageCount++; else videoCount++;
        totalSize += f.size;

        bool needThumb   = f.thumbnailPath.isEmpty();
        bool needPreview = (f.type == MediaFile::Video) && f.previewPath.isEmpty();
        if (needThumb || needPreview)
            toCache.append(f);
    }

    Project* project = ProjectManager::instance()->projectById(projectId);
    if (project) {
        project->scanState  = Project::Scanned;
        project->imageCount = imageCount;
        project->videoCount = videoCount;
        project->totalSize  = totalSize;
        project->scanFound  = enrichedFiles.size();
        // 只有真正有未缓存文件时才重置状态；否则保持 Generated
        project->cacheState = toCache.isEmpty() ? Project::Generated : Project::NoneGenerated;
        if (!firstThumbPath.isEmpty())
            project->cardThumbnailPath = firstThumbPath;
        ProjectManager::instance()->updateProject(*project);
    }

    if (m_currentProjectId == projectId && m_stackedWidget->currentIndex() == 1)
        m_browsePage->loadProject(*ProjectManager::instance()->projectById(projectId), enrichedFiles);

    if (!toCache.isEmpty())
        startCaching(projectId, toCache);
}

void MainWindow::startCaching(const QString& projectId, const QList<MediaFile>& files) {
    if (files.isEmpty()) return;

    asyncStopCacheWorker();

    Project* project = ProjectManager::instance()->projectById(projectId);
    if (project) {
        project->cacheState    = Project::Generating;
        project->cacheProgress = 0;
        project->cacheTotal    = files.size();
        ProjectManager::instance()->updateProject(*project);
    }

    m_cachingProjectId = projectId;
    m_cacheThread = new QThread(this);
    m_cacheWorker = new CacheWorker();
    m_cacheWorker->moveToThread(m_cacheThread);

    connect(m_cacheWorker, &CacheWorker::thumbnailReady, this, &MainWindow::onThumbnailReady);
    connect(m_cacheWorker, &CacheWorker::previewReady,   this, &MainWindow::onPreviewReady);
    connect(m_cacheWorker, &CacheWorker::progressUpdated,this, &MainWindow::onCacheProgress);
    connect(m_cacheWorker, &CacheWorker::allComplete,    this, &MainWindow::onCacheComplete);

    m_cacheThread->start(QThread::LowPriority);

    CacheWorker* worker  = m_cacheWorker;
    QString      quality = m_previewQuality;
    QMetaObject::invokeMethod(m_cacheWorker, [worker, projectId, files, quality]() {
        worker->startCaching(projectId, files, quality);
    }, Qt::QueuedConnection);
}

void MainWindow::onThumbnailReady(const QString& filePath, const QString& thumbnailPath) {
    QString ownerProjectId;
    bool isFirstThumb = false;
    for (auto it = m_projectFiles.begin(); it != m_projectFiles.end(); ++it) {
        bool alreadyHasThumb = false;
        for (const MediaFile& f : it.value())
            if (!f.thumbnailPath.isEmpty()) { alreadyHasThumb = true; break; }
        for (MediaFile& f : it.value()) {
            if (f.path == filePath) {
                f.thumbnailPath = thumbnailPath;
                ownerProjectId  = it.key();
                if (!alreadyHasThumb) isFirstThumb = true;
                break;
            }
        }
        if (!ownerProjectId.isEmpty()) break;
    }

    if (isFirstThumb && !ownerProjectId.isEmpty()) {
        QPixmap px(thumbnailPath);
        if (!px.isNull()) {
            m_projectListPage->setProjectThumbnail(ownerProjectId, px);
            Project* p = ProjectManager::instance()->projectById(ownerProjectId);
            if (p && p->cardThumbnailPath.isEmpty()) {
                p->cardThumbnailPath = thumbnailPath;
                ProjectManager::instance()->updateProject(*p);
            }
        }
    }

    if (m_stackedWidget->currentIndex() == 1)
        m_browsePage->updateThumbnail(filePath, thumbnailPath);
}

void MainWindow::onPreviewReady(const QString& filePath, const QString& previewPath) {
    for (auto& fileList : m_projectFiles)
        for (MediaFile& f : fileList)
            if (f.path == filePath) { f.previewPath = previewPath; break; }

    if (m_stackedWidget->currentIndex() == 1)
        m_browsePage->updatePreview(filePath, previewPath);
}

void MainWindow::onCacheProgress(int completed, int total) {
    if (m_cachingProjectId.isEmpty()) return;
    Project* p = ProjectManager::instance()->projectById(m_cachingProjectId);
    if (p && p->cacheState == Project::Generating) {
        p->cacheProgress = completed;
        p->cacheTotal    = total;
        ProjectManager::instance()->updateProject(*p);
    }
}

void MainWindow::onCacheComplete() {
    if (!m_cachingProjectId.isEmpty()) {
        Project* p = ProjectManager::instance()->projectById(m_cachingProjectId);
        if (p) {
            p->cacheState = Project::Generated;
            p->cacheSize  = CacheManager::calculateCacheSize(p->id);
            ProjectManager::instance()->updateProject(*p);
        }
        m_cachingProjectId.clear();
    }
}

void MainWindow::onAppearanceChanged(bool isDark) { applyTheme(isDark); }
void MainWindow::onQualityChanged(const QString& quality) { m_previewQuality = quality; }
QString MainWindow::currentQuality() const { return m_previewQuality; }

void MainWindow::onProjectRemoved(const QString& projectId) {
    // 停止该项目正在进行的扫描
    if (m_scanningProjectId == projectId) {
        if (m_scanWorker && m_scanWorker->isRunning()) {
            m_scanWorker->requestStop();
            m_scanWorker->wait(500);
            delete m_scanWorker;
            m_scanWorker = nullptr;
        }
        m_scanningProjectId.clear();
    }
    // 停止该项目正在进行的缓存生成
    if (m_cachingProjectId == projectId) {
        asyncStopCacheWorker();
        m_cachingProjectId.clear();
    }
    // 删除缓存目录
    QDir(CacheManager::projectCacheDir(projectId)).removeRecursively();
    // 清理内存
    m_projectFiles.remove(projectId);
}

void MainWindow::asyncStopCacheWorker() {
    if (!m_cacheWorker) return;

    CacheWorker* worker = m_cacheWorker;
    QThread*     thread = m_cacheThread;
    m_cacheWorker = nullptr;
    m_cacheThread = nullptr;

    // 断开所有信号，防止后台线程清理期间投递陈旧回调到主线程
    disconnect(worker, nullptr, this, nullptr);
    // 设置取消标志：FFmpeg 进程在 ≤300ms 内被 kill，信号量等待 ≤500ms 内退出
    worker->cancel();

    // CacheWorker 析构函数含 waitForDone(5000)，会阻塞调用线程
    // 改为在独立后台线程执行，主线程立即返回
    QThread* cleanup = QThread::create([worker, thread]() {
        if (thread && thread->isRunning()) {
            thread->quit();
            thread->wait(3000);
        }
        delete worker;  // 析构内的 waitForDone 在此线程执行，不阻塞主线程
        delete thread;
    });
    cleanup->start();
    connect(cleanup, &QThread::finished, cleanup, &QThread::deleteLater);
}

// ============================================================
// 窗口外观：无边框 + 圆角 + 自定义窗口控件
// ============================================================

void MainWindow::applyRoundedCorners() {
    // Windows 系统下取消圆角，此函数保留以供 changeEvent / showEvent 调用
    clearMask();
}

void MainWindow::showEvent(QShowEvent* event) {
    QMainWindow::showEvent(event);
#ifdef Q_OS_WIN
    // 还原 WS_THICKFRAME（Qt::FramelessWindowHint 会移除它），
    // 让 Windows 的原生拖拽缩放仍可通过 WM_NCHITTEST 响应
    HWND hwnd = reinterpret_cast<HWND>(winId());
    LONG style = GetWindowLong(hwnd, GWL_STYLE);
    style |= WS_THICKFRAME | WS_CAPTION | WS_MAXIMIZEBOX | WS_MINIMIZEBOX;
    SetWindowLong(hwnd, GWL_STYLE, style);
    SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
#endif
    applyRoundedCorners();
    // 窗口显示后强制更新装饰线宽度（初次布局完成后 statusBar 才有正确宽度）
    if (m_titleLine)
        m_titleLine->setGeometry(0, GlassNavBar::NavBarHeight - 1, width(), 1);

    // 将所有已扫描但尚未建立搜索索引的项目加入后台索引队列
    for (const Project& p : ProjectManager::instance()->projects()) {
        if (p.scanState == Project::Scanned && !m_projectFiles.contains(p.id))
            m_indexQueue.append(p.id);
    }
    indexNextProject();
}

void MainWindow::resizeEvent(QResizeEvent* event) {
    QMainWindow::resizeEvent(event);
    applyRoundedCorners();

    // 更新装饰线宽度（随窗口宽度拉伸）
    if (m_titleLine)
        m_titleLine->setGeometry(0, GlassNavBar::NavBarHeight - 1, width(), 1);

    // 更新缩放手柄位置（最大化时隐藏）
    const int b = 6, w = width(), h = height();
#ifdef Q_OS_WIN
    const bool maximized = m_nativeMaximized;
#else
    const bool maximized = isMaximized();
#endif
    if (maximized) {
        for (auto* handle : m_resizeHandles) handle->hide();
    } else {
        for (auto* handle : m_resizeHandles) { handle->show(); handle->raise(); }
        m_resizeHandles[0]->setGeometry(b,   0,   w-2*b, b);   // top
        m_resizeHandles[1]->setGeometry(b,   h-b, w-2*b, b);   // bottom
        m_resizeHandles[2]->setGeometry(0,   b,   b, h-2*b);   // left
        m_resizeHandles[3]->setGeometry(w-b, b,   b, h-2*b);   // right
        m_resizeHandles[4]->setGeometry(0,   0,   b, b);       // top-left
        m_resizeHandles[5]->setGeometry(w-b, 0,   b, b);       // top-right
        m_resizeHandles[6]->setGeometry(0,   h-b, b, b);       // bottom-left
        m_resizeHandles[7]->setGeometry(w-b, h-b, b, b);       // bottom-right
    }
}

void MainWindow::changeEvent(QEvent* event) {
    QMainWindow::changeEvent(event);
    if (event->type() == QEvent::WindowStateChange) {
        applyRoundedCorners();
        update();  // 触发 WinButton 重绘（最大化图标切换）
    }
}

#ifdef Q_OS_WIN
bool MainWindow::nativeEvent(const QByteArray& eventType, void* message, qintptr* result) {
    if (eventType == "windows_generic_MSG") {
        MSG* msg = reinterpret_cast<MSG*>(message);

        // 让客户区覆盖整个窗口（去除系统绘制的标题栏与边框留白）
        if (msg->message == WM_NCCALCSIZE && msg->wParam == TRUE) {
            auto* params = reinterpret_cast<NCCALCSIZE_PARAMS*>(msg->lParam);
            HMONITOR mon = MonitorFromWindow(msg->hwnd, MONITOR_DEFAULTTONEAREST);
            MONITORINFO mi = {};
            mi.cbSize = sizeof(mi);
            GetMonitorInfo(mon, &mi);
            // 用提议矩形与工作区比较判断最大化，避免依赖 isMaximized()（Aero Snap
            // 还原时 Qt 状态滞后，会把还原后的小窗口错误地裁剪到工作区大小）
            const RECT& r = params->rgrc[0];
            bool isMax = (r.left  <= mi.rcWork.left  &&
                          r.top   <= mi.rcWork.top   &&
                          r.right >= mi.rcWork.right &&
                          r.bottom >= mi.rcWork.bottom);
            if (isMax)
                params->rgrc[0] = mi.rcWork;  // 最大化：限定到工作区，不遮挡任务栏
            *result = 0;
            return true;
        }

        if (msg->message == WM_ERASEBKGND) {
            // 阻止系统用白色擦除背景，避免拖拽调整窗口大小时新区域闪白
            *result = 1;
            return true;
        }

        if (msg->message == WM_SIZE) {
            m_nativeMaximized = (msg->wParam == SIZE_MAXIMIZED);
        }

        if (msg->message == WM_NCHITTEST) {
            // 缩放由 ResizeHandle 子控件通过 startSystemResize 处理；
            // 拖拽由 GlassNavBar::mousePressEvent + startSystemMove 处理；
            // 此处统一返回 HTCLIENT，让 Qt 正常分发所有鼠标事件
            *result = HTCLIENT;
            return true;
        }
    }
    return QMainWindow::nativeEvent(eventType, message, result);
}
#endif

void MainWindow::indexNextProject() {
    if (m_indexQueue.isEmpty()) return;

    // 若当前正在索引，等其完成后自然触发下一个
    if (m_indexWorker && m_indexWorker->isRunning()) return;

    QString projectId = m_indexQueue.takeFirst();

    // 若该项目在本次会话中已通过正常扫描建立了索引，跳过
    if (m_projectFiles.contains(projectId)) {
        indexNextProject();
        return;
    }

    Project* project = ProjectManager::instance()->projectById(projectId);
    if (!project) { indexNextProject(); return; }

    m_indexWorker = new ScanWorker(this);
    m_indexWorker->setProject(projectId, project->folderPath);

    connect(m_indexWorker, &ScanWorker::scanComplete,
            this, [this, projectId](const QList<MediaFile>& files) {
        onIndexComplete(files, projectId);
    });
    m_indexWorker->start(QThread::IdlePriority);  // 最低优先级，不影响前台操作
}

void MainWindow::onIndexComplete(const QList<MediaFile>& files, const QString& projectId) {
    Project* project = ProjectManager::instance()->projectById(projectId);
    if (!project) { indexNextProject(); return; }

    // 补充缓存路径信息
    QList<MediaFile> enriched = files;
    for (MediaFile& f : enriched) {
        QString thumbPath = CacheManager::thumbnailPath(projectId, f.path);
        if (QFile::exists(thumbPath)) f.thumbnailPath = thumbPath;
        if (f.type == MediaFile::Video) {
            QString prevPath = CacheManager::previewPath(projectId, f.path);
            if (QFile::exists(prevPath)) f.previewPath = prevPath;
        }
    }

    // 填充项目文件列表与搜索索引（仅在尚未由正常扫描覆盖时）
    if (!m_projectFiles.contains(projectId))
        m_projectFiles[projectId] = enriched;
    m_projectListPage->addSearchableFiles(project->folderPath, enriched);

    if (m_indexWorker) {
        m_indexWorker->deleteLater();
        m_indexWorker = nullptr;
    }
    indexNextProject();  // 继续处理队列中的下一个项目
}

void MainWindow::closeEvent(QCloseEvent* event) {
    if (m_scanWorker && m_scanWorker->isRunning()) {
        m_scanWorker->requestStop();
        m_scanWorker->wait(2000);
    }
    if (m_indexWorker && m_indexWorker->isRunning()) {
        m_indexWorker->requestStop();
        m_indexWorker->wait(2000);
    }
    if (m_cacheWorker) m_cacheWorker->cancel();
    if (m_cacheThread && m_cacheThread->isRunning()) {
        m_cacheThread->quit();
        m_cacheThread->wait(2000);
    }
    QMainWindow::closeEvent(event);
}
