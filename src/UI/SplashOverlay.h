#pragma once
#include <QWidget>

class QTimer;
class QVariantAnimation;
class QGraphicsOpacityEffect;

// 启动遮罩层：全屏覆盖主窗口，展示 NEXTVIEW 蓝色渐变扫光文字。
// 显示 1 秒后以"中心放大+淡出"动效消散（0.5 秒），随后自动销毁。
class SplashOverlay : public QWidget {
    Q_OBJECT

public:
    explicit SplashOverlay(QWidget* parent);

protected:
    void paintEvent(QPaintEvent*) override;
    bool eventFilter(QObject* obj, QEvent* event) override;

private:
    void startDismiss();

    QTimer*                 m_shimmerTimer  = nullptr;
    QVariantAnimation*      m_dismissAnim   = nullptr;
    QGraphicsOpacityEffect* m_opacityEffect = nullptr;

    float  m_shimmerPhase   = 0.0f;  // 扫光相位 0~1
    float  m_scale          = 1.0f;  // 消散放大系数
    qint64 m_shimmerStartMs = 0;     // 扫光起始时间戳（墙上时钟）
};
