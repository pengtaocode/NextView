#include "ProjectListPage.h"
#include "ProjectManager.h"

#include <QVBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QFrame>
#include <QFont>
#include <QScrollBar>
#include <QResizeEvent>
#include <QDebug>
#include <QFile>
#include <QTimer>
#include <QApplication>
#include <QFocusEvent>
#include <QMouseEvent>
#include "AnimUtils.h"
#include <QPainter>
#include <QFontMetrics>
#include <cmath>
#include "ConfirmDialog.h"
#include <functional>
#include <QPainterPath>

// ── 逐字打字机动画控件 ────────────────────────────────────────────────
class TypewriterLabel : public QWidget {
public:
    explicit TypewriterLabel(const QString& text, QWidget* parent = nullptr)
        : QWidget(parent), m_fullText(text), m_visibleCount(0)
        , m_currentX(0.0), m_targetX(0.0)
    {
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setAttribute(Qt::WA_NoSystemBackground);
        setAttribute(Qt::WA_TranslucentBackground);

        m_font.setPixelSize(24);  // 比 NavBar "NextView"（20px）稍大
        m_font.setBold(true);

        // 每 90ms 显示下一个字符
        m_charTimer = new QTimer(this);
        m_charTimer->setInterval(90);
        connect(m_charTimer, &QTimer::timeout, this, [this]() {
            if (m_visibleCount < m_fullText.length()) {
                m_visibleCount++;
                updateTargetX();
            } else {
                m_charTimer->stop();
            }
            update();
        });

        // 平滑插值，适配屏幕刷新率，让文字向左滑动始终居中
        m_smoothTimer = new QTimer(this);
        m_smoothTimer->setInterval(screenAnimInterval());
        connect(m_smoothTimer, &QTimer::timeout, this, [this]() {
            double diff = m_targetX - m_currentX;
            if (std::abs(diff) < 0.3) {
                m_currentX = m_targetX;
                if (!m_charTimer->isActive()) {
                    m_smoothTimer->stop();
                    // 打字 + 滑动全部结束，触发完成回调
                    if (m_onFinished) m_onFinished();
                }
            } else {
                // 系数随帧间隔缩放，保持相同视觉速度（参考 8ms/120fps）
                m_currentX += diff * 0.10 * (m_smoothTimer->interval() / 8.0);
            }
            update();
        });
    }

    void setTextColor(const QColor& c) { m_color = c; update(); }
    void setOnFinished(std::function<void()> cb) { m_onFinished = std::move(cb); }

    void startAnimation() {
        m_visibleCount = 0;
        // 动画从控件水平中心出发，随字符增多向左滑至居中
        m_currentX = width() / 2.0;
        updateTargetX();
        m_charTimer->start();
        m_smoothTimer->start();
        update();
    }

    void stopAnimation() {
        m_charTimer->stop();
        m_smoothTimer->stop();
    }

protected:
    void paintEvent(QPaintEvent*) override {
        if (m_visibleCount == 0) return;
        QPainter p(this);
        p.setRenderHint(QPainter::TextAntialiasing);
        p.setFont(m_font);
        p.setPen(m_color);
        QFontMetrics fm(m_font);
        // 垂直居中
        int y = (height() + fm.ascent() - fm.descent()) / 2;
        p.drawText(QPointF(m_currentX, y), m_fullText.left(m_visibleCount));
    }

    void resizeEvent(QResizeEvent* e) override {
        QWidget::resizeEvent(e);
        updateTargetX();
        m_currentX = m_targetX;
    }

private:
    void updateTargetX() {
        QFontMetrics fm(m_font);
        int textW = fm.horizontalAdvance(m_fullText.left(m_visibleCount));
        m_targetX = (width() - textW) / 2.0;
    }

    QString m_fullText;
    int     m_visibleCount;
    double  m_currentX;
    double  m_targetX;
    QFont   m_font;
    QColor  m_color = QColor(0x1C, 0x1C, 0x1E);
    QTimer* m_charTimer;
    QTimer* m_smoothTimer;
    std::function<void()> m_onFinished;
};

