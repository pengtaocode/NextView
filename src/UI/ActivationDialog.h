#pragma once

#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QPainter>
#include <QPainterPath>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include "LicenseManager.h"

/**
 * @brief 激活码输入对话框
 * 视觉风格与 ConfirmDialog 一致（无标题栏、圆角、毛玻璃色背景）
 *
 * 激活流程：
 *   1. parseCode()    —— 本地解析格式（无网络）
 *   2. POST /activate —— 携带 key1 + machine_code 向服务端请求签名 KEY
 *   3. verifyAndSave() —— 本地 Ed25519 验签 + AES 加密持久化
 */
class ActivationDialog : public QDialog {
    Q_OBJECT

    bool        m_dark;
    QWidget*    m_inputPage    = nullptr;
    QWidget*    m_successPage  = nullptr;
    QLineEdit*  m_codeInput    = nullptr;
    QLabel*     m_errorLabel   = nullptr;
    QPushButton* m_activateBtn = nullptr;
    QPushButton* m_cancelBtn   = nullptr;
    QNetworkAccessManager* m_nam         = nullptr;
    QNetworkReply*         m_activeReply = nullptr; // 当前进行中的请求

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
        if (parentWidget()) {
            QPoint c = parentWidget()->mapToGlobal(parentWidget()->rect().center());
            move(c.x() - width() / 2, c.y() - height() / 2);
        }
        QDialog::showEvent(e);
        if (m_codeInput) m_codeInput->setFocus();
    }

    void closeEvent(QCloseEvent* e) override {
        abortRequest();
        QDialog::closeEvent(e);
    }

    QString btnStyleNormal() const {
        return m_dark
            ? "QPushButton { border-radius: 14px; font-size: 13px; font-weight: bold;"
              "  background: #FFFFFF; border: none; padding: 7px 0; color: #1C1C1E; }"
              "QPushButton:hover { background: #e5e5ea; }"
              "QPushButton:disabled { background: rgba(255,255,255,0.3); color: rgba(28,28,30,0.4); }"
            : "QPushButton { border-radius: 14px; font-size: 13px; font-weight: bold;"
              "  background: #F2F2F7; border: none; padding: 7px 0; color: #1C1C1E; }"
              "QPushButton:hover { background: #e5e5ea; }"
              "QPushButton:disabled { background: rgba(242,242,247,0.5); color: rgba(28,28,30,0.4); }";
    }

    static QString btnStylePrimary() {
        return "QPushButton { border-radius: 14px; font-size: 13px; font-weight: bold;"
               "  background: #0A84FF; border: none; padding: 7px 0; color: white; }"
               "QPushButton:hover   { background: #3399FF; }"
               "QPushButton:pressed { background: #0070E0; }"
               "QPushButton:disabled { background: rgba(10,132,255,0.4); color: rgba(255,255,255,0.5); }";
    }

    // 请求进行中时禁用输入控件
    void setLoading(bool loading) {
        m_activateBtn->setEnabled(!loading);
        m_activateBtn->setText(loading ? "验证中..." : "激活");
        m_cancelBtn->setEnabled(!loading);
        m_codeInput->setEnabled(!loading);
    }

    void abortRequest() {
        if (m_activeReply) {
            m_activeReply->abort();
            m_activeReply = nullptr;
        }
    }

    // 向服务端发起激活请求
    void startRequest(const QString& apiUrl, const QByteArray& key1) {
        abortRequest();
        setLoading(true);
        m_errorLabel->clear();

        if (!m_nam) m_nam = new QNetworkAccessManager(this);

        QNetworkRequest req(QUrl(apiUrl + "/activate"));
        req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        req.setTransferTimeout(15000);  // 15s 超时

        QJsonObject body;
        body["key1"]         = QString::fromLatin1(key1.toHex());
        body["machine_code"] = LicenseManager::instance()->machineCode();

        m_activeReply = m_nam->post(req, QJsonDocument(body).toJson(QJsonDocument::Compact));

        connect(m_activeReply, &QNetworkReply::finished, this, [this]() {
            QNetworkReply* reply = m_activeReply;
            m_activeReply = nullptr;
            if (!reply) return;
            reply->deleteLater();
            setLoading(false);

            // 网络层错误（含主动 abort）
            if (reply->error() != QNetworkReply::NoError) {
                if (reply->error() != QNetworkReply::OperationCanceledError)
                    m_errorLabel->setText("激活码无效，请重新输入");
                return;
            }

            // HTTP 状态码非 200
            int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            if (status != 200) {
                m_errorLabel->setText("激活码无效，请重新输入");
                return;
            }

            // 解析 JSON 响应
            QJsonParseError pe;
            QJsonDocument doc = QJsonDocument::fromJson(reply->readAll(), &pe);
            if (pe.error != QJsonParseError::NoError || !doc.isObject()) {
                m_errorLabel->setText("激活码无效，请重新输入");
                return;
            }

            QString keyB32 = doc.object()["key"].toString();

            // 本地 Ed25519 验签 + 机器码匹配 + 有效期检查 + AES 持久化
            QString err = LicenseManager::instance()->verifyAndSave(keyB32);
            if (err.isEmpty()) {
                showSuccessPage();
            } else {
                m_errorLabel->setText(err);
            }
        });
    }

