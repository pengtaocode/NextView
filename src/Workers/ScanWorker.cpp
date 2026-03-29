#include "ScanWorker.h"
#include "FFmpegHelper.h"

#include <QDirIterator>
#include <QFileInfo>
#include <QImageReader>
#include <QDebug>
#include <QSize>

ScanWorker::ScanWorker(QObject* parent)
    : QThread(parent)
    , m_stopRequested(false)
{
    m_imageExtensions = supportedImageExtensions();
    m_videoExtensions = supportedVideoExtensions();
}

QStringList ScanWorker::supportedImageExtensions() {
    return {
        "jpg", "jpeg", "png", "gif", "bmp",
        "tiff", "tif", "webp", "heic", "heif", "avif"
    };
}

QStringList ScanWorker::supportedVideoExtensions() {
    return {
        "mp4", "mov", "avi", "mkv", "wmv",
        "m4v", "webm", "flv", "ts", "mts"
    };
}

void ScanWorker::setProject(const QString& projectId, const QString& folderPath) {
    m_projectId = projectId;
    m_folderPath = folderPath;
}

void ScanWorker::requestStop() {
    m_stopRequested = true;
}

bool ScanWorker::isImageFile(const QString& extension) const {
    return m_imageExtensions.contains(extension.toLower());
}

bool ScanWorker::isVideoFile(const QString& extension) const {
    return m_videoExtensions.contains(extension.toLower());
}

QSize ScanWorker::getImageSize(const QString& filePath) const {
    // 使用 QImageReader 只读取图片头信息，不完整解码
    QImageReader reader(filePath);
    reader.setAutoDetectImageFormat(true);
    QSize size = reader.size();
    return size;
}

void ScanWorker::run() {
    m_stopRequested = false;

    QDir dir(m_folderPath);
    if (!dir.exists()) {
        emit scanError(QString("文件夹不存在：%1").arg(m_folderPath));
        return;
    }

    qDebug() << "开始扫描文件夹：" << m_folderPath;

    QList<MediaFile> allFiles;
    int foundCount = 0;

    // 递归遍历所有子目录
    QDirIterator it(
        m_folderPath,
        QDir::Files | QDir::Readable,
        QDirIterator::Subdirectories | QDirIterator::FollowSymlinks
    );

    while (it.hasNext() && !m_stopRequested) {
        it.next();
        QFileInfo info = it.fileInfo();
        QString ext = info.suffix().toLower();

        MediaFile::Type fileType;
        bool isMedia = false;

        if (isImageFile(ext)) {
            fileType = MediaFile::Image;
            isMedia = true;
        } else if (isVideoFile(ext)) {
            fileType = MediaFile::Video;
            isMedia = true;
        }

        if (!isMedia) {
            continue;
        }

        // 创建媒体文件对象
        MediaFile file;
        file.path = info.absoluteFilePath();
        file.name = info.fileName();
        file.type = fileType;
        file.size = info.size();

        // 获取图片尺寸（快速，不完整解码）
        if (fileType == MediaFile::Image) {
            file.dimensions = getImageSize(file.path);
        }
        // 视频的尺寸和时长在 CacheWorker 中获取（避免扫描阻塞）

        allFiles.append(file);
        foundCount++;

        // 发出单个文件发现信号
        emit fileFound(file);

        // 每发现10个文件更新一次进度
        if (foundCount % 10 == 0) {
            emit progressUpdated(foundCount);
        }
    }

    if (m_stopRequested) {
        qDebug() << "扫描被取消，已找到" << foundCount << "个文件";
        return;
    }

    // 最终进度更新
    emit progressUpdated(foundCount);

    qDebug() << "扫描完成，共找到" << foundCount << "个媒体文件";
    emit scanComplete(allFiles);
}