// ── 添加文件夹按钮脉动光晕（胶囊形，空状态引导视线，2次后自动消失）──────
class ButtonGlowOverlay : public QWidget {
    static constexpr float kMaxSpread   = 20.0f;  // 最大向外扩散距离（px）
    static constexpr int   kPad         = 34;     // 控件每边超出按钮的像素
    static constexpr int   kTotalCycles = 7;      // 播放完整周期数后自动隐藏
public:
    explicit ButtonGlowOverlay(QWidget* parent = nullptr)
        : QWidget(parent)
    {
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setAttribute(Qt::WA_NoSystemBackground);
        setAttribute(Qt::WA_TranslucentBackground);

        m_timer = new QTimer(this);
        m_timer->setInterval(screenAnimInterval());
        connect(m_timer, &QTimer::timeout, this, [this]() {
            float newPhase = m_phase + m_timer->interval() / 1800.0f;
            if (newPhase >= 1.0f) {
                m_cyclesDone++;
                if (m_cyclesDone >= kTotalCycles) {
                    m_timer->stop();
                    hide();
                    return;
                }
            }
            m_phase = std::fmod(newPhase, 1.0f);
            update();
        });
    }

    // 根据按钮在父控件中的 rect 重新定位，记录按钮宽高用于胶囊绘制
    void trackButton(const QRect& btnRect) {
        m_btnW = float(btnRect.width());
        m_btnH = float(btnRect.height());
        setGeometry(btnRect.adjusted(-kPad, -kPad, kPad, kPad));
    }

    void start() {
        m_phase      = 0.0f;
        m_cyclesDone = 0;
        m_timer->start();
        show();
        raise();
    }
    void stop() { m_timer->stop(); hide(); }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);

        const QPointF c(width() / 2.0, height() / 2.0);
        static constexpr float kPi = 3.14159265f;

        // 单圈胶囊形扩散光环
        float alpha  = std::sin(m_phase * kPi);
        float spread = m_phase * kMaxSpread;

        float rw = m_btnW + 2.0f * spread;
        float rh = m_btnH + 2.0f * spread;
        float cr = qMin(m_btnH / 2.0f + spread, rh / 2.0f);
        QRectF rect(c.x() - rw / 2.0f, c.y() - rh / 2.0f, rw, rh);
        QPainterPath path;
        path.addRoundedRect(rect, cr, cr);

        p.setPen(QPen(QColor(255, 255, 255, int(alpha * 0.85f * 255)),
                      1.2f, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.setBrush(Qt::NoBrush);
        p.drawPath(path);
    }

private:
    QTimer* m_timer      = nullptr;
    float   m_phase      = 0.0f;
    int     m_cyclesDone = 0;
    float   m_btnW       = 80.0f;
    float   m_btnH       = 28.0f;
};

