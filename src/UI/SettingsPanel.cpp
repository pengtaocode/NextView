#include "SettingsPanel.h"
#include "ProjectManager.h"
#include "CacheManager.h"
#include "ConfirmDialog.h"
#include "ActivationDialog.h"
#include "LicenseManager.h"

#include <QPainter>
#include <QResizeEvent>
#include <QScrollArea>
#include <QHBoxLayout>
#include <QButtonGroup>
#include <QAbstractButton>
#include <QFrame>
#include <QFont>
#include <QFontMetrics>
#include <QSettings>
#include <QDebug>

SettingsPanel::SettingsPanel(QWidget* parent)
    : QWidget(parent)
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // ---- 导航栏 ----
    m_navBar = new GlassNavBar(this);
    m_navBar->setTitle("设置");

    QPushButton* backBtn = new QPushButton("返回", this);
    backBtn->setFlat(true);
    backBtn->setCursor(Qt::PointingHandCursor);
    backBtn->setStyleSheet(
        "QPushButton { font-size: 13px; background: white; border: none; "
        "color: #1C1C1E; padding: 5px 13px; border-radius: 14px; }"
        "QPushButton:hover { background: #E0E0E0; }"
    );
    connect(backBtn, &QPushButton::clicked, this, &SettingsPanel::backRequested);
    m_navBar->addLeftWidget(backBtn);

    // ---- 滚动内容区 ----
    QScrollArea* scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setStyleSheet("QScrollArea { background: transparent; border: none; }");

    QWidget* contentWidget = new QWidget();
    QVBoxLayout* contentLayout = new QVBoxLayout(contentWidget);
    contentLayout->setContentsMargins(24, 20, 24, 32);
    contentLayout->setSpacing(16);
    scrollArea->setWidget(contentWidget);

    mainLayout->addWidget(scrollArea);

    // 把导航栏绝对定位在顶部（resizeEvent 中更新）
    m_navBar->setParent(this);
    m_navBar->raise();

    // 内容区顶部留出导航栏高度
    contentLayout->setContentsMargins(24, GlassNavBar::NavBarHeight + 16, 24, 32);

    // 分隔线辅助函数
    auto makeLine = [this]() {
        QFrame* line = new QFrame(this);
        line->setFrameShape(QFrame::HLine);
        line->setStyleSheet("color: rgba(128,128,128,0.2);");
        return line;
    };

    // ---- 激活（第一项）----
    contentLayout->addWidget(createSectionTitle("激活"));

    // 状态行：文字 + 激活按钮（未激活时显示）
    QWidget* activationRow = new QWidget(this);
    QHBoxLayout* activationRowLayout = new QHBoxLayout(activationRow);
    activationRowLayout->setContentsMargins(0, 0, 0, 0);
    activationRowLayout->setSpacing(6);

    m_activationStatusLabel = new QLabel(this);
    m_activationStatusLabel->setWordWrap(false);
    m_activationStatusLabel->setStyleSheet("font-size: 13px;");

    m_activateBtn = new QPushButton("激活", this);
    m_activateBtn->setFlat(true);
    m_activateBtn->setCursor(Qt::PointingHandCursor);
    m_activateBtn->setStyleSheet(
        "QPushButton { background: transparent; border: none; color: #0A84FF;"
        "  font-size: 13px; font-weight: bold; padding: 0; }"
        "QPushButton:hover { color: #3399FF; }");

    connect(m_activateBtn, &QPushButton::clicked, this, [this]() {
        ActivationDialog dlg(m_darkMode, this);
        if (dlg.exec() == QDialog::Accepted) {
            refreshActivationStatus();
            emit activationChanged();
        }
    });

    activationRowLayout->addWidget(m_activationStatusLabel);
    activationRowLayout->addWidget(m_activateBtn);
    activationRowLayout->addStretch();
    contentLayout->addWidget(activationRow);

    refreshActivationStatus();

    contentLayout->addWidget(makeLine());

    // ---- 外观 ----
    contentLayout->addWidget(createSectionTitle("外观"));

    QButtonGroup* appearGroup = new QButtonGroup(this);
    m_appearSystem = new QRadioButton("跟随系统", this);
    m_appearLight  = new QRadioButton("浅色模式", this);
    m_appearDark   = new QRadioButton("深色模式", this);
    appearGroup->addButton(m_appearSystem);
    appearGroup->addButton(m_appearLight);
    appearGroup->addButton(m_appearDark);
    m_appearDark->setChecked(true);

    QVBoxLayout* appearLayout = new QVBoxLayout();
    appearLayout->setSpacing(10);
    appearLayout->addWidget(m_appearSystem);
    appearLayout->addWidget(m_appearLight);
    appearLayout->addWidget(m_appearDark);
    contentLayout->addLayout(appearLayout);

    connect(appearGroup, &QButtonGroup::buttonClicked, this, [this](QAbstractButton*) {
        bool isDark = m_appearDark->isChecked();
        saveSettings();
        emit appearanceChanged(isDark);
    });

    contentLayout->addWidget(makeLine());

    // ---- 预览质量 ----
    contentLayout->addWidget(createSectionTitle("预览质量"));

    QButtonGroup* qualityGroup = new QButtonGroup(this);
    m_qualityLow    = new QRadioButton("低（480p）- 节省空间", this);
    m_qualityMedium = new QRadioButton("中（720p）- 推荐", this);
    m_qualityHigh   = new QRadioButton("高（1080p）- 最佳体验", this);
    qualityGroup->addButton(m_qualityLow);
    qualityGroup->addButton(m_qualityMedium);
    qualityGroup->addButton(m_qualityHigh);
    m_qualityHigh->setChecked(true);

    QVBoxLayout* qualityLayout = new QVBoxLayout();
    qualityLayout->setSpacing(10);
    qualityLayout->addWidget(m_qualityLow);
    qualityLayout->addWidget(m_qualityMedium);
    qualityLayout->addWidget(m_qualityHigh);
    contentLayout->addLayout(qualityLayout);

    connect(qualityGroup, &QButtonGroup::buttonClicked, this, [this](QAbstractButton*) {
        QString q = "medium";
        if (m_qualityLow->isChecked()) q = "low";
        else if (m_qualityHigh->isChecked()) q = "high";
        saveSettings();
        emit qualityChanged(q);
    });

    contentLayout->addWidget(makeLine());

    // ---- 缓存管理 ----
    contentLayout->addWidget(createSectionTitle("缓存管理"));

    m_cacheInfoLabel = new QLabel("正在计算缓存大小...", this);
    m_cacheInfoLabel->setStyleSheet("color: rgba(128,128,128,0.9); font-size: 13px;");
    contentLayout->addWidget(m_cacheInfoLabel);

    m_cacheScroll = new QScrollArea(this);
    m_cacheScroll->setWidgetResizable(true);
    m_cacheScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_cacheScroll->setFrameShape(QFrame::NoFrame);
    m_cacheScroll->setStyleSheet("QScrollArea { background: transparent; }");
    // 高度在 refreshCacheInfo() 里根据实际项目数动态设置，上限 7 项

    QWidget* cacheListWidget = new QWidget();
    m_cacheListLayout = new QVBoxLayout(cacheListWidget);
    m_cacheListLayout->setContentsMargins(0, 0, 0, 0);
    m_cacheListLayout->setSpacing(4);
    m_cacheScroll->setWidget(cacheListWidget);
    contentLayout->addWidget(m_cacheScroll);

    QPushButton* clearAllBtn = new QPushButton("清除所有缓存", this);
    clearAllBtn->setStyleSheet(
        "QPushButton { background: rgba(255,59,48,0.9); color: white; "
        "border-radius: 14px; padding: 9px 18px; border: none; font-size: 13px; font-weight: bold; }"
        "QPushButton:hover { background: rgba(255,59,48,0.6); color: white; }"
    );
    clearAllBtn->setCursor(Qt::PointingHandCursor);
    connect(clearAllBtn, &QPushButton::clicked, this, [this]() {
        ConfirmDialog dlg("清除所有缓存", "取消", "清除", true, m_darkMode, this);
        if (dlg.exec() == QDialog::Accepted) {
            CacheManager::deleteAllCache();
            refreshCacheInfo();
        }
    });
    contentLayout->addWidget(clearAllBtn);

    contentLayout->addWidget(makeLine());

    // ---- 关于 ----
    contentLayout->addWidget(createSectionTitle("关于"));

    m_aboutLabel = new QLabel("© 2026 阿南. All rights reserved.", this);
    m_aboutLabel->setStyleSheet("color: #1C1C1E; font-size: 13px;");
    contentLayout->addWidget(m_aboutLabel);

    contentLayout->addStretch();

    loadSettings();
}

