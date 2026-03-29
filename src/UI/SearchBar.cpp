#include "SearchBar.h"
#include <QHBoxLayout>

SearchBar::SearchBar(QWidget* parent)
    : QWidget(parent)
{
    QHBoxLayout* outerLayout = new QHBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->setSpacing(0);

    // ── 输入框容器（带蓝紫边框）────────────────────────────────
    m_inputContainer = new QWidget(this);
    m_inputContainer->setObjectName("SearchInputBox");
    m_inputContainer->setFixedHeight(30);
    m_inputContainer->setStyleSheet(
        "#SearchInputBox {"
        "  border: none;"
        "  border-radius: 14px;"
        "  background: rgba(255,255,255,0.95);"
        "}"
    );

    QHBoxLayout* innerLayout = new QHBoxLayout(m_inputContainer);
    innerLayout->setContentsMargins(12, 1, 1, 1);
    innerLayout->setSpacing(4);

    // 输入框
    m_lineEdit = new QLineEdit(m_inputContainer);
    m_lineEdit->setFrame(false);
    m_lineEdit->setPlaceholderText("");
    m_lineEdit->setStyleSheet(
        "QLineEdit {"
        "  border: none;"
        "  background: transparent;"
        "  font-size: 13px;"
        "  color: #1C1C1E;"
        "  padding: 0;"
        "}"
    );

    // 清除按钮：圆形背景 + ✕ 图标
    m_clearBtn = new QPushButton("✕", m_inputContainer);
    m_clearBtn->setFlat(true);
    m_clearBtn->setFixedSize(18, 18);
    m_clearBtn->setCursor(Qt::PointingHandCursor);
    m_clearBtn->setStyleSheet(
        "QPushButton {"
        "  border: none;"
        "  border-radius: 9px;"
        "  background: rgba(60,60,67,0.25);"
        "  color: white;"
        "  font-size: 9px;"
        "  font-weight: bold;"
        "  padding: 0;"
        "}"
        "QPushButton:hover { background: rgba(60,60,67,0.45); }"
    );
    m_clearBtn->hide();

    // ── 搜索按钮（在输入框容器内，右侧）─────────────────────────
    m_searchBtn = new QPushButton("搜索", m_inputContainer);
    m_searchBtn->setFixedHeight(28);
    m_searchBtn->setCursor(Qt::PointingHandCursor);
    m_searchBtn->setStyleSheet(
        "QPushButton {"
        "  border-radius: 14px;"
        "  font-size: 12px;"
        "  font-weight: bold;"
        "  color: white;"
        "  padding: 0 18px;"
        "  border: none;"
        "  background: #0A84FF;"
        "}"
        "QPushButton:hover { background: #3399FF; }"
        "QPushButton:pressed { background: #0070E0; }"
    );

    innerLayout->addWidget(m_lineEdit, 1);
    innerLayout->addWidget(m_clearBtn);
    innerLayout->addSpacing(4);
    innerLayout->addWidget(m_searchBtn);

    outerLayout->addWidget(m_inputContainer, 1);

    // ── 信号连接 ─────────────────────────────────────────────────
    connect(m_lineEdit, &QLineEdit::textChanged, this, [this](const QString& txt) {
        m_clearBtn->setVisible(!txt.isEmpty());
        emit textChanged(txt);
    });
    connect(m_lineEdit, &QLineEdit::returnPressed, this, [this]() {
        emit searchRequested(m_lineEdit->text());
    });
    connect(m_clearBtn, &QPushButton::clicked, this, [this]() {
        m_lineEdit->clear();
        emit cleared();
    });
    connect(m_searchBtn, &QPushButton::clicked, this, [this]() {
        emit searchRequested(m_lineEdit->text());
    });
}

void SearchBar::setDarkMode(bool /*dark*/) {
    m_searchBtn->setStyleSheet(
        "QPushButton { border-radius: 14px; font-size: 12px; font-weight: bold;"
        "  color: white; padding: 0 18px; border: none; background: #0A84FF; }"
        "QPushButton:hover { background: #3399FF; }"
        "QPushButton:pressed { background: #0070E0; }"
    );
}

QString SearchBar::text() const {
    return m_lineEdit->text();
}

void SearchBar::setTextSilent(const QString& text) {
    m_lineEdit->blockSignals(true);
    m_lineEdit->setText(text);
    m_clearBtn->setVisible(!text.isEmpty());
    m_lineEdit->blockSignals(false);
}