ProjectListPage::ProjectListPage(QWidget* parent)
    : QWidget(parent)
    , m_darkMode(false)
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // ── 项目网格滚动区域 ────────────────────────────────────────
    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_scrollArea->setFrameShape(QFrame::NoFrame);
    m_scrollArea->setStyleSheet("QScrollArea { background: transparent; border: none; }");

    m_gridContainer = new QWidget();
    m_gridContainer->setObjectName("gridContainer");
    m_scrollArea->setWidget(m_gridContainer);

    // ── 搜索结果滚动区域 ─────────────────────────────────────────
    m_searchScrollArea = new QScrollArea(this);
    m_searchScrollArea->setWidgetResizable(true);
    m_searchScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_searchScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_searchScrollArea->setFrameShape(QFrame::NoFrame);
    m_searchScrollArea->setStyleSheet("QScrollArea { border: none; }");

    QWidget* searchContent = new QWidget();
    QVBoxLayout* searchLayout = new QVBoxLayout(searchContent);
    // WaterfallWidget 自身有 ItemSpacing(8px) 的内边距，
    // 项目卡片页左右边距为 CardSpacing(16px)，补 8px 使两者对齐
    searchLayout->setContentsMargins(CardSpacing - 8, 0, CardSpacing - 8, 0);
    searchLayout->setSpacing(0);
    m_searchResults = new WaterfallWidget(searchContent);
    connect(m_searchResults, &WaterfallWidget::itemClicked,
            this, &ProjectListPage::mediaFileClicked);
    searchLayout->addWidget(m_searchResults);
    searchLayout->addStretch(1);
    m_searchScrollArea->setWidget(searchContent);
    m_searchScrollArea->hide();  // 初始隐藏，搜索激活时浮动显示

    // ── 项目网格直接加入主布局 ────────────────────────────────────
    mainLayout->addWidget(m_scrollArea);

    // ── 导航栏（浮动覆盖在视图上方）──────────────────────────────
    m_navBar = new GlassNavBar(this);
    m_navBar->setContentSource(m_gridContainer);

    // 搜索栏（左侧，宽度由 resizeEvent 动态设定）
    m_searchBar = new SearchBar(this);
    connect(m_searchBar, &SearchBar::textChanged,
            this, &ProjectListPage::updateSearchDropdown);
    connect(m_searchBar, &SearchBar::searchRequested,
            this, &ProjectListPage::performSearch);
    connect(m_searchBar, &SearchBar::cleared,
            this, &ProjectListPage::clearSearch);
    m_navBar->addLeftWidget(m_searchBar);

    // 右侧按钮
    const QString navBtnStyle =
        "QPushButton { border-radius: 14px; font-size: 13px; font-weight: bold; background: white;"
        "  border: none; padding: 5px 13px; color: #1C1C1E; }"
        "QPushButton:hover { background: qlineargradient(x1:0,y1:0,x2:1,y2:0,"
        "  stop:0 #E0E0E0, stop:1 #E0E0E0); color: #1C1C1E; }";

    m_addBtn = new QPushButton("添加文件夹", this);
    m_addBtn->setFlat(true);
    m_addBtn->setCursor(Qt::PointingHandCursor);
    m_addBtn->setStyleSheet(navBtnStyle);
    connect(m_addBtn, &QPushButton::clicked, this, &ProjectListPage::addProjectRequested);

    QPushButton* settingsBtn = new QPushButton("设置", this);
    settingsBtn->setFlat(true);
    settingsBtn->setCursor(Qt::PointingHandCursor);
    settingsBtn->setStyleSheet(navBtnStyle);
    connect(settingsBtn, &QPushButton::clicked, this, &ProjectListPage::settingsRequested);

    m_navBar->addRightWidget(m_addBtn);
    m_navBar->addRightWidget(settingsBtn);

    // ── 搜索下拉框（浮动，绝对定位，初始隐藏）────────────────────
    m_searchDropdown = new QFrame(this);
    m_searchDropdown->setObjectName("SearchDropdown");
    m_searchDropdown->setStyleSheet(
        "#SearchDropdown {"
        "  background: white;"
        "  border: none;"
        "  border-radius: 12px;"
        "}"
    );
    QVBoxLayout* dropLayout = new QVBoxLayout(m_searchDropdown);
    dropLayout->setContentsMargins(0, 4, 0, 4);
    dropLayout->setSpacing(0);

    // 预建固定数量的按钮：避免每次输入时创建/销毁子控件触发 Windows 焦点事件
    for (int i = 0; i < MaxDropdownItems; ++i) {
        QPushButton* btn = new QPushButton(m_searchDropdown);
        btn->setFlat(true);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setFixedHeight(36);
        btn->hide();
        dropLayout->addWidget(btn);
        m_dropdownBtns[i] = btn;
    }

    m_searchDropdown->hide();
    m_searchDropdown->raise();

    // ── 空状态提示（无项目时居中显示打字动画）──────────────────────
    m_emptyHint = new TypewriterLabel("(●'◡'●)ﾉ添加文件夹开始使用", this);
    m_emptyHint->hide();

    // ── 添加文件夹按钮光晕（空状态时引导视线）───────────────────────
    m_glowOverlay = new ButtonGlowOverlay(this);
    m_glowOverlay->hide();

    // 打字动画全部结束后启动光晕（文字完整显示 → 引导注意力到按钮）
    m_emptyHint->setOnFinished([this]() {
        if (!m_glowOverlay || !m_addBtn || !m_cards.isEmpty()) return;
        repositionGlow();
        m_glowOverlay->start();
    });

    // 鼠标移入按钮时光晕提前消失；应用级过滤器用于点击外部时隐藏下拉框
    m_addBtn->installEventFilter(this);
    m_searchBar->lineEdit()->installEventFilter(this);
    QApplication::instance()->installEventFilter(this);

    reloadProjects();
}

