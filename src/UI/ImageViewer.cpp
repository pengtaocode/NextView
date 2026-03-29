#include "ImageViewer.h"

#include <QPainter>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QResizeEvent>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QFont>
#include <QApplication>
#include <QScreen>
#include <QDebug>
#include <QFile>
#include <cmath>

ImageViewer::ImageViewer(QWidget* parent)
    : QDialog(parent, Qt::Window | Qt::FramelessWindowHint)
    , m_currentIndex(-1)
    , m_loading(false)
    , m_scale(1.0)
    , m_offset(0, 0)
    , m_dragging(false)
{
    // 黑色背景全屏对话框
    setStyleSheet("QDialog { background: black; }");
    setAttribute(Qt::WA_DeleteOnClose, false);

    // 最大化显示
    setWindowState(Qt::WindowMaximized);

    // ---- 顶部信息栏 ----
    m_infoBar = new QWidget(this);
    m_infoBar->setGeometry(0, 0, 1200, 52);
    m_infoBar->setStyleSheet("background: rgba(0,0,0,0.6);");

    QHBoxLayout* infoLayout = new QHBoxLayout(m_infoBar);
    infoLayout->setContentsMargins(16, 8, 16, 8);
    infoLayout->setSpacing(12);

    m_filenameLabel = new QLabel(m_infoBar);
    m_filenameLabel->setStyleSheet("color: white; font-size: 14px;");
    m_filenameLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    m_indexLabel = new QLabel(m_infoBar);
    m_indexLabel->setStyleSheet("color: rgba(255,255,255,0.7); font-size: 13px;");

    m_closeBtn = new QPushButton("✕", m_infoBar);
    m_closeBtn->setFixedSize(32, 32);
    m_closeBtn->setStyleSheet(
        "QPushButton { background: rgba(255,255,255,0.1); color: white; "
        "border-radius: 16px; font-size: 14px; font-weight: bold; border: none; }"
        "QPushButton:hover { background: rgba(255,255,255,0.25); }"
    );
    connect(m_closeBtn, &QPushButton::clicked, this, &ImageViewer::hide);

    infoLayout->addWidget(m_filenameLabel);
    infoLayout->addWidget(m_indexLabel);
    infoLayout->addWidget(m_closeBtn);

    // ---- 左侧翻页按钮 ----
    m_prevBtn = new QPushButton("❮", this);
    m_prevBtn->setFixedSize(48, 80);
    m_prevBtn->setStyleSheet(
        "QPushButton { background: rgba(255,255,255,0.12); color: white; "
        "border-radius: 8px; font-size: 20px; font-weight: bold; border: none; }"
        "QPushButton:hover { background: rgba(255,255,255,0.25); }"
        "QPushButton:pressed { background: rgba(255,255,255,0.35); }"
    );
    connect(m_prevBtn, &QPushButton::clicked, this, &ImageViewer::showPrevious);

    // ---- 右侧翻页按钮 ----
    m_nextBtn = new QPushButton("❯", this);
    m_nextBtn->setFixedSize(48, 80);
    m_nextBtn->setStyleSheet(
        "QPushButton { background: rgba(255,255,255,0.12); color: white; "
        "border-radius: 8px; font-size: 20px; font-weight: bold; border: none; }"
        "QPushButton:hover { background: rgba(255,255,255,0.25); }"
        "QPushButton:pressed { background: rgba(255,255,255,0.35); }"
    );
    connect(m_nextBtn, &QPushButton::clicked, this, &ImageViewer::showNext);

    setCursor(Qt::ArrowCursor);
}

void ImageViewer::open(const QList<MediaFile>& files, int startIndex) {
    // 只保留图片文件
    m_files.clear();
    int adjustedIndex = 0;
    int imageCount = 0;

    for (const MediaFile& f : files) {
        if (f.type == MediaFile::Image) {
            if (f.path == files.value(startIndex).path) {
                adjustedIndex = imageCount;
            }
            m_files.append(f);
            imageCount++;
        }
    }

    if (m_files.isEmpty()) return;

    adjustedIndex = qBound(0, adjustedIndex, m_files.size() - 1);

    showMaximized();
    loadImage(adjustedIndex);
}

void ImageViewer::loadImage(int index) {
    if (index < 0 || index >= m_files.size()) return;

    m_currentIndex = index;
    m_loading = true;

    // 加载图片
    QPixmap pixmap(m_files[index].path);
    if (pixmap.isNull() && m_files[index].hasThumbnail()) {
        pixmap = QPixmap(m_files[index].thumbnailPath);
    }

    m_currentPixmap = pixmap;
    m_loading = false;

    // 适应窗口大小
    fitToWindow();

    // 更新界面
    updateInfoBar();
    updateNavButtons();
    update();
}

