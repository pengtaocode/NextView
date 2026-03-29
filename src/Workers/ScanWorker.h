#pragma once

#include <QThread>
#include <QList>
#include <QString>
#include <QSize>
#include "MediaFile.h"

/**
 * @brief 文件夹扫描工作线程
 * 递归遍历文件夹，查找所有支持的图片和视频文件
 */
class ScanWorker : public QThread {
    Q_OBJECT

public:
    explicit ScanWorker(QObject* parent = nullptr);

    /**
     * @brief 设置要扫描的项目信息
     * @param projectId 项目ID
     * @param folderPath 文件夹路径
     */
    void setProject(const QString& projectId, const QString& folderPath);

    /**
     * @brief 请求停止扫描
     */
    void requestStop();

    /**
     * @brief 支持的图片扩展名列表
     */
    static QStringList supportedImageExtensions();

    /**
     * @brief 支持的视频扩展名列表
     */
    static QStringList supportedVideoExtensions();

signals:
    /**
     * @brief 每找到一个媒体文件时发出
     */
    void fileFound(const MediaFile& file);

    /**
     * @brief 扫描进度更新
     * @param found 已找到的文件数
     */
    void progressUpdated(int found);

    /**
     * @brief 扫描完成
     * @param allFiles 所有找到的媒体文件列表
     */
    void scanComplete(const QList<MediaFile>& allFiles);

    /**
     * @brief 扫描出错
     */
    void scanError(const QString& errorMessage);

protected:
    /**
     * @brief 线程主函数
     */
    void run() override;

private:
    /**
     * @brief 判断文件是否为支持的图片格式
     */
    bool isImageFile(const QString& extension) const;

    /**
     * @brief 判断文件是否为支持的视频格式
     */
    bool isVideoFile(const QString& extension) const;

    /**
     * @brief 获取图片尺寸（不完整解码）
     */
    QSize getImageSize(const QString& filePath) const;

    QString m_projectId;    // 项目ID
    QString m_folderPath;   // 扫描目录
    bool m_stopRequested;   // 停止标志

    // 支持的扩展名集合（小写）
    QStringList m_imageExtensions;
    QStringList m_videoExtensions;
};