void ProjectListPage::setDarkMode(bool dark) {
    m_darkMode = dark;
    m_navBar->setDarkMode(dark);
    if (m_searchBar) m_searchBar->setDarkMode(dark);
    if (m_emptyHint)
        m_emptyHint->setTextColor(dark ? QColor(0xFF, 0xFF, 0xFF) : QColor(0x1C, 0x1C, 0x1E));
    if (m_searchDropdown) {
        QString bg = dark ? "rgb(44,44,46)" : "white";
        m_searchDropdown->setStyleSheet(
            QString("#SearchDropdown { background: %1; border: none; border-radius: 12px; }")
                .arg(bg));
    }
    // 搜索结果区背景色与页面一致，防止项目卡片透出
    QColor bg = dark ? QColor(0x1C, 0x1C, 0x1E) : QColor(0xF2, 0xF2, 0xF7);
    auto applyBg = [&](QWidget* w) {
        QPalette p = w->palette();
        p.setColor(QPalette::Window, bg);
        p.setColor(QPalette::Base,   bg);
        w->setPalette(p);
        w->setAutoFillBackground(true);
    };
    applyBg(m_searchScrollArea);
    applyBg(m_searchScrollArea->viewport());
    if (m_searchResults) m_searchResults->setDarkMode(dark);

    for (auto* card : m_cards)
        card->setDarkMode(dark);
    update();
}

QWidget* ProjectListPage::scrollContent() const {
    return m_scrollArea->widget();
}

void ProjectListPage::addSearchableFiles(const QString& rootPath,
                                         const QList<MediaFile>& files) {
    // 删除该 rootPath 的旧条目，再加入新的
    m_searchEntries.erase(
        std::remove_if(m_searchEntries.begin(), m_searchEntries.end(),
            [&](const SearchEntry& e) {
                return e.file.path.startsWith(rootPath);
            }),
        m_searchEntries.end()
    );

    for (const MediaFile& f : files) {
        SearchEntry entry;
        entry.relativePath = f.path.mid(rootPath.length() + 1); // 去掉根路径+分隔符
        entry.file = f;
        m_searchEntries.append(entry);
    }
}

void ProjectListPage::reloadProjects() {
    for (auto* card : m_cards.values())
        card->deleteLater();
    m_cards.clear();

    for (const Project& p : ProjectManager::instance()->projects()) {
        ProjectCard* card = new ProjectCard(p, m_gridContainer);
        card->setDarkMode(m_darkMode);
        connect(card, &ProjectCard::clicked, this, [this, card](const QString& id) {
            m_lastCardRectInWindow = QRect(card->mapTo(window(), QPoint(0,0)), card->size());
            emit projectClicked(id);
        });
        connect(card, &ProjectCard::deleteRequested, this, &ProjectListPage::removeProjectCard);
        m_cards.insert(p.id, card);
        card->show();
    }
    relayoutCards();
}

void ProjectListPage::addProjectCard(const Project& project) {
    if (m_cards.contains(project.id)) {
        updateProjectCard(project);
        return;
    }
    ProjectCard* card = new ProjectCard(project, m_gridContainer);
    card->setDarkMode(m_darkMode);
    connect(card, &ProjectCard::clicked, this, [this, card](const QString& id) {
        m_lastCardRectInWindow = QRect(card->mapTo(window(), QPoint(0,0)), card->size());
        emit projectClicked(id);
    });
    connect(card, &ProjectCard::deleteRequested, this, &ProjectListPage::removeProjectCard);
    m_cards.insert(project.id, card);
    card->show();
    relayoutCards();
}

void ProjectListPage::updateProjectCard(const Project& project) {
    if (m_cards.contains(project.id))
        m_cards[project.id]->updateProject(project);
}

void ProjectListPage::setProjectThumbnail(const QString& projectId, const QPixmap& pixmap) {
    if (m_cards.contains(projectId))
        m_cards[projectId]->setThumbnail(pixmap);
}

