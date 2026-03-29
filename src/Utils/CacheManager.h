#pragma once

#include <QString>
#include <QObject>

/**
 * @brief 缓存管理器
 * 负责缓存文件路径的计算、缓存大小统计和缓存清理
 */
class CacheManager : public QObject {
    Q_OBJECT

public:
    /**
     * @brief 获取单例实例
     */
    static CacheManager* instance();

    /**
     * @brief 获取缓存根目录路径
     */
    static QString cacheRootDir();

    /**
     * @brief 获取指定项目的缓存目录
     */
    static QString projectCacheDir(const QString& projectId);

    /**
     * @brief 获取指定文件的缩略图缓存路径
     * @param projectId 项目ID
     * @param filePath 媒体文件完整路径
     * @return 缩略图 JPG 文件路径
     */
    static QString thumbnailPath(const QString& projectId, const QString& filePath);

    /**
     * @brief 获取指定视频文件的预览视频缓存路径
     * @param projectId 项目ID
     * @param filePath 视频文件完整路径
     * @return 预览 MP4 文件路径
     */
    static QString previewPath(const QString& projectId, const QString& filePath);

    /**
     * @brief 删除指定项目的所有缓存
     */
    static bool deleteProjectCache(const QString& projectId);

    /**
     * @brief 删除所有项目缓存
     */
    static bool deleteAllCache();

    /**
     * @brief 计算指定项目的缓存大小（字节）
     */
    static qint64 calculateCacheSize(const QString& projectId);

    /**
     * @brief 计算所有缓存总大小（字节）
     */
    static qint64 calculateTotalCacheSize();

    /**
     * @brief 确保项目缓存目录存在
     */
    static bool ensureProjectCacheDir(const QString& projectId);

private:
    explicit CacheManager(QObject* parent = nullptr) : QObject(parent) {}

    /**
     * @brief 使用 MD5 哈希对文件路径生成安全的文件名
     */
    static QString hashFilePath(const QString& filePath);

    /**
     * @brief 递归计算目录大小
     */
    static qint64 dirSize(const QString& dirPath);
};
