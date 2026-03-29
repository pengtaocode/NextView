#include "BrowsePage.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>
#include <QResizeEvent>
#include <QScrollBar>
#include <QFont>
#include <QDebug>

BrowsePage::BrowsePage(QWidget* parent)
    : QWidget(parent)
    , m_darkMode(false)
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // ---- 滚动区域（主内容） ----
    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_scrollArea->setFrameShape(QFrame::NoFrame);
    m_scrollArea->setStyleSheet("QScrollArea { background: transparent; border: none; }");

    // 瀑布流控件（内容顶部留出导航栏高度）
    QWidget* scrollContent = new QWidget();
    QVBoxLayout* scrollLayout = new QVBoxLayout(scrollContent);
    scrollLayout->setContentsMargins(0, GlassNavBar::NavBarHeight, 0, 0);
    scrollLayout->setSpacing(0);

    m_waterfallWidget = new WaterfallWidget(scrollContent);
    scrollLayout->addWidget(m_waterfallWidget);
    scrollLayout->addStretch();

    m_scrollArea->setWidget(scrollContent);

    mainLayout->addWidget(m_scrollArea);

    // ---- 导航栏（覆盖在滚动区域上方） ----
    m_navBar = new GlassNavBar(this);
    m_navBar->setContentSource(m_scrollArea->widget());

    // 左侧：返回按钮
    QPushButton* backBtn = new QPushButton("返回", this);
    backBtn->setFlat(true);
    backBtn->setCursor(Qt::PointingHandCursor);
    backBtn->setStyleSheet(
        "QPushButton { font-size: 13px; font-weight: bold; background: white; border: none; "
        "color: #1C1C1E; padding: 5px 13px; border-radius: 14px; }"
        "QPushButton:hover { background: #E0E0E0; }"
    );
    connect(backBtn, &QPushButton::clicked, this, &BrowsePage::backRequested);
    m_navBar->addLeftWidget(backBtn);

    // 右侧：过滤按钮组 + 设置按钮
    QWidget* filterWidget = createFilterButtons();
    m_navBar->addRightWidget(filterWidget);

    // 设置按钮已按需求移除（设置入口保留在首页导航栏）

    // 连接瀑布流的点击信号
    connect(m_waterfallWidget, &WaterfallWidget::itemClicked,
            this, [this](const MediaFile& file) {
        emit mediaItemClicked(file, m_currentFiles);
    });
}

QWidget* BrowsePage::createFilterButtons() {
    QWidget* container = new QWidget(this);
    QHBoxLayout* layout = new QHBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);

    m_filterAll    = new QPushButton("全部", container);
    m_filterImages = new QPushButton("图片", container);
    m_filterVideos = new QPushButton("视频", container);

    m_filterAll->setCursor(Qt::PointingHandCursor);
    m_filterImages->setCursor(Qt::PointingHandCursor);
    m_filterVideos->setCursor(Qt::PointingHandCursor);

    layout->addWidget(m_filterAll);
    layout->addWidget(m_filterImages);
    layout->addWidget(m_filterVideos);

    // 连接过滤按钮信号
    connect(m_filterAll, &QPushButton::clicked, this, [this]() {
        m_waterfallWidget->setFilter(WaterfallWidget::All);
        updateFilterButtonStyles(WaterfallWidget::All);
    });
    connect(m_filterImages, &QPushButton::clicked, this, [this]() {
        m_waterfallWidget->setFilter(WaterfallWidget::ImagesOnly);
        updateFilterButtonStyles(WaterfallWidget::ImagesOnly);
    });
    connect(m_filterVideos, &QPushButton::clicked, this, [this]() {
        m_waterfallWidget->setFilter(WaterfallWidget::VideosOnly);
        updateFilterButtonStyles(WaterfallWidget::VideosOnly);
    });

    // 默认选中"全部"
    updateFilterButtonStyles(WaterfallWidget::All);

    return container;
}

void BrowsePage::updateFilterButtonStyles(WaterfallWidget::FilterType active) {
    // 激活态：深灰填充
    QString activeStyle =
        "QPushButton { border-radius: 12px; padding: 4px 12px; font-size: 12px; font-weight: bold; border: none;"
        "  background: #0A84FF;"
        "  color: white; }";
    // 非激活态：白底无边框
    QString inactiveStyle =
        "QPushButton { border-radius: 12px; padding: 4px 12px; font-size: 12px; font-weight: bold; border: none;"
        "  background: white; color: #1C1C1E; }"
        "QPushButton:hover { background: #E0E0E0; }";

    m_filterAll->setStyleSheet(active == WaterfallWidget::All ? activeStyle : inactiveStyle);
    m_filterImages->setStyleSheet(active == WaterfallWidget::ImagesOnly ? activeStyle : inactiveStyle);
    m_filterVideos->setStyleSheet(active == WaterfallWidget::VideosOnly ? activeStyle : inactiveStyle);
}

void BrowsePage::loadProject(const Project& project, const QList<MediaFile>& files) {
    m_currentProject = project;
    m_currentFiles = files;

    // 设置导航栏标题
    m_navBar->setTitle(project.name);

    // 重置过滤状态
    m_waterfallWidget->setFilter(WaterfallWidget::All);
    updateFilterButtonStyles(WaterfallWidget::All);

    // 加载文件到瀑布流
    m_waterfallWidget->setFiles(files);

    m_scrollArea->verticalScrollBar()->setValue(0);

    qDebug() << "浏览页已加载项目：" << project.name
             << "，共" << files.size() << "个文件";
}

void BrowsePage::updateThumbnail(const QString& filePath, const QString& thumbnailPath) {
    m_waterfallWidget->updateThumbnail(filePath, thumbnailPath);

    // 更新 m_currentFiles 中的缩略图路径
    for (MediaFile& f : m_currentFiles) {
        if (f.path == filePath) {
            f.thumbnailPath = thumbnailPath;
            break;
        }
    }
}

void BrowsePage::updatePreview(const QString& filePath, const QString& previewPath) {
    m_waterfallWidget->updatePreview(filePath, previewPath);

    // 更新 m_currentFiles 中的预览路径
    for (MediaFile& f : m_currentFiles) {
        if (f.path == filePath) {
            f.previewPath = previewPath;
            break;
        }
    }
}

void BrowsePage::setDarkMode(bool dark) {
    m_darkMode = dark;

    // QScrollArea 的 viewport 是 Qt 内部原生控件，全局 stylesheet 覆盖时机不稳定，
    // 用 palette 显式设置背景色，防止深色模式下首次渲染时闪白
    QColor bg = dark ? QColor(0x1C, 0x1C, 0x1E) : QColor(0xF2, 0xF2, 0xF7);
    auto applyBg = [&](QWidget* w) {
        QPalette p = w->palette();
        p.setColor(QPalette::Window, bg);
        p.setColor(QPalette::Base,   bg);
        w->setPalette(p);
        w->setAutoFillBackground(true);
    };
    applyBg(this);
    applyBg(m_scrollArea);
    applyBg(m_scrollArea->viewport());

    m_navBar->setDarkMode(dark);
    m_waterfallWidget->setDarkMode(dark);
    update();
}

void BrowsePage::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    m_navBar->setGeometry(0, 0, width(), GlassNavBar::NavBarHeight);
}