QLabel* SettingsPanel::createSectionTitle(const QString& title) {
    QLabel* label = new QLabel(title, this);
    QFont font = label->font();
    font.setPixelSize(13);
    font.setWeight(QFont::DemiBold);
    label->setFont(font);
    label->setStyleSheet("color: rgba(128,128,128,0.9); letter-spacing: 0.5px;");
    return label;
}

void SettingsPanel::loadSettings() {
    QSettings settings("NextView", "NextView");
    QString quality = settings.value("preview/quality", "high").toString();
    if (quality == "low") m_qualityLow->setChecked(true);
    else if (quality == "high") m_qualityHigh->setChecked(true);
    else m_qualityMedium->setChecked(true);

    QString appear = settings.value("appearance/mode", "dark").toString();
    if (appear == "light") m_appearLight->setChecked(true);
    else if (appear == "dark") m_appearDark->setChecked(true);
    else m_appearSystem->setChecked(true);
}

void SettingsPanel::saveSettings() {
    QSettings settings("NextView", "NextView");
    QString quality = "medium";
    if (m_qualityLow->isChecked()) quality = "low";
    else if (m_qualityHigh->isChecked()) quality = "high";
    settings.setValue("preview/quality", quality);

    QString appear = "system";
    if (m_appearLight->isChecked()) appear = "light";
    else if (m_appearDark->isChecked()) appear = "dark";
    settings.setValue("appearance/mode", appear);
}

