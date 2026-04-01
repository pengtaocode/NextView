#pragma once

#include <QString>
#include <QDateTime>
#include <QJsonObject>
#include <QUuid>
#include <QDir>
#include <QFileInfo>

/**
 * @brief 项目数据模型
 * 以"项目"为单位管理本地文件夹中的图片和视频
 */
struct Project {
    QString id;           // UUID 唯一标识
    QString name;         // 可编辑，默认文件夹名
    QString folderPath;   // 文件夹路径
    QDateTime createdAt;  // 创建时间
    int imageCount = 0;   // 图片数量
    int videoCount = 0;   // 视频数量
    qint64 totalSize = 0; // 所有媒体文件总大小（字节）
    qint64 cacheSize = 0;          // 缓存占用大小（字节）
    QString cardThumbnailPath;     // 项目卡片封面缩略图路径

    // 扫描状态枚举
    enum ScanState {
        NotScanned,  // 未扫描
        Scanning,    // 扫描中
        Scanned      // 已扫描完成
    } scanState = NotScanned;

    int scanFound = 0;    // 扫描中已发现文件数

    // 缓存生成状态枚举
    enum CacheState {
        NoneGenerated,  // 未生成缓存
        Generating,     // 生成中
        Generated       // 已生成完成
    } cacheState = NoneGenerated;

    int cacheProgress = 0;  // 已完成缓存数
    int cacheTotal = 0;     // 总需缓存数（主要是视频数）

    /**
     * @brief 序列化为 JSON 对象
     */
    QJsonObject toJson() const {
        QJsonObject obj;
        obj["id"] = id;
        obj["name"] = name;
        obj["folderPath"] = folderPath;
        obj["createdAt"] = createdAt.toString(Qt::ISODate);
        obj["imageCount"] = imageCount;
        obj["videoCount"] = videoCount;
        obj["totalSize"] = static_cast<qint64>(totalSize);
        obj["cacheSize"] = static_cast<qint64>(cacheSize);
        obj["cardThumbnailPath"] = cardThumbnailPath;
        return obj;
    }

    /**
     * @brief 从 JSON 对象反序列化
     */
    static Project fromJson(const QJsonObject& obj) {
        Project p;
        p.id = obj["id"].toString();
        p.name = obj["name"].toString();
        p.folderPath = obj["folderPath"].toString();
        p.createdAt = QDateTime::fromString(obj["createdAt"].toString(), Qt::ISODate);
        p.imageCount = obj["imageCount"].toInt(0);
        p.videoCount = obj["videoCount"].toInt(0);
        p.totalSize = static_cast<qint64>(obj["totalSize"].toDouble(0));
        p.cacheSize = static_cast<qint64>(obj["cacheSize"].toDouble(0));
        p.cardThumbnailPath = obj["cardThumbnailPath"].toString();
        p.scanState = Scanned; // 已保存的项目视为已扫描
        p.cacheState = NoneGenerated;
        return p;
    }

    /**
     * @brief 创建新项目（自动生成 UUID 和创建时间）
     */
    static Project create(const QString& folderPath) {
        Project p;
        p.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        p.folderPath = folderPath;
        // 默认项目名为文件夹名（用 QDir::dirName 兼容末尾带 / 的 macOS 路径）
        p.name = QDir(folderPath).dirName();
        if (p.name.isEmpty()) {
            p.name = folderPath;
        }
        p.createdAt = QDateTime::currentDateTime();
        p.scanState = NotScanned;
        p.cacheState = NoneGenerated;
        return p;
    }
};

