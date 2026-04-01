#pragma once

#include <QWidget>
#include <QScrollArea>
#include <QMap>
#include <QList>
#include <QFrame>
#include "GlassNavBar.h"
#include "AnimUtils.h"
#include "ProjectCard.h"
#include "WaterfallWidget.h"
#include "SearchBar.h"
#include "Project.h"
#include "MediaFile.h"

class TypewriterLabel;
class ButtonGlowOverlay;

class ProjectListPage : public QWidget {
    Q_OBJECT

public:
    explicit ProjectListPage(QWidget* parent = nullptr);

    void setDarkMode(bool dark);
    GlassNavBar* navBar() const { return m_navBar; }
    QWidget* scrollContent() const;

    void setProjectThumbnail(const QString& projectId, const QPixmap& pixmap);

    // 点击时的卡片位置（供 MainWindow 驱动缩放过渡动画）
    QRect   lastCardRectInWindow()   const { return m_lastCardRectInWindow; }

    // 添加/更新可搜索文件（由 MainWindow 在扫描完成后调用）
    void addSearchableFiles(const QString& rootPath, const QList<MediaFile>& files);

public slots:
    void addProjectCard(const Project& project);
    void updateProjectCard(const Project& project);
    void removeProjectCard(const QString& projectId);
    void reloadProjects();

signals:
    void projectClicked(const QString& projectId);
    void addProjectRequested();
    void settingsRequested();
    void mediaFileClicked(const MediaFile& file);

protected:
    void resizeEvent(QResizeEvent* event) override;
    void showEvent(QShowEvent* event) override;
    bool eventFilter(QObject* obj, QEvent* event) override;

private:
    void relayoutCards();
    int  calculateColumnCount() const;

    // 搜索相关
    void updateSearchDropdown(const QString& query);
    void performSearch(const QString& query);
    void showSingleFile(const MediaFile& file);
    void clearSearch();
    void hideDropdown();
    void positionDropdown();
    void rebuildDropdownItems(const QList<QPair<QString,MediaFile>>& matches,
                              const QString& query);
    void startContentTransition(std::function<void()> onDone);

    // 卡片点击时记录位置，供 MainWindow 制作过渡动画
    QRect    m_lastCardRectInWindow;

    // ── 项目网格 ──────────────────────────────────────────────────
    GlassNavBar*   m_navBar;
    QScrollArea*   m_scrollArea;
    QWidget*       m_gridContainer;
    QMap<QString, ProjectCard*> m_cards;
    bool m_darkMode;
    bool m_firstShow = true;   // 首次 showEvent 需等待 SplashOverlay 消散
    static constexpr int CardSpacing = 16;

    void repositionGlow();

    ZoomTransitionOverlay* m_contentOverlay = nullptr;

    // ── 空状态提示 ────────────────────────────────────────────────
    TypewriterLabel*    m_emptyHint     = nullptr;
    QPushButton*        m_addBtn        = nullptr;
    ButtonGlowOverlay*  m_glowOverlay   = nullptr;

    // ── 搜索 ──────────────────────────────────────────────────────
    SearchBar*       m_searchBar      = nullptr;
    QWidget*         m_searchDropdown = nullptr;  // 绝对定位浮动下拉框
    static constexpr int MaxDropdownItems = 7;
    QPushButton*     m_dropdownBtns[MaxDropdownItems] = {};  // 预建按钮，避免反复创建
    QScrollArea*     m_searchScrollArea = nullptr;  // 浮动覆盖，搜索激活时显示
    WaterfallWidget* m_searchResults  = nullptr;
    bool             m_searchActive   = false;

    struct SearchEntry {
        QString  relativePath;
        MediaFile file;
    };
    QList<SearchEntry> m_searchEntries;
};
