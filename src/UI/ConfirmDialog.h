#pragma once

#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QPainter>
#include <QPainterPath>
#include "AnimUtils.h"

/**
 * @brief 统一风格的确认对话框
 * - 无标题栏，文字水平居中
 * - 宽度 = 父窗口一列宽度，高度 = 宽度 × 0.618
 * - 按钮形状与"添加文件夹"一致（胶囊样式）
 * - 左按钮左边距 = 右按钮右边距（水平对称）
 */
class ConfirmDialog : public QDialog {
    bool m_dark;

    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        QColor bg = m_dark ? QColor(44, 44, 46) : QColor(255, 255, 255);
        QPainterPath path;
        path.addRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), 12, 12);
        p.fillPath(path, bg);
        p.setPen(QPen(m_dark ? QColor(255, 255, 255, 40) : QColor(0, 0, 0, 25), 1));
        p.setBrush(Qt::NoBrush);
        p.drawPath(path);
    }

    void showEvent(QShowEvent* e) override {
        // 相对父窗口居中显示
        if (parentWidget()) {
            QPoint center = parentWidget()->mapToGlobal(parentWidget()->rect().center());
            move(center.x() - width() / 2, center.y() - height() / 2);
        }
        QDialog::showEvent(e);
    }

public:
    /**
     * @param text              显示的说明文字
     * @param cancelText        左侧取消按钮文字
     * @param confirmText       右侧确认按钮文字
     * @param confirmDestructive 确认按钮是否为危险操作（红色）
     * @param dark              是否深色模式
     * @param parent            父控件
     */
    explicit ConfirmDialog(const QString& text,
                           const QString& cancelText,
                           const QString& confirmText,
                           bool           confirmDestructive,
                           bool           dark,
                           QWidget*       parent = nullptr)
        : QDialog(parent, Qt::Dialog | Qt::FramelessWindowHint)
        , m_dark(dark)
    {
        setAttribute(Qt::WA_TranslucentBackground);

        // 宽度 = 父窗口一列瀑布流宽度；高度 = 宽度 × 0.618（黄金比例）
        int winW = parent ? parent->topLevelWidget()->width() : 900;
        int cols = qMax(2, winW / 480);
        int baseW = (winW - 8 * (cols + 1)) / cols;
        int dlgW = qBound(168, (int)(baseW * 0.6), 336);
        int dlgH = qRound(dlgW * 0.618);
        setFixedSize(dlgW, dlgH);

        // ---- 布局 ----
        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(28, 28, 28, 24);
        layout->setSpacing(0);

        // 文字（水平居中，自动换行）
        auto* label = new QLabel(text, this);
        label->setAlignment(Qt::AlignCenter);
        label->setWordWrap(true);
        label->setStyleSheet(QString(
            "color: %1; background: transparent; font-size: 14px; font-weight: bold;")
            .arg(dark ? "#F2F2F7" : "#1C1C1E"));
        layout->addWidget(label, 1);

        // 按钮行：两个按钮等宽，外侧边距对称
        auto* btnLayout = new QHBoxLayout();
        btnLayout->setContentsMargins(0, 0, 0, 0);
        btnLayout->setSpacing(12);

        // 胶囊按钮样式（与"添加文件夹"形状一致）
        auto mkStyle = [&](bool destructive) -> QString {
            if (destructive) {
                return dark
                    ? "QPushButton { border-radius: 14px; font-size: 13px; font-weight: bold; background: #FFFFFF; "
                      "border: none; padding: 6px 0; color: #FF3B30; }"
                      "QPushButton:hover { background: #e5e5ea; }"
                    : "QPushButton { border-radius: 14px; font-size: 13px; font-weight: bold; background: #F2F2F7; "
                      "border: none; padding: 6px 0; color: #FF3B30; }"
                      "QPushButton:hover { background: #e5e5ea; }";
            }
            return dark
                ? "QPushButton { border-radius: 14px; font-size: 13px; font-weight: bold; background: #FFFFFF; "
                  "border: none; padding: 6px 0; color: #1C1C1E; }"
                  "QPushButton:hover { background: #e5e5ea; }"
                : "QPushButton { border-radius: 14px; font-size: 13px; font-weight: bold; background: #F2F2F7; "
                  "border: none; padding: 6px 0; color: #1C1C1E; }"
                  "QPushButton:hover { background: #e5e5ea; }";
        };

        auto* cancelBtn  = new QPushButton(cancelText,  this);
        auto* confirmBtn = new QPushButton(confirmText, this);
        cancelBtn->setStyleSheet(mkStyle(false));
        confirmBtn->setStyleSheet(mkStyle(confirmDestructive));
        cancelBtn->setCursor(Qt::PointingHandCursor);
        confirmBtn->setCursor(Qt::PointingHandCursor);
        setupCustomStyledButton(cancelBtn);
        setupCustomStyledButton(confirmBtn);
        cancelBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        confirmBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

        connect(cancelBtn,  &QPushButton::clicked, this, &QDialog::reject);
        connect(confirmBtn, &QPushButton::clicked, this, &QDialog::accept);

        btnLayout->addWidget(cancelBtn);
        btnLayout->addWidget(confirmBtn);
        layout->addLayout(btnLayout);
    }
};