void ProjectListPage::removeProjectCard(const QString& projectId) {
    if (!m_cards.contains(projectId)) return;

    ConfirmDialog dlg("确认删除项目（不会删除文件）", "取消", "删除",
                      true, m_darkMode, this);
    if (dlg.exec() != QDialog::Accepted) return;

    ProjectCard* card = m_cards.take(projectId);
    card->deleteLater();
    ProjectManager::instance()->removeProject(projectId);
    relayoutCards();
}

// ── 搜索逻辑 ─────────────────────────────────────────────────────

void ProjectListPage::updateSearchDropdown(const QString& query) {
    if (query.isEmpty()) {
        hideDropdown();
        if (m_searchActive) clearSearch();
        return;
    }

    // 按相对路径做包含匹配（不区分大小写）
    QList<QPair<QString,MediaFile>> matches;
    for (const SearchEntry& entry : m_searchEntries) {
        if (entry.relativePath.contains(query, Qt::CaseInsensitive)) {
            matches.append({entry.relativePath, entry.file});
            if (matches.size() >= 7) break;
        }
    }

    if (matches.isEmpty()) {
        hideDropdown();
        return;
    }

    rebuildDropdownItems(matches, query);
    positionDropdown();
    m_searchDropdown->show();
    m_searchDropdown->raise();
}

void ProjectListPage::rebuildDropdownItems(
    const QList<QPair<QString,MediaFile>>& matches, const QString& query)
{
    // 直接复用预建按钮，不创建/销毁任何子控件，彻底避免 Windows 焦点事件
    QString textColor = m_darkMode ? "#F2F2F7" : "#1C1C1E";
    QString btnStyle =
        QString("QPushButton { text-align: left; padding: 0 14px; border: none; "
                "  background: transparent; font-size: 12px; font-weight: bold; color: %1; }"
                "QPushButton:hover { background: rgba(128,128,128,0.1); }").arg(textColor);

    for (int i = 0; i < MaxDropdownItems; ++i) {
        QPushButton* btn = m_dropdownBtns[i];
        if (i < matches.size()) {
            const auto& [relPath, file] = matches[i];
            int matchPos = relPath.indexOf(query, Qt::CaseInsensitive);
            QString display = (matchPos >= 0) ? relPath.mid(matchPos) : relPath;

            btn->setText(display);
            btn->setStyleSheet(btnStyle);

            // 断开旧信号，重新绑定新文件
            disconnect(btn, &QPushButton::clicked, nullptr, nullptr);
            MediaFile capturedFile = file;
            connect(btn, &QPushButton::clicked, this, [this, capturedFile]() {
                m_searchBar->setTextSilent(capturedFile.path);
                hideDropdown();
                showSingleFile(capturedFile);
            });
            btn->show();
        } else {
            btn->hide();
        }
    }
    m_searchDropdown->adjustSize();
}

void ProjectListPage::positionDropdown() {
    // 下拉框左对齐搜索栏，出现在导航栏正下方
    // 向外扩 1px：让圆角抗锯齿边缘落在白色实心区域内，避免透明角落露出卡片边缘
    QPoint searchPos = m_searchBar->mapTo(this, QPoint(0, 0));
    int dropW = m_searchBar->width();
    int dropH = m_searchDropdown->sizeHint().height();
    if (dropH < 40) dropH = m_searchDropdown->height();
    m_searchDropdown->setGeometry(searchPos.x() - 1, GlassNavBar::NavBarHeight - 1,
                                  dropW + 2, dropH + 1);
}

void ProjectListPage::hideDropdown() {
    m_searchDropdown->hide();
}

void ProjectListPage::performSearch(const QString& query) {
    hideDropdown();
    if (query.isEmpty()) {
        clearSearch();
        return;
    }

    QList<MediaFile> results;
    for (const SearchEntry& entry : m_searchEntries) {
        if (entry.relativePath.contains(query, Qt::CaseInsensitive))
            results.append(entry.file);
    }

    if (results.isEmpty()) {
        clearSearch();
        return;
    }

    startContentTransition([this, results]() {
        m_searchResults->setFiles(results);
        for (const MediaFile& f : results) {
            if (!f.thumbnailPath.isEmpty() && QFile::exists(f.thumbnailPath))
                m_searchResults->updateThumbnail(f.path, f.thumbnailPath);
            if (f.type == MediaFile::Video && !f.previewPath.isEmpty())
                m_searchResults->updatePreview(f.path, f.previewPath);
        }
        m_searchScrollArea->show();
        m_searchScrollArea->raise();
        m_searchActive = true;
    });
}