void ImageViewer::fitToWindow() {
    if (m_currentPixmap.isNull()) {
        m_scale = 1.0;
        m_offset = QPointF(0, 0);
        return;
    }

    int viewW = width();
    int viewH = height() - 52; // 减去信息栏高度

    double scaleW = static_cast<double>(viewW) / m_currentPixmap.width();
    double scaleH = static_cast<double>(viewH) / m_currentPixmap.height();
    m_scale = qMin(scaleW, scaleH);

    // 居中
    m_offset = QPointF(0, 0);
}

void ImageViewer::updateInfoBar() {
    if (m_currentIndex < 0 || m_currentIndex >= m_files.size()) return;

    m_filenameLabel->setText(m_files[m_currentIndex].name);
    m_indexLabel->setText(QString("%1 / %2").arg(m_currentIndex + 1).arg(m_files.size()));

    // 更新信息栏宽度
    m_infoBar->setGeometry(0, 0, width(), 52);
}

void ImageViewer::updateNavButtons() {
    bool hasPrev = m_currentIndex > 0;
    bool hasNext = m_currentIndex < m_files.size() - 1;

    m_prevBtn->setVisible(hasPrev);
    m_nextBtn->setVisible(hasNext);

    // 左侧按钮居中垂直
    int centerY = (height() - m_prevBtn->height()) / 2;
    m_prevBtn->move(16, centerY);
    m_nextBtn->move(width() - m_nextBtn->width() - 16, centerY);
}

QRectF ImageViewer::imageRect() const {
    if (m_currentPixmap.isNull()) return QRectF();

    double scaledW = m_currentPixmap.width() * m_scale;
    double scaledH = m_currentPixmap.height() * m_scale;

    // 视图中心（排除信息栏）
    double centerX = width() / 2.0;
    double centerY = 52 + (height() - 52) / 2.0;

    return QRectF(
        centerX - scaledW / 2.0 + m_offset.x(),
        centerY - scaledH / 2.0 + m_offset.y(),
        scaledW,
        scaledH
    );
}

void ImageViewer::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event)
    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    // 黑色背景
    painter.fillRect(rect(), Qt::black);

    if (!m_currentPixmap.isNull()) {
        QRectF r = imageRect();
        painter.drawPixmap(r, m_currentPixmap, QRectF(m_currentPixmap.rect()));
    }
}

void ImageViewer::keyPressEvent(QKeyEvent* event) {
    switch (event->key()) {
    case Qt::Key_Escape:
        hide();
        break;
    case Qt::Key_Left:
    case Qt::Key_Up:
        showPrevious();
        break;
    case Qt::Key_Right:
    case Qt::Key_Down:
        showNext();
        break;
    default:
        QDialog::keyPressEvent(event);
    }
}

void ImageViewer::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        m_dragging = true;
        m_dragStart = event->pos();
        m_offsetAtDragStart = m_offset;
        setCursor(Qt::ClosedHandCursor);
    }
}

void ImageViewer::mouseMoveEvent(QMouseEvent* event) {
    if (m_dragging) {
        QPoint delta = event->pos() - m_dragStart;
        m_offset = m_offsetAtDragStart + QPointF(delta);
        update();
    }
}

void ImageViewer::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        m_dragging = false;
        setCursor(Qt::ArrowCursor);
    }
}

void ImageViewer::mouseDoubleClickEvent(QMouseEvent* event) {
    Q_UNUSED(event)
    // 双击还原缩放
    fitToWindow();
    update();
}

void ImageViewer::wheelEvent(QWheelEvent* event) {
    // 以鼠标位置为中心缩放
    QPointF mousePos = event->position();

    // 鼠标相对于图片中心的位置
    double centerX = width() / 2.0;
    double centerY = 52 + (height() - 52) / 2.0;
    QPointF relativeToCenter = mousePos - QPointF(centerX, centerY) - m_offset;

    // 计算缩放因子
    double scaleFactor = 1.0;
    int delta = event->angleDelta().y();
    if (delta > 0) {
        scaleFactor = 1.15; // 放大
    } else if (delta < 0) {
        scaleFactor = 1.0 / 1.15; // 缩小
    }

    double newScale = qBound(MinScale, m_scale * scaleFactor, MaxScale);
    double actualFactor = newScale / m_scale;
    m_scale = newScale;

    // 调整偏移，使鼠标位置保持不变
    m_offset = mousePos - QPointF(centerX, centerY) - relativeToCenter * actualFactor;

    update();
}

void ImageViewer::resizeEvent(QResizeEvent* event) {
    QDialog::resizeEvent(event);
    updateInfoBar();
    updateNavButtons();

    // 如果有图片，重新适应窗口
    if (!m_currentPixmap.isNull()) {
        fitToWindow();
    }
}

void ImageViewer::showPrevious() {
    if (m_currentIndex > 0) {
        loadImage(m_currentIndex - 1);
    }
}

void ImageViewer::showNext() {
    if (m_currentIndex < m_files.size() - 1) {
        loadImage(m_currentIndex + 1);
    }
}
