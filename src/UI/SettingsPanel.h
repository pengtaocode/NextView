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
    void backRequested();
    void activationChanged();   // 激活状态发生变化，MainWindow 据此重启缓存

protected:
    void resizeEvent(QResizeEvent* event) override;

private:
    QLabel* createSectionTitle(const QString& title);
    void loadSettings();
    void saveSettings();
    void refreshActivationStatus();

    GlassNavBar*  m_navBar;
    bool          m_darkMode = false;

    // 激活区块
    QLabel*       m_activationStatusLabel = nullptr;
    QPushButton*  m_activateBtn           = nullptr;

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