void ProjectListPage::showSingleFile(const MediaFile& file) {
    startContentTransition([this, file]() {
        m_searchResults->setFiles({file});
        if (!file.thumbnailPath.isEmpty() && QFile::exists(file.thumbnailPath))
            m_searchResults->updateThumbnail(file.path, file.thumbnailPath);
        if (file.type == MediaFile::Video && !file.previewPath.isEmpty())
            m_searchResults->updatePreview(file.path, file.previewPath);
        m_searchScrollArea->show();
        m_searchScrollArea->raise();
        m_searchActive = true;
    });
}

void ProjectListPage::startContentTransition(std::function<void()> onDone) {
    if (m_contentOverlay) {
        m_contentOverlay->deleteLater();
        m_contentOverlay = nullptr;
    }

    // 内容区：导航栏以下部分
    QRect contentRect(0, GlassNavBar::NavBarHeight,
                      width(), height() - GlassNavBar::NavBarHeight);

    // 截取当前内容区画面
    QWidget* topLevel = window();
    QPoint topLeft = mapTo(topLevel, contentRect.topLeft());
    QPixmap snapshot = QGuiApplication::primaryScreen()->grabWindow(
        topLevel->winId(), topLeft.x(), topLeft.y(),
        contentRect.width(), contentRect.height());

    QColor bgColor = palette().color(QPalette::Window);

    m_contentOverlay = new ZoomTransitionOverlay(this, snapshot, bgColor, contentRect,
        [this, onDone]() {
            onDone();
            if (m_contentOverlay) {
                m_contentOverlay->deleteLater();
                m_contentOverlay = nullptr;
            }
        }
    );
}

void ProjectListPage::clearSearch() {
    hideDropdown();
    if (!m_searchActive) {
        // 未展示搜索结果时直接清理，不需要过渡
        m_searchResults->clear();
        m_searchScrollArea->hide();
        return;
    }
    // 搜索结果正在展示：淡出搜索结果，淡入项目卡片
    startContentTransition([this]() {
        m_searchResults->clear();
        m_searchScrollArea->hide();
        m_searchActive = false;
    });
}

// ── 布局 ──────────────────────────────────────────────────────────

int ProjectListPage::calculateColumnCount() const {
    int w = m_scrollArea->width();
    if (w < 1) return 2;
    return qMax(2, w / 240);
}

void ProjectListPage::relayoutCards() {
    int cols = calculateColumnCount();
    int totalWidth = m_scrollArea->width();
    int totalSpacing = CardSpacing * (cols + 1);
    int itemWidth = (totalWidth - totalSpacing) / cols;
    if (itemWidth < 80) itemWidth = 80;

    int itemHeight = itemWidth * 4 / 3;
    int topPad = GlassNavBar::NavBarHeight;
    int row = 0, col = 0;

    for (auto* card : m_cards.values()) {
        int x = CardSpacing + col * (itemWidth + CardSpacing);
        int y = topPad + row * (itemHeight + CardSpacing);
        card->setGeometry(x, y, itemWidth, itemHeight);
        card->show();

        col++;
        if (col >= cols) { col = 0; row++; }
    }

    int totalRows = (m_cards.size() + cols - 1) / cols;
    int totalH = topPad + totalRows * (itemHeight + CardSpacing) + CardSpacing;
    m_gridContainer->setMinimumSize(totalWidth, qMax(totalH, m_scrollArea->height()));
    m_gridContainer->resize(totalWidth, qMax(totalH, m_scrollArea->height()));

    // 无项目时展示打字动画提示 + 按钮光晕
    if (m_emptyHint) {
        if (m_cards.isEmpty()) {
            if (!m_emptyHint->isVisible() && isVisible()) {
                // 已显示的页面内删光项目：geometry 已知，直接启动
                m_emptyHint->setGeometry(0, GlassNavBar::NavBarHeight,
                                         width(), height() - GlassNavBar::NavBarHeight);
                m_emptyHint->show();
                m_emptyHint->raise();
                m_emptyHint->startAnimation();
                // 光晕在打字动画完成回调中自动触发，无需此处手动启动
            }
        } else {
            m_emptyHint->hide();
            m_emptyHint->stopAnimation();
            if (m_glowOverlay) m_glowOverlay->stop();
        }
    }
}

