#pragma once

#include <QWidget>
#include <QRadioButton>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QScrollArea>
#include <QSettings>
#include "GlassNavBar.h"

/**
 * @brief 设置页面（全页面，加入 QStackedWidget）
 */
class SettingsPanel : public QWidget {
    Q_OBJECT

public:
    explicit SettingsPanel(QWidget* parent = nullptr);

    void refreshCacheInfo();
    void setDarkMode(bool dark);
    GlassNavBar* navBar() const { return m_navBar; }

signals:
    void appearanceChanged(bool isDark);
    void qualityChanged(const QString& quality);
    void backRequested();   // 返回上一页

protected:
    void resizeEvent(QResizeEvent* event) override;

private:
    QLabel* createSectionTitle(const QString& title);
    void loadSettings();
    void saveSettings();

    GlassNavBar*  m_navBar;
    bool          m_darkMode = false;

    QRadioButton* m_qualityLow;
    QRadioButton* m_qualityMedium;
    QRadioButton* m_qualityHigh;

    QRadioButton* m_appearSystem;
    QRadioButton* m_appearLight;
    QRadioButton* m_appearDark;

    QLabel*       m_aboutLabel;
    QLabel*       m_cacheInfoLabel;
    QScrollArea*  m_cacheScroll;
    QVBoxLayout*  m_cacheListLayout;
};
