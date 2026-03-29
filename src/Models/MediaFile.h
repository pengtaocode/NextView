#pragma once

#include <QString>
#include <QSize>
#include <QFile>
#include <QMetaType>

/**
 * @brief 媒体文件数据模型
 * 表示单个图片或视频文件的信息
 */
struct MediaFile {
    QString path;           // 文件完整路径
    QString name;           // 文件名（不含路径）

    // 媒体类型枚举
    enum Type {
        Image,  // 图片
        Video   // 视频
    } type;

    qint64 size = 0;          // 文件大小（字节）
    QSize dimensions;          // 图片/视频尺寸（宽x高）
    double duration = 0.0;     // 视频时长（秒），图片为0
    QString thumbnailPath;     // 缩略图路径（已生成则非空）
    QString previewPath;       // 视频预览缓存路径（已生成则非空）

    /**
     * @brief 检查缩略图是否已存在
     */
    bool hasThumbnail() const {
        return !thumbnailPath.isEmpty() && QFile::exists(thumbnailPath);
    }

    /**
     * @brief 检查视频预览是否已存在
     */
    bool hasPreview() const {
        return !previewPath.isEmpty() && QFile::exists(previewPath);
    }

    /**
     * @brief 格式化视频时长为 "分:秒" 字符串
     */
    QString formattedDuration() const {
        if (type != Video || duration <= 0.0) return QString();
        int totalSec = static_cast<int>(duration);
        int minutes = totalSec / 60;
        int seconds = totalSec % 60;
        return QString("%1:%2").arg(minutes).arg(seconds, 2, 10, QChar('0'));
    }

    /**
     * @brief 格式化文件大小为人类可读字符串
     */
    QString formattedSize() const {
        if (size < 1024) return QString("%1 B").arg(size);
        if (size < 1024 * 1024) return QString("%1 KB").arg(size / 1024);
        if (size < 1024 * 1024 * 1024)
            return QString("%1 MB").arg(size / (1024.0 * 1024.0), 0, 'f', 1);
        return QString("%1 GB").arg(size / (1024.0 * 1024.0 * 1024.0), 0, 'f', 2);
    }
};

// 注册自定义类型以支持跨线程信号传递（仅声明一次）
Q_DECLARE_METATYPE(MediaFile)
Q_DECLARE_METATYPE(QList<MediaFile>)