void SettingsPanel::refreshCacheInfo() {
    qint64 totalSize = CacheManager::calculateTotalCacheSize();

    auto fmt = [](qint64 bytes) -> QString {
        if (bytes < 1024) return QString("%1 B").arg(bytes);
        if (bytes < 1024 * 1024) return QString("%1 KB").arg(bytes / 1024);
        if (bytes < 1024LL * 1024 * 1024)
            return QString("%1 MB").arg(bytes / (1024.0 * 1024.0), 0, 'f', 1);
        return QString("%1 GB").arg(bytes / (1024.0 * 1024.0 * 1024.0), 0, 'f', 2);
    };

    m_cacheInfoLabel->setText(QString("总缓存：%1").arg(fmt(totalSize)));

    while (QLayoutItem* item = m_cacheListLayout->takeAt(0)) {
        if (item->widget()) item->widget()->deleteLater();
        delete item;
    }

    for (const Project& p : ProjectManager::instance()->projects()) {
        qint64 cacheSize = CacheManager::calculateCacheSize(p.id);

        QWidget* item = new QWidget(this);
        QHBoxLayout* itemLayout = new QHBoxLayout(item);
        itemLayout->setContentsMargins(0, 2, 0, 2);
        itemLayout->setSpacing(8);

        QLabel* nameLabel = new QLabel(p.name, item);
        nameLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        QFontMetrics fm(nameLabel->font());
        nameLabel->setText(fm.elidedText(p.name, Qt::ElideRight, 160));

        QLabel* sizeLabel = new QLabel(fmt(cacheSize), item);
        sizeLabel->setStyleSheet("color: rgba(128,128,128,0.9); font-size: 12px;");

        // 用 QFrame 包装提供圆角，按钮内层透明（Windows 上 QPushButton border-radius 不可靠）
        QFrame* btnWrap = new QFrame(item);
        btnWrap->setObjectName("ClearBtnWrap");
        btnWrap->setAutoFillBackground(true);
        btnWrap->setStyleSheet(
            "#ClearBtnWrap { background: rgba(255,59,48,0.9); border-radius: 13px; border: none; }"
        );
        QHBoxLayout* btnLay = new QHBoxLayout(btnWrap);
        btnLay->setContentsMargins(0, 0, 0, 0);
        btnLay->setSpacing(0);

        QPushButton* clearBtn = new QPushButton("清除", btnWrap);
        clearBtn->setFixedHeight(26);
        clearBtn->setFlat(true);
        clearBtn->setCursor(Qt::PointingHandCursor);
        clearBtn->setStyleSheet(
            "QPushButton { background: transparent; color: white; border: none;"
            "  padding: 2px 10px; font-size: 12px; font-weight: bold; }"
            "QPushButton:hover { background: rgba(0,0,0,0.1); border-radius: 13px; }"
        );
        btnLay->addWidget(clearBtn);

        QString pid = p.id;
        connect(clearBtn, &QPushButton::clicked, this, [this, pid]() {
            CacheManager::deleteProjectCache(pid);
            refreshCacheInfo();
        });

        itemLayout->addWidget(nameLabel);
        itemLayout->addWidget(sizeLabel);
        itemLayout->addWidget(btnWrap);
        m_cacheListLayout->addWidget(item);
    }
    m_cacheListLayout->addStretch();

    // 根据实际项目数动态设高（最多显示 7 项，超出后滚动）
    // 每项 = 按钮高(26) + 上下margin(2+2) = 30px，间距 4px
    int count = qMin(7, m_cacheListLayout->count() - 1);  // -1 排除 addStretch
    int h = count > 0 ? count * 30 + (count - 1) * 4 : 30;
    m_cacheScroll->setFixedHeight(h);
}

void SettingsPanel::setDarkMode(bool dark) {
    m_darkMode = dark;
    m_navBar->setDarkMode(dark);
    m_aboutLabel->setStyleSheet(
        QString("color: %1; font-size: 13px;").arg(dark ? "#FFFFFF" : "#1C1C1E"));
    refreshActivationStatus();
}

void SettingsPanel::refreshActivationStatus() {
    bool activated = LicenseManager::instance()->isActivated();
    if (activated) {
        m_activationStatusLabel->setText("✓ 已激活，视频预览数量无限制");
        m_activationStatusLabel->setStyleSheet("color: #34C759; font-size: 13px;");
        m_activateBtn->hide();
    } else {
        m_activationStatusLabel->setText(
            QString("每个项目仅预览前 %1 个视频")
                .arg(LicenseManager::FreeVideoPreviewLimit));
        m_activationStatusLabel->setStyleSheet(
            QString("color: %1; font-size: 13px;")
                .arg(m_darkMode ? "#FFFFFF" : "#1C1C1E"));
        m_activateBtn->show();
    }
}

void SettingsPanel::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    m_navBar->setGeometry(0, 0, width(), GlassNavBar::NavBarHeight);
}
