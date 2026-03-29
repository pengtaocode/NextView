#pragma once
#include <QWidget>
#include <QLineEdit>
#include <QPushButton>

// 搜索栏：输入框 + 清除 × + 搜索按钮（7px 间距在边框外）
// 父级通过 setFixedWidth 设置总宽度（建议 = 窗口宽 × 0.6）
class SearchBar : public QWidget {
    Q_OBJECT
public:
    explicit SearchBar(QWidget* parent = nullptr);

    QString    text() const;
    void       setTextSilent(const QString& text);
    void       setDarkMode(bool dark);
    QLineEdit* lineEdit() const { return m_lineEdit; }

signals:
    void textChanged(const QString& text);
    void searchRequested(const QString& text);
    void cleared();

private:
    QWidget*     m_inputContainer;
    QLineEdit*   m_lineEdit;
    QPushButton* m_clearBtn;
    QPushButton* m_searchBtn;
};