public:
    explicit ActivationDialog(bool dark, QWidget* parent = nullptr)
        : QDialog(parent, Qt::Dialog | Qt::FramelessWindowHint)
        , m_dark(dark)
    {
        setAttribute(Qt::WA_TranslucentBackground);

        int winW = parent ? parent->topLevelWidget()->width() : 900;
        int cols = qMax(2, winW / 480);
        int baseW = (winW - 8 * (cols + 1)) / cols;
        int dlgW  = qBound(260, int(baseW * 0.80), 420);
        setFixedWidth(dlgW);

        auto* root = new QVBoxLayout(this);
        root->setContentsMargins(0, 0, 0, 0);
        root->setSpacing(0);

        // ── 输入页 ────────────────────────────────────────────────
        m_inputPage = new QWidget(this);
        m_inputPage->setStyleSheet("background: transparent;");
        auto* inputLayout = new QVBoxLayout(m_inputPage);
        inputLayout->setContentsMargins(28, 24, 28, 20);
        inputLayout->setSpacing(0);

        QString textColor = dark ? "#F2F2F7" : "#1C1C1E";

        // 标题
        auto* title = new QLabel("输入激活码", m_inputPage);
        title->setAlignment(Qt::AlignCenter);
        title->setStyleSheet(QString("color: %1; background: transparent;"
                                     "font-size: 14px; font-weight: bold;").arg(textColor));
        inputLayout->addWidget(title);
        inputLayout->addSpacing(16);

        // 输入框
        m_codeInput = new QLineEdit(m_inputPage);
        m_codeInput->setPlaceholderText("XXXXX-XXXXX-XXXXX-XXXXX");
        m_codeInput->setAlignment(Qt::AlignCenter);
        m_codeInput->setStyleSheet(QString(
            "QLineEdit {"
            "  border: 1px solid transparent;"
            "  border-radius: 8px;"
            "  padding: 8px 12px;"
            "  font-size: 13px;"
            "  background: %1;"
            "  color: %2;"
            "}"
            "QLineEdit:focus   { border-color: #0A84FF; }"
            "QLineEdit:disabled { opacity: 0.5; }")
            .arg(dark ? "#1C1C1E" : "#F2F2F7")
            .arg(textColor));
        inputLayout->addWidget(m_codeInput);

        // 错误提示（始终占位，空文本时不显示内容）
        m_errorLabel = new QLabel(m_inputPage);
        m_errorLabel->setAlignment(Qt::AlignCenter);
        m_errorLabel->setWordWrap(true);
        m_errorLabel->setFixedHeight(18);
        m_errorLabel->setStyleSheet("color: #FF3B30; background: transparent; font-size: 12px;");
        inputLayout->addWidget(m_errorLabel);

        inputLayout->addSpacing(16);

        // 按钮行
        auto* btnRow = new QHBoxLayout();
        btnRow->setSpacing(12);

        m_cancelBtn   = new QPushButton("取消", m_inputPage);
        m_activateBtn = new QPushButton("激活", m_inputPage);
        m_cancelBtn->setStyleSheet(btnStyleNormal());
        m_activateBtn->setStyleSheet(btnStylePrimary());
        m_cancelBtn->setCursor(Qt::PointingHandCursor);
        m_activateBtn->setCursor(Qt::PointingHandCursor);
        m_cancelBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        m_activateBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

        connect(m_cancelBtn, &QPushButton::clicked, this, [this]() {
            abortRequest();
            reject();
        });

        connect(m_activateBtn, &QPushButton::clicked, this, [this]() {
            // 1. 本地解析激活码（格式验证 + Base32 解码）
            QString    apiUrl;
            QByteArray key1;
            QString err = LicenseManager::instance()->parseCode(
                m_codeInput->text(), apiUrl, key1);
            if (!err.isEmpty()) {
                m_errorLabel->setText(err);
                return;
            }
            // 2. 向服务端发起请求（异步，回调中完成验签和持久化）
            startRequest(apiUrl, key1);
        });

        connect(m_codeInput, &QLineEdit::returnPressed,  m_activateBtn, &QPushButton::click);
        connect(m_codeInput, &QLineEdit::textChanged,    this, [this]() { m_errorLabel->clear(); });

        btnRow->addWidget(m_cancelBtn);
        btnRow->addWidget(m_activateBtn);
        inputLayout->addLayout(btnRow);

        root->addWidget(m_inputPage);

        // ── 成功页 ────────────────────────────────────────────────
        m_successPage = new QWidget(this);
        m_successPage->setStyleSheet("background: transparent;");
        m_successPage->hide();
        auto* successLayout = new QVBoxLayout(m_successPage);
        successLayout->setContentsMargins(28, 28, 28, 20);
        successLayout->setSpacing(0);

        auto* successIcon = new QLabel("✓", m_successPage);
        successIcon->setAlignment(Qt::AlignCenter);
        successIcon->setStyleSheet("color: #34C759; background: transparent;"
                                   "font-size: 28px; font-weight: bold;");
        successLayout->addWidget(successIcon);
        successLayout->addSpacing(10);

        auto* successText = new QLabel("激活成功\n视频预览数量限制已解除", m_successPage);
        successText->setAlignment(Qt::AlignCenter);
        successText->setWordWrap(true);
        successText->setStyleSheet(QString("color: %1; background: transparent;"
                                           "font-size: 14px; font-weight: bold;").arg(textColor));
        successLayout->addWidget(successText);
        successLayout->addSpacing(20);

        auto* okBtn = new QPushButton("好的", m_successPage);
        okBtn->setStyleSheet(btnStylePrimary());
        okBtn->setCursor(Qt::PointingHandCursor);
        connect(okBtn, &QPushButton::clicked, this, &QDialog::accept);
        successLayout->addWidget(okBtn);

        root->addWidget(m_successPage);

        adjustSize();
    }

private:
    void showSuccessPage() {
        m_inputPage->hide();
        m_successPage->show();
        adjustSize();
        if (parentWidget()) {
            QPoint c = parentWidget()->mapToGlobal(parentWidget()->rect().center());
            move(c.x() - width() / 2, c.y() - height() / 2);
        }
    }
};