void ProjectListPage::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    if (!m_emptyHint || !m_cards.isEmpty()) return;

    m_emptyHint->setGeometry(0, GlassNavBar::NavBarHeight,
                             width(), height() - GlassNavBar::NavBarHeight);
    m_emptyHint->show();
    m_emptyHint->raise();

    if (m_firstShow) {
        m_firstShow = false;
        // SplashOverlay 共需 2500ms（2000ms 展示 + 500ms 淡出）
        // 延迟到遮罩消失后再播放打字动画；光晕由打字完成回调触发
        QTimer::singleShot(2500, this, [this]() {
            if (m_emptyHint && m_cards.isEmpty())
                m_emptyHint->startAnimation();
        });
    } else {
        m_emptyHint->startAnimation();
        // 光晕由打字完成回调触发，无需此处手动启动
    }
}

bool ProjectListPage::eventFilter(QObject* obj, QEvent* event) {
    if (obj == m_addBtn && event->type() == QEvent::Enter) {
        if (m_glowOverlay) m_glowOverlay->stop();
    }

    if (obj == m_searchBar->lineEdit()) {
        // 重新获得焦点时，根据当前内容恢复下拉框
        if (event->type() == QEvent::FocusIn) {
            const QString txt = m_searchBar->text();
            if (!txt.isEmpty())
                updateSearchDropdown(txt);
        }
    }

    // 应用级鼠标点击：点击搜索栏和下拉框之外的区域时隐藏下拉框
    if (event->type() == QEvent::MouseButtonPress && m_searchDropdown->isVisible()) {
        QPoint gp = static_cast<QMouseEvent*>(event)->globalPosition().toPoint();
        bool inSearchBar = m_searchBar->rect().contains(m_searchBar->mapFromGlobal(gp));
        bool inDropdown  = m_searchDropdown->rect().contains(m_searchDropdown->mapFromGlobal(gp));
        if (!inSearchBar && !inDropdown)
            hideDropdown();
    }

    return QWidget::eventFilter(obj, event);
}

void ProjectListPage::repositionGlow() {
    if (!m_glowOverlay || !m_addBtn) return;
    // 将按钮坐标从其所在父级映射到 ProjectListPage 坐标系
    QRect btnRect(m_addBtn->mapTo(this, QPoint(0, 0)), m_addBtn->size());
    m_glowOverlay->trackButton(btnRect);
    m_glowOverlay->raise();
}

void ProjectListPage::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    m_navBar->setGeometry(0, 0, width(), GlassNavBar::NavBarHeight);
    m_searchScrollArea->setGeometry(0, GlassNavBar::NavBarHeight,
                                    width(), height() - GlassNavBar::NavBarHeight);

    // 搜索栏动态宽度 = 窗口宽度 × 0.4
    if (m_searchBar) {
        int barW = static_cast<int>(width() * 0.4);
        m_navBar->setLeftMaxWidth(barW);
        m_searchBar->setFixedWidth(barW);
        // 立即触发同步布局，避免 navBar 背景已更宽但子控件几何未更新导致的裁剪闪烁
        m_navBar->layout()->activate();
    }

    // 重新定位已显示的下拉框
    if (m_searchDropdown && m_searchDropdown->isVisible())
        positionDropdown();

    // 空状态提示：始终覆盖导航栏以下区域，垂直居中由控件内部 paintEvent 处理
    if (m_emptyHint)
        m_emptyHint->setGeometry(0, GlassNavBar::NavBarHeight,
                                 width(), height() - GlassNavBar::NavBarHeight);

    // 光晕跟随按钮位置（navBar 布局稳定后再映射）
    QTimer::singleShot(0, this, [this]() { repositionGlow(); });

    relayoutCards();
}
