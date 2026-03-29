#pragma once
#include <QWidget>
#include <QTimer>
#include <QPainter>
#include <QPainterPath>
#include <QConicalGradient>
#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>
#include "AnimUtils.h"

// 点击卡片时的加载遮罩：半透明黑色背景 + 紫蓝渐变旋转圆弧
class LoadingOverlay : public QWidget {
    Q_OBJECT
public:
    explicit LoadingOverlay(QWidget* parent)
        : QWidget(parent)
    {
        setAttribute(Qt::WA_TransparentForMouseEvents, false); // 拦截点击
        resize(parent->size());
        raise();

        m_spinTimer = new QTimer(this);
        m_spinTimer->setInterval(screenAnimInterval());
        connect(m_spinTimer, &QTimer::timeout, this, [this]() {
            // 保持 300°/秒固定角速度
            m_angle = std::fmod(m_angle + 300.0f * m_spinTimer->interval() / 1000.0f, 360.0f);
            update();
        });
        m_spinTimer->start();
    }

    // 触发淡出并在完成后 deleteLater
    void fadeOut() {
        m_spinTimer->stop();
        auto* effect = new QGraphicsOpacityEffect(this);
        effect->setOpacity(1.0);
        setGraphicsEffect(effect);
        auto* anim = new QPropertyAnimation(effect, "opacity", this);
        anim->setStartValue(1.0);
        anim->setEndValue(0.0);
        anim->setDuration(220);
        connect(anim, &QPropertyAnimation::finished, this, &QObject::deleteLater);
        anim->start(QAbstractAnimation::DeleteWhenStopped);
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);

        // 裁剪为圆角（与 ProjectCard 一致，radius=12）
        QPainterPath clip;
        clip.addRoundedRect(rect(), 12, 12);
        p.setClipPath(clip);

        // 半透明黑色背景
        p.fillRect(rect(), QColor(0, 0, 0, 155));

        // 旋转紫蓝渐变圆弧
        const double cx = width()  / 2.0;
        const double cy = height() / 2.0;
        const int r = 26;
        p.translate(cx, cy);
        p.rotate(m_angle);

        QConicalGradient grad(0, 0, 0);
        grad.setColorAt(0.00, QColor(80,  100, 255, 255));
        grad.setColorAt(0.40, QColor(160, 80,  255, 255));
        grad.setColorAt(0.85, QColor(80,  100, 255,  50));
        grad.setColorAt(1.00, QColor(80,  100, 255,   0));

        QPen pen(QBrush(grad), 3.5, Qt::SolidLine, Qt::RoundCap);
        p.setPen(pen);
        p.drawArc(-r, -r, r * 2, r * 2, 0, 300 * 16); // 300° 弧
    }

private:
    QTimer* m_spinTimer;
    float   m_angle = 0.0f;
};
